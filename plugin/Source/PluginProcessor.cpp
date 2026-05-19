#include "PluginProcessor.h"
#ifndef MIX_ANALYZER_HEADLESS_TESTS
#include "PluginEditor.h"
#endif

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
constexpr double lufsOffset = -0.691;
constexpr double absoluteGateLufs = -70.0;
constexpr double minimumPower = 1.0e-20;
constexpr double momentarySeconds = 0.4;
constexpr double shortTermSeconds = 3.0;
constexpr double integratedHopSeconds = 0.1;
constexpr float kWeightPreFilterHz = 1681.97445f;
constexpr float kWeightPreFilterQ = 0.70717526f;
constexpr float kWeightPreFilterGainDb = 3.9998438f;
constexpr float kWeightHighPassHz = 38.13547f;
constexpr float kWeightHighPassQ = 0.50032705f;
constexpr float autoMasterDefaultCeilingDbTp = -1.0f;
constexpr float autoMasterMaxBoostDb = 6.0f;
constexpr float autoMasterMaxCutDb = -8.0f;
constexpr float autoMasterMaxLowShelfBoostDb = 1.5f;
constexpr float autoMasterMaxLowShelfCutDb = -2.0f;
constexpr float autoMasterMaxPresenceBoostDb = 0.8f;
constexpr float autoMasterMaxPresenceCutDb = -2.0f;
constexpr float autoMasterMaxAirBoostDb = 0.8f;
constexpr float autoMasterMaxGlueReductionDb = 2.5f;

constexpr std::array<std::pair<float, float>, fmma::bandCount> bandRanges {{
    {20.0f, 60.0f},
    {60.0f, 250.0f},
    {250.0f, 500.0f},
    {500.0f, 2000.0f},
    {2000.0f, 6000.0f},
    {6000.0f, 20000.0f},
}};

const juce::Identifier storedAnalysisStateId { "StoredAnalysisState" };
const juce::Identifier referenceMetricsId { "ReferenceMetrics" };
const juce::Identifier snapshotAId { "SnapshotA" };
const juce::Identifier snapshotBId { "SnapshotB" };

float toDb(float value) noexcept
{
    return juce::Decibels::gainToDecibels(juce::jmax(value, 1.0e-6f), -120.0f);
}

float powerToLufs(double meanPower) noexcept
{
    if (! std::isfinite(meanPower) || meanPower <= minimumPower)
        return -120.0f;

    return static_cast<float>(lufsOffset + (10.0 * std::log10(meanPower)));
}

float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float smoothed(float current, float target, float smoothing) noexcept
{
    target = finiteOr(target, current);
    return current + ((target - current) * smoothing);
}

float smoothingForWindow(float seconds, float windowSeconds) noexcept
{
    if (seconds <= 0.0f || windowSeconds <= 0.0f)
        return 1.0f;

    return juce::jlimit(0.0f, 1.0f, 1.0f - std::exp(-seconds / windowSeconds));
}

float decibelsToGain(float decibels) noexcept
{
    return juce::Decibels::decibelsToGain(decibels);
}

float gainToDecibels(float gain) noexcept
{
    return juce::Decibels::gainToDecibels(juce::jmax(gain, 1.0e-6f), -120.0f);
}

float audioEvidenceTrust(float seconds) noexcept
{
    if (! std::isfinite(seconds))
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, (seconds - 2.0f) / 18.0f);
}

float distanceOutsideRange(float value, const fmma::Range& range) noexcept
{
    if (! std::isfinite(value))
        return 0.0f;

    if (value < range.low)
        return range.low - value;

    if (value > range.high)
        return value - range.high;

    return 0.0f;
}

void updateMaximum(std::atomic<float>& target, float value) noexcept
{
    if (! std::isfinite(value))
        return;

    const auto current = target.load(std::memory_order_relaxed);
    if (value > current)
        target.store(value, std::memory_order_relaxed);
}

void updateMinimum(std::atomic<float>& target, float value) noexcept
{
    if (! std::isfinite(value))
        return;

    const auto current = target.load(std::memory_order_relaxed);
    if (value < current)
        target.store(value, std::memory_order_relaxed);
}

float peakOf(const float* data, int numSamples) noexcept
{
    auto peak = 0.0f;
    for (auto i = 0; i < numSamples; ++i)
        peak = juce::jmax(peak, std::abs(data[i]));
    return peak;
}

void setFloatProperty(juce::ValueTree& tree, const char* name, float value)
{
    tree.setProperty(juce::Identifier { name }, value, nullptr);
}

void setBoolProperty(juce::ValueTree& tree, const char* name, bool value)
{
    tree.setProperty(juce::Identifier { name }, value, nullptr);
}

float getFloatProperty(const juce::ValueTree& tree, const char* name, float fallback)
{
    return static_cast<float>(static_cast<double>(tree.getProperty(juce::Identifier { name }, fallback)));
}

bool getBoolProperty(const juce::ValueTree& tree, const char* name, bool fallback)
{
    return static_cast<bool>(tree.getProperty(juce::Identifier { name }, fallback));
}

juce::ValueTree metricsToValueTree(const juce::Identifier& type, const fmma::AnalyzerMetrics& metrics)
{
    juce::ValueTree tree { type };
    setFloatProperty(tree, "momentaryLufs", metrics.momentaryLufs);
    setFloatProperty(tree, "shortTermLufs", metrics.shortTermLufs);
    setFloatProperty(tree, "integratedLufs", metrics.integratedLufs);
    setFloatProperty(tree, "truePeakDb", metrics.truePeakDb);
    setFloatProperty(tree, "truePeakHoldDb", metrics.truePeakHoldDb);
    setFloatProperty(tree, "worstTruePeakDb", metrics.worstTruePeakDb);
    setFloatProperty(tree, "lraLu", metrics.lraLu);
    setFloatProperty(tree, "rmsDb", metrics.rmsDb);
    setFloatProperty(tree, "peakDb", metrics.peakDb);
    setFloatProperty(tree, "crestDb", metrics.crestDb);
    setFloatProperty(tree, "leftPeakDb", metrics.leftPeakDb);
    setFloatProperty(tree, "rightPeakDb", metrics.rightPeakDb);
    setFloatProperty(tree, "monoPeakDb", metrics.monoPeakDb);
    setFloatProperty(tree, "monoRmsDb", metrics.monoRmsDb);
    setFloatProperty(tree, "monoLossDb", metrics.monoLossDb);
    setFloatProperty(tree, "correlation", metrics.correlation);
    setFloatProperty(tree, "worstCorrelation", metrics.worstCorrelation);
    setFloatProperty(tree, "worstMonoLossDb", metrics.worstMonoLossDb);
    setFloatProperty(tree, "widthPct", metrics.widthPct);
    setFloatProperty(tree, "msRatioDb", metrics.msRatioDb);
    setFloatProperty(tree, "stereoBalanceDb", metrics.stereoBalanceDb);
    setFloatProperty(tree, "dcOffset", metrics.dcOffset);
    setFloatProperty(tree, "clippedPercent", metrics.clippedPercent);
    setFloatProperty(tree, "worstClippedPercent", metrics.worstClippedPercent);
    setFloatProperty(tree, "silencePercent", metrics.silencePercent);
    setFloatProperty(tree, "transientDensity", metrics.transientDensity);
    setFloatProperty(tree, "attackTimeMs", metrics.attackTimeMs);
    setFloatProperty(tree, "percussionEnergyPct", metrics.percussionEnergyPct);
    setFloatProperty(tree, "spectralCentroidHz", metrics.spectralCentroidHz);
    setFloatProperty(tree, "spectralRolloffHz", metrics.spectralRolloffHz);
    setFloatProperty(tree, "resonanceFreqHz", metrics.resonanceFreqHz);
    setFloatProperty(tree, "resonanceGainDb", metrics.resonanceGainDb);
    setFloatProperty(tree, "worstResonanceFreqHz", metrics.worstResonanceFreqHz);
    setFloatProperty(tree, "worstResonanceGainDb", metrics.worstResonanceGainDb);
    setFloatProperty(tree, "worstLowMidPercent", metrics.worstLowMidPercent);
    setFloatProperty(tree, "phaseCorrelation", metrics.phaseCorrelation);
    setFloatProperty(tree, "analysisSeconds", metrics.analysisSeconds);
    setFloatProperty(tree, "fullPassSeconds", metrics.fullPassSeconds);
    setBoolProperty(tree, "fullPassActive", metrics.fullPassActive);
    setBoolProperty(tree, "fullPassCompleted", metrics.fullPassCompleted);
    setBoolProperty(tree, "analysisFrozen", metrics.analysisFrozen);

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        setFloatProperty(tree,
                         ("band" + juce::String(band)).toRawUTF8(),
                         metrics.bandPercents[static_cast<size_t>(band)]);
        setFloatProperty(tree,
                         ("bandCorr" + juce::String(band)).toRawUTF8(),
                         metrics.bandCorrelations[static_cast<size_t>(band)]);
        setFloatProperty(tree,
                         ("bandSide" + juce::String(band)).toRawUTF8(),
                         metrics.bandSideRatiosDb[static_cast<size_t>(band)]);
    }

    return tree;
}

fmma::AnalyzerMetrics valueTreeToMetrics(const juce::ValueTree& tree)
{
    fmma::AnalyzerMetrics metrics;
    metrics.momentaryLufs = getFloatProperty(tree, "momentaryLufs", metrics.momentaryLufs);
    metrics.shortTermLufs = getFloatProperty(tree, "shortTermLufs", metrics.shortTermLufs);
    metrics.integratedLufs = getFloatProperty(tree, "integratedLufs", metrics.integratedLufs);
    metrics.truePeakDb = getFloatProperty(tree, "truePeakDb", metrics.truePeakDb);
    metrics.truePeakHoldDb = getFloatProperty(tree, "truePeakHoldDb", metrics.truePeakHoldDb);
    metrics.worstTruePeakDb = getFloatProperty(tree, "worstTruePeakDb", metrics.worstTruePeakDb);
    metrics.lraLu = getFloatProperty(tree, "lraLu", metrics.lraLu);
    metrics.rmsDb = getFloatProperty(tree, "rmsDb", metrics.rmsDb);
    metrics.peakDb = getFloatProperty(tree, "peakDb", metrics.peakDb);
    metrics.crestDb = getFloatProperty(tree, "crestDb", metrics.crestDb);
    metrics.leftPeakDb = getFloatProperty(tree, "leftPeakDb", metrics.leftPeakDb);
    metrics.rightPeakDb = getFloatProperty(tree, "rightPeakDb", metrics.rightPeakDb);
    metrics.monoPeakDb = getFloatProperty(tree, "monoPeakDb", metrics.monoPeakDb);
    metrics.monoRmsDb = getFloatProperty(tree, "monoRmsDb", metrics.monoRmsDb);
    metrics.monoLossDb = getFloatProperty(tree, "monoLossDb", metrics.monoLossDb);
    metrics.correlation = getFloatProperty(tree, "correlation", metrics.correlation);
    metrics.worstCorrelation = getFloatProperty(tree, "worstCorrelation", metrics.worstCorrelation);
    metrics.worstMonoLossDb = getFloatProperty(tree, "worstMonoLossDb", metrics.worstMonoLossDb);
    metrics.widthPct = getFloatProperty(tree, "widthPct", metrics.widthPct);
    metrics.msRatioDb = getFloatProperty(tree, "msRatioDb", metrics.msRatioDb);
    metrics.stereoBalanceDb = getFloatProperty(tree, "stereoBalanceDb", metrics.stereoBalanceDb);
    metrics.dcOffset = getFloatProperty(tree, "dcOffset", metrics.dcOffset);
    metrics.clippedPercent = getFloatProperty(tree, "clippedPercent", metrics.clippedPercent);
    metrics.worstClippedPercent = getFloatProperty(tree, "worstClippedPercent", metrics.worstClippedPercent);
    metrics.silencePercent = getFloatProperty(tree, "silencePercent", metrics.silencePercent);
    metrics.transientDensity = getFloatProperty(tree, "transientDensity", metrics.transientDensity);
    metrics.attackTimeMs = getFloatProperty(tree, "attackTimeMs", metrics.attackTimeMs);
    metrics.percussionEnergyPct = getFloatProperty(tree, "percussionEnergyPct", metrics.percussionEnergyPct);
    metrics.spectralCentroidHz = getFloatProperty(tree, "spectralCentroidHz", metrics.spectralCentroidHz);
    metrics.spectralRolloffHz = getFloatProperty(tree, "spectralRolloffHz", metrics.spectralRolloffHz);
    metrics.resonanceFreqHz = getFloatProperty(tree, "resonanceFreqHz", metrics.resonanceFreqHz);
    metrics.resonanceGainDb = getFloatProperty(tree, "resonanceGainDb", metrics.resonanceGainDb);
    metrics.worstResonanceFreqHz = getFloatProperty(tree, "worstResonanceFreqHz", metrics.worstResonanceFreqHz);
    metrics.worstResonanceGainDb = getFloatProperty(tree, "worstResonanceGainDb", metrics.worstResonanceGainDb);
    metrics.analysisSeconds = getFloatProperty(tree, "analysisSeconds", metrics.analysisSeconds);
    metrics.fullPassSeconds = getFloatProperty(tree, "fullPassSeconds", metrics.fullPassSeconds);
    metrics.fullPassActive = false;
    metrics.fullPassCompleted = getBoolProperty(tree, "fullPassCompleted", metrics.fullPassCompleted);
    metrics.analysisFrozen = getBoolProperty(tree, "analysisFrozen", metrics.analysisFrozen);

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        metrics.bandPercents[static_cast<size_t>(band)] =
            getFloatProperty(tree,
                             ("band" + juce::String(band)).toRawUTF8(),
                             metrics.bandPercents[static_cast<size_t>(band)]);
        metrics.bandCorrelations[static_cast<size_t>(band)] =
            getFloatProperty(tree,
                             ("bandCorr" + juce::String(band)).toRawUTF8(),
                             metrics.bandCorrelations[static_cast<size_t>(band)]);
        metrics.bandSideRatiosDb[static_cast<size_t>(band)] =
            getFloatProperty(tree,
                             ("bandSide" + juce::String(band)).toRawUTF8(),
                             metrics.bandSideRatiosDb[static_cast<size_t>(band)]);
    }

    metrics.worstLowMidPercent = getFloatProperty(tree, "worstLowMidPercent", metrics.worstLowMidPercent);
    metrics.phaseCorrelation = getFloatProperty(tree, "phaseCorrelation", metrics.phaseCorrelation);

    return metrics;
}
}

void FunkyMooseMixAnalyzerAudioProcessor::AutoMasterBiquad::reset() noexcept
{
    z1 = 0.0f;
    z2 = 0.0f;
}

void FunkyMooseMixAnalyzerAudioProcessor::AutoMasterBiquad::setIdentity() noexcept
{
    b0 = 1.0f;
    b1 = 0.0f;
    b2 = 0.0f;
    a1 = 0.0f;
    a2 = 0.0f;
}

void FunkyMooseMixAnalyzerAudioProcessor::AutoMasterBiquad::setLowShelf(double sampleRate,
                                                                        float frequencyHz,
                                                                        float gainDb) noexcept
{
    if (std::abs(gainDb) < 0.02f || sampleRate <= 0.0)
    {
        setIdentity();
        return;
    }

    const auto freq = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45), frequencyHz);
    const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const auto omega = juce::MathConstants<double>::twoPi * static_cast<double>(freq) / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto sqrtA = std::sqrt(a);
    const auto alpha = sinOmega * std::sqrt(2.0) * 0.5;

    const auto rawB0 = a * ((a + 1.0) - ((a - 1.0) * cosOmega) + (2.0 * sqrtA * alpha));
    const auto rawB1 = 2.0 * a * ((a - 1.0) - ((a + 1.0) * cosOmega));
    const auto rawB2 = a * ((a + 1.0) - ((a - 1.0) * cosOmega) - (2.0 * sqrtA * alpha));
    const auto rawA0 = (a + 1.0) + ((a - 1.0) * cosOmega) + (2.0 * sqrtA * alpha);
    const auto rawA1 = -2.0 * ((a - 1.0) + ((a + 1.0) * cosOmega));
    const auto rawA2 = (a + 1.0) + ((a - 1.0) * cosOmega) - (2.0 * sqrtA * alpha);

    b0 = static_cast<float>(rawB0 / rawA0);
    b1 = static_cast<float>(rawB1 / rawA0);
    b2 = static_cast<float>(rawB2 / rawA0);
    a1 = static_cast<float>(rawA1 / rawA0);
    a2 = static_cast<float>(rawA2 / rawA0);
}

void FunkyMooseMixAnalyzerAudioProcessor::AutoMasterBiquad::setHighShelf(double sampleRate,
                                                                         float frequencyHz,
                                                                         float gainDb) noexcept
{
    if (std::abs(gainDb) < 0.02f || sampleRate <= 0.0)
    {
        setIdentity();
        return;
    }

    const auto freq = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45), frequencyHz);
    const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const auto omega = juce::MathConstants<double>::twoPi * static_cast<double>(freq) / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto sqrtA = std::sqrt(a);
    const auto alpha = sinOmega * std::sqrt(2.0) * 0.5;

    const auto rawB0 = a * ((a + 1.0) + ((a - 1.0) * cosOmega) + (2.0 * sqrtA * alpha));
    const auto rawB1 = -2.0 * a * ((a - 1.0) + ((a + 1.0) * cosOmega));
    const auto rawB2 = a * ((a + 1.0) + ((a - 1.0) * cosOmega) - (2.0 * sqrtA * alpha));
    const auto rawA0 = (a + 1.0) - ((a - 1.0) * cosOmega) + (2.0 * sqrtA * alpha);
    const auto rawA1 = 2.0 * ((a - 1.0) - ((a + 1.0) * cosOmega));
    const auto rawA2 = (a + 1.0) - ((a - 1.0) * cosOmega) - (2.0 * sqrtA * alpha);

    b0 = static_cast<float>(rawB0 / rawA0);
    b1 = static_cast<float>(rawB1 / rawA0);
    b2 = static_cast<float>(rawB2 / rawA0);
    a1 = static_cast<float>(rawA1 / rawA0);
    a2 = static_cast<float>(rawA2 / rawA0);
}

void FunkyMooseMixAnalyzerAudioProcessor::AutoMasterBiquad::setPeak(double sampleRate,
                                                                    float frequencyHz,
                                                                    float q,
                                                                    float gainDb) noexcept
{
    if (std::abs(gainDb) < 0.02f || sampleRate <= 0.0)
    {
        setIdentity();
        return;
    }

    const auto freq = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45), frequencyHz);
    const auto safeQ = juce::jlimit(0.1f, 12.0f, q);
    const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
    const auto omega = juce::MathConstants<double>::twoPi * static_cast<double>(freq) / sampleRate;
    const auto alpha = std::sin(omega) / (2.0 * static_cast<double>(safeQ));
    const auto cosOmega = std::cos(omega);

    const auto rawB0 = 1.0 + (alpha * a);
    const auto rawB1 = -2.0 * cosOmega;
    const auto rawB2 = 1.0 - (alpha * a);
    const auto rawA0 = 1.0 + (alpha / a);
    const auto rawA1 = -2.0 * cosOmega;
    const auto rawA2 = 1.0 - (alpha / a);

    b0 = static_cast<float>(rawB0 / rawA0);
    b1 = static_cast<float>(rawB1 / rawA0);
    b2 = static_cast<float>(rawB2 / rawA0);
    a1 = static_cast<float>(rawA1 / rawA0);
    a2 = static_cast<float>(rawA2 / rawA0);
}

float FunkyMooseMixAnalyzerAudioProcessor::AutoMasterBiquad::process(float sample) noexcept
{
    const auto output = (b0 * sample) + z1;
    z1 = (b1 * sample) - (a1 * output) + z2;
    z2 = (b2 * sample) - (a2 * output);
    return output;
}

FunkyMooseMixAnalyzerAudioProcessor::FunkyMooseMixAnalyzerAudioProcessor()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    for (auto& band : bandPercents)
        band.store(0.0f, std::memory_order_relaxed);
    for (auto& band : bandCorrelations)
        band.store(1.0f, std::memory_order_relaxed);
    for (auto& band : bandSideRatiosDb)
        band.store(-120.0f, std::memory_order_relaxed);

#ifndef MIX_ANALYZER_HEADLESS_TESTS
    startTimerHz(6);
#endif
}

juce::AudioProcessorValueTreeState::ParameterLayout
FunkyMooseMixAnalyzerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID {"genre", 1},
        "Genre Profile",
        fmma::getGenreNames(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID {"instrumental", 1},
        "Instrumental",
        false));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID {"autoMasterEnabled", 1},
        "Auto Master",
        false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID {"autoMasterStrength", 1},
        "Auto Master Strength",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 1.0f },
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    return { params.begin(), params.end() };
}

void FunkyMooseMixAnalyzerAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    currentSampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    configureBandFilters(currentSampleRate, samplesPerBlock);
    configureLoudnessMeter(currentSampleRate, samplesPerBlock);
    resetAutoMasterState();
}

void FunkyMooseMixAnalyzerAudioProcessor::releaseResources()
{
    for (auto& filter : leftBandFilters)
        filter.reset();
    for (auto& filter : rightBandFilters)
        filter.reset();
    for (auto& filter : kWeightPreFilters)
        filter.reset();
    for (auto& filter : kWeightHighPassFilters)
        filter.reset();

    if (truePeakOversampler != nullptr)
        truePeakOversampler->reset();

    resetAutoMasterState();
}

bool FunkyMooseMixAnalyzerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainOut == layouts.getMainInputChannelSet();
}

void FunkyMooseMixAnalyzerAudioProcessor::requestAnalyzerReset() noexcept
{
    resetRequested.store(true, std::memory_order_release);
}

void FunkyMooseMixAnalyzerAudioProcessor::requestFullPassStart() noexcept
{
    fullPassStartRequested.store(true, std::memory_order_release);
}

void FunkyMooseMixAnalyzerAudioProcessor::requestFullPassFinish() noexcept
{
    fullPassFinishRequested.store(true, std::memory_order_release);
}

void FunkyMooseMixAnalyzerAudioProcessor::configureBandFilters(double newSampleRate, int samplesPerBlock)
{
    const auto processBlockSize = static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock));
    juce::dsp::ProcessSpec spec { newSampleRate, processBlockSize, 1 };

    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        const auto [low, high] = bandRanges[static_cast<size_t>(i)];
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;

        if (i == fmma::bandCount - 1)
        {
            coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(newSampleRate, low);
        }
        else
        {
            const auto centre = std::sqrt(low * high);
            const auto q = juce::jlimit(0.45f, 2.0f, centre / (high - low));
            coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass(newSampleRate, centre, q);
        }

        leftBandFilters[static_cast<size_t>(i)].prepare(spec);
        rightBandFilters[static_cast<size_t>(i)].prepare(spec);
        leftBandFilters[static_cast<size_t>(i)].coefficients = coefficients;
        rightBandFilters[static_cast<size_t>(i)].coefficients = coefficients;
        leftBandFilters[static_cast<size_t>(i)].reset();
        rightBandFilters[static_cast<size_t>(i)].reset();
    }
}

void FunkyMooseMixAnalyzerAudioProcessor::configureLoudnessMeter(double newSampleRate, int samplesPerBlock)
{
    const auto processBlockSize = static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock));
    juce::dsp::ProcessSpec monoSpec { newSampleRate, processBlockSize, 1 };

    const auto preFilterCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        newSampleRate,
        kWeightPreFilterHz,
        kWeightPreFilterQ,
        juce::Decibels::decibelsToGain(kWeightPreFilterGainDb));

    const auto highPassCoefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        newSampleRate,
        kWeightHighPassHz,
        kWeightHighPassQ);

    for (auto i = 0; i < 2; ++i)
    {
        kWeightPreFilters[static_cast<size_t>(i)].prepare(monoSpec);
        kWeightHighPassFilters[static_cast<size_t>(i)].prepare(monoSpec);
        kWeightPreFilters[static_cast<size_t>(i)].coefficients = preFilterCoefficients;
        kWeightHighPassFilters[static_cast<size_t>(i)].coefficients = highPassCoefficients;
        kWeightPreFilters[static_cast<size_t>(i)].reset();
        kWeightHighPassFilters[static_cast<size_t>(i)].reset();
    }

    const auto momentarySamples = static_cast<size_t>(juce::jmax(1.0, std::round(newSampleRate * momentarySeconds)));
    const auto shortTermSamples = static_cast<size_t>(juce::jmax(1.0, std::round(newSampleRate * shortTermSeconds)));
    momentaryPowerRing.assign(momentarySamples, 0.0);
    shortTermPowerRing.assign(shortTermSamples, 0.0);
    momentaryPowerIndex = 0;
    shortTermPowerIndex = 0;
    momentaryPowerFilled = 0;
    shortTermPowerFilled = 0;
    momentaryPowerSum = 0.0;
    shortTermPowerSum = 0.0;

    integratedHopSamples = static_cast<int>(juce::jmax(1.0, std::round(newSampleRate * integratedHopSeconds)));
    integratedSamplesUntilHop = integratedHopSamples;
    integratedLoudnessDirty = false;
    integratedBlockPowers.fill(0.0f);
    integratedBlockCount = 0;
    integratedBlockWriteIndex = 0;
    lraBlockPowers.fill(0.0f);
    lraBlockBins.fill(0);
    lraHistogram.fill(0);
    lraBlockCount = 0;
    lraBlockWriteIndex = 0;
    lraPowerSum = 0.0;

    truePeakChannelCount = juce::jlimit(1, 2, getTotalNumInputChannels());
    truePeakOversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(truePeakChannelCount),
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        false,
        true);
    truePeakOversampler->initProcessing(processBlockSize);
    truePeakOversampler->reset();

    resetMeterState();
}

void FunkyMooseMixAnalyzerAudioProcessor::resetMeterState()
{
    std::fill(momentaryPowerRing.begin(), momentaryPowerRing.end(), 0.0);
    std::fill(shortTermPowerRing.begin(), shortTermPowerRing.end(), 0.0);
    momentaryPowerIndex = 0;
    shortTermPowerIndex = 0;
    momentaryPowerFilled = 0;
    shortTermPowerFilled = 0;
    momentaryPowerSum = 0.0;
    shortTermPowerSum = 0.0;
    integratedSamplesUntilHop = integratedHopSamples;
    integratedLoudnessDirty = false;
    integratedBlockPowers.fill(0.0f);
    integratedBlockCount = 0;
    integratedBlockWriteIndex = 0;
    lraBlockPowers.fill(0.0f);
    lraBlockBins.fill(0);
    lraHistogram.fill(0);
    lraBlockCount = 0;
    lraBlockWriteIndex = 0;
    lraPowerSum = 0.0;
    processedSamplesSinceReset = 0;

    for (auto& filter : leftBandFilters)
        filter.reset();
    for (auto& filter : rightBandFilters)
        filter.reset();
    for (auto& filter : kWeightPreFilters)
        filter.reset();
    for (auto& filter : kWeightHighPassFilters)
        filter.reset();
    if (truePeakOversampler != nullptr)
        truePeakOversampler->reset();
    transientFastEnvelope = 0.0f;
    transientSlowEnvelope = 0.0f;
    transientCooldownSamples = 0;
    rollingTransientDensity = 0.0f;
    rollingAttackTimeMs = 0.0f;
    rollingPercussionEnergyPct = 0.0f;
    spectrumFifo.fill(0.0f);
    spectrumData.fill(0.0f);
    spectrumFifoIndex = 0;
    phaseFifoLeft.fill(0.0f);
    phaseFifoRight.fill(0.0f);
    phaseDataLeft.fill(0.0f);
    phaseDataRight.fill(0.0f);
    phaseFifoIndex = 0;

    momentaryLufs.store(-120.0f, std::memory_order_relaxed);
    shortTermLufs.store(-120.0f, std::memory_order_relaxed);
    integratedLufs.store(-120.0f, std::memory_order_relaxed);
    truePeakDb.store(-120.0f, std::memory_order_relaxed);
    truePeakHoldDb.store(-120.0f, std::memory_order_relaxed);
    worstTruePeakDb.store(-120.0f, std::memory_order_relaxed);
    lraLu.store(0.0f, std::memory_order_relaxed);
    rmsDb.store(-120.0f, std::memory_order_relaxed);
    peakDb.store(-120.0f, std::memory_order_relaxed);
    crestDb.store(0.0f, std::memory_order_relaxed);
    leftPeakDb.store(-120.0f, std::memory_order_relaxed);
    rightPeakDb.store(-120.0f, std::memory_order_relaxed);
    monoPeakDb.store(-120.0f, std::memory_order_relaxed);
    monoRmsDb.store(-120.0f, std::memory_order_relaxed);
    monoLossDb.store(0.0f, std::memory_order_relaxed);
    correlation.store(1.0f, std::memory_order_relaxed);
    worstCorrelation.store(1.0f, std::memory_order_relaxed);
    worstMonoLossDb.store(0.0f, std::memory_order_relaxed);
    widthPct.store(0.0f, std::memory_order_relaxed);
    msRatioDb.store(0.0f, std::memory_order_relaxed);
    stereoBalanceDb.store(0.0f, std::memory_order_relaxed);
    dcOffset.store(0.0f, std::memory_order_relaxed);
    clippedPercent.store(0.0f, std::memory_order_relaxed);
    worstClippedPercent.store(0.0f, std::memory_order_relaxed);
    silencePercent.store(0.0f, std::memory_order_relaxed);
    transientDensity.store(0.0f, std::memory_order_relaxed);
    attackTimeMs.store(0.0f, std::memory_order_relaxed);
    percussionEnergyPct.store(0.0f, std::memory_order_relaxed);
    spectralCentroidHz.store(0.0f, std::memory_order_relaxed);
    spectralRolloffHz.store(0.0f, std::memory_order_relaxed);
    resonanceFreqHz.store(0.0f, std::memory_order_relaxed);
    resonanceGainDb.store(0.0f, std::memory_order_relaxed);
    worstResonanceFreqHz.store(0.0f, std::memory_order_relaxed);
    worstResonanceGainDb.store(0.0f, std::memory_order_relaxed);
    worstLowMidPercent.store(0.0f, std::memory_order_relaxed);
    phaseCorrelation.store(1.0f, std::memory_order_relaxed);
    analysisSeconds.store(0.0f, std::memory_order_relaxed);
    fullPassSeconds.store(0.0f, std::memory_order_relaxed);
    fullPassActive.store(false, std::memory_order_relaxed);
    fullPassCompleted.store(false, std::memory_order_relaxed);
    analysisFrozen.store(false, std::memory_order_relaxed);
    hostAutoPassActive.store(false, std::memory_order_relaxed);

    for (auto& band : bandPercents)
        band.store(0.0f, std::memory_order_relaxed);
    for (auto& band : bandCorrelations)
        band.store(1.0f, std::memory_order_relaxed);
    for (auto& band : bandSideRatiosDb)
        band.store(-120.0f, std::memory_order_relaxed);

    resetAutoMasterState();
}

void FunkyMooseMixAnalyzerAudioProcessor::resetAutoMasterState() noexcept
{
    smoothedAutoMasterGainDb = 0.0f;
    smoothedAutoMasterLowShelfDb = 0.0f;
    smoothedAutoMasterPresenceDb = 0.0f;
    smoothedAutoMasterAirShelfDb = 0.0f;
    smoothedAutoMasterSideGain = 1.0f;
    smoothedAutoMasterGlueDepthDb = 0.0f;
    autoMasterGlueGain = 1.0f;
    autoMasterLimiterGain = 1.0f;
    displayedAutoMasterGlueReductionDb = 0.0f;
    displayedAutoMasterLimiterReductionDb = 0.0f;

    for (auto& filter : autoMasterLowShelfFilters)
    {
        filter.setIdentity();
        filter.reset();
    }
    for (auto& filter : autoMasterPresenceFilters)
    {
        filter.setIdentity();
        filter.reset();
    }
    for (auto& filter : autoMasterAirShelfFilters)
    {
        filter.setIdentity();
        filter.reset();
    }

    autoMasterStrengthPct.store(0.0f, std::memory_order_relaxed);
    autoMasterTargetLufs.store(-14.0f, std::memory_order_relaxed);
    autoMasterCeilingDbTp.store(autoMasterDefaultCeilingDbTp, std::memory_order_relaxed);
    autoMasterGainDb.store(0.0f, std::memory_order_relaxed);
    autoMasterLowShelfDb.store(0.0f, std::memory_order_relaxed);
    autoMasterPresenceDb.store(0.0f, std::memory_order_relaxed);
    autoMasterAirShelfDb.store(0.0f, std::memory_order_relaxed);
    autoMasterWidthPercent.store(100.0f, std::memory_order_relaxed);
    autoMasterGlueReductionDb.store(0.0f, std::memory_order_relaxed);
    autoMasterLimiterReductionDb.store(0.0f, std::memory_order_relaxed);
    autoMasterProjectedLufs.store(-120.0f, std::memory_order_relaxed);
    autoMasterProjectedTruePeakDbTp.store(-120.0f, std::memory_order_relaxed);
    autoMasterLoudnessMatchGainDb.store(0.0f, std::memory_order_relaxed);
    autoMasterLufsDeltaDb.store(0.0f, std::memory_order_relaxed);
    autoMasterTruePeakMarginDb.store(0.0f, std::memory_order_relaxed);
    autoMasterReleaseScore.store(0.0f, std::memory_order_relaxed);
    autoMasterAbLoudnessDeltaDb.store(0.0f, std::memory_order_relaxed);
    autoMasterAbTruePeakDbTp.store(-120.0f, std::memory_order_relaxed);
    autoMasterAbTruePeakDeltaDb.store(0.0f, std::memory_order_relaxed);
    autoMasterAbDynamicsDeltaDb.store(0.0f, std::memory_order_relaxed);
    autoMasterAbScore.store(0.0f, std::memory_order_relaxed);
}

void FunkyMooseMixAnalyzerAudioProcessor::applyAutoMaster(juce::AudioBuffer<float>& buffer,
                                                         int channelCount,
                                                         int numSamples) noexcept
{
    const auto enabled = parameters.getRawParameterValue("autoMasterEnabled")->load() >= 0.5f;
    const auto strengthPercent = juce::jlimit(0.0f,
                                              100.0f,
                                              parameters.getRawParameterValue("autoMasterStrength")->load());
    const auto strength = enabled ? strengthPercent / 100.0f : 0.0f;
    const auto genreIndex = static_cast<int>(parameters.getRawParameterValue("genre")->load());
    const auto& profile = fmma::getGenreProfile(genreIndex);

    autoMasterStrengthPct.store(enabled ? strengthPercent : 0.0f, std::memory_order_relaxed);
    autoMasterTargetLufs.store(profile.targetLufs, std::memory_order_relaxed);
    autoMasterCeilingDbTp.store(autoMasterDefaultCeilingDbTp, std::memory_order_relaxed);

    if (! enabled || strength <= 0.0f || channelCount <= 0 || numSamples <= 0)
    {
        resetAutoMasterState();
        autoMasterTargetLufs.store(profile.targetLufs, std::memory_order_relaxed);
        autoMasterCeilingDbTp.store(autoMasterDefaultCeilingDbTp, std::memory_order_relaxed);
        return;
    }

    const auto metrics = getMetrics();
    const auto analysisTime = metrics.fullPassCompleted && metrics.fullPassSeconds > 0.0f
        ? metrics.fullPassSeconds
        : metrics.analysisSeconds;
    const auto trust = audioEvidenceTrust(analysisTime);
    const auto truePeakForSafety = metrics.worstTruePeakDb > -119.0f
        ? juce::jmax(metrics.truePeakDb, metrics.worstTruePeakDb)
        : metrics.truePeakDb;
    const auto hasLoudness = std::isfinite(metrics.integratedLufs) && metrics.integratedLufs > -90.0f;
    const auto hasTruePeak = std::isfinite(truePeakForSafety) && truePeakForSafety > -119.0f;
    const auto safetyNeedsCut = hasTruePeak && truePeakForSafety > (autoMasterDefaultCeilingDbTp - 0.1f);
    const auto gainTrust = safetyNeedsCut ? juce::jmax(0.65f, trust) : trust;

    auto targetGainDb = 0.0f;
    if (hasLoudness)
    {
        targetGainDb = juce::jlimit(autoMasterMaxCutDb,
                                    autoMasterMaxBoostDb,
                                    profile.targetLufs - metrics.integratedLufs);

        if (hasTruePeak)
            targetGainDb = juce::jmin(targetGainDb, autoMasterDefaultCeilingDbTp - truePeakForSafety);

        targetGainDb = juce::jlimit(autoMasterMaxCutDb, autoMasterMaxBoostDb, targetGainDb);
    }
    else if (safetyNeedsCut)
    {
        targetGainDb = juce::jlimit(autoMasterMaxCutDb,
                                    0.0f,
                                    autoMasterDefaultCeilingDbTp - truePeakForSafety);
    }

    targetGainDb *= strength * gainTrust;

    auto targetLowShelfDb = 0.0f;
    const auto lowEndPercent = fmma::lowEndOf(metrics);
    if (std::isfinite(lowEndPercent) && trust > 0.0f)
    {
        if (lowEndPercent < profile.lowEndRange.low)
            targetLowShelfDb = juce::jlimit(0.0f,
                                            autoMasterMaxLowShelfBoostDb,
                                            (profile.lowEndRange.low - lowEndPercent) * 0.12f);
        else if (lowEndPercent > profile.lowEndRange.high)
            targetLowShelfDb = juce::jlimit(autoMasterMaxLowShelfCutDb,
                                            0.0f,
                                            -(lowEndPercent - profile.lowEndRange.high) * 0.10f);
    }

    auto targetPresenceDb = 0.0f;
    const auto presencePercent = fmma::presenceOf(metrics);
    if (std::isfinite(presencePercent) && trust > 0.0f)
    {
        if (presencePercent > profile.presenceMax)
            targetPresenceDb = juce::jlimit(autoMasterMaxPresenceCutDb,
                                            0.0f,
                                            -(presencePercent - profile.presenceMax) * 0.12f);
        else if (presencePercent < profile.presenceMax - 14.0f && metrics.spectralCentroidHz > 600.0f)
            targetPresenceDb = juce::jlimit(0.0f,
                                            autoMasterMaxPresenceBoostDb,
                                            ((profile.presenceMax - 14.0f) - presencePercent) * 0.05f);

        if (metrics.resonanceGainDb > 8.0f
            && metrics.resonanceFreqHz >= 1800.0f
            && metrics.resonanceFreqHz <= 6500.0f)
        {
            targetPresenceDb = juce::jmin(targetPresenceDb,
                                          -juce::jlimit(0.3f, 1.2f, (metrics.resonanceGainDb - 7.0f) * 0.25f));
        }
    }

    auto targetAirShelfDb = 0.0f;
    if (std::isfinite(metrics.bandPercents[5])
        && metrics.bandPercents[5] < 5.0f
        && targetPresenceDb >= -0.2f
        && metrics.spectralRolloffHz > 0.0f)
    {
        targetAirShelfDb = juce::jlimit(0.0f,
                                        autoMasterMaxAirBoostDb,
                                        (5.0f - metrics.bandPercents[5]) * 0.18f);
    }

    targetLowShelfDb *= strength * trust;
    targetPresenceDb *= strength * trust;
    targetAirShelfDb *= strength * trust;

    auto targetSideGain = 1.0f;
    const auto lowEndCorrelation = fmma::lowEndCorrelationOf(metrics);
    const auto lowEndSideDb = fmma::lowEndSideDbOf(metrics);
    if ((std::isfinite(metrics.correlation) && metrics.correlation < profile.correlationMin)
        || (std::isfinite(metrics.monoLossDb) && metrics.monoLossDb < -2.5f)
        || (std::isfinite(lowEndCorrelation) && lowEndCorrelation < 0.65f)
        || (std::isfinite(lowEndSideDb) && lowEndSideDb > -6.0f))
    {
        targetSideGain = 0.82f;
    }
    else if (! profile.wideExpected && metrics.widthPct > 45.0f)
    {
        targetSideGain = 0.90f;
    }
    else if (profile.wideExpected && metrics.widthPct > 0.0f && metrics.widthPct < 25.0f && metrics.correlation > 0.75f)
    {
        targetSideGain = 1.04f;
    }
    targetSideGain = 1.0f + ((targetSideGain - 1.0f) * strength * trust);

    auto targetGlueDepthDb = 0.0f;
    auto crestForGlueDb = metrics.crestDb;
    if (hasTruePeak && std::isfinite(metrics.rmsDb) && metrics.rmsDb > -119.0f)
        crestForGlueDb = juce::jmax(crestForGlueDb, truePeakForSafety - metrics.rmsDb);

    if (trust > 0.0f && std::isfinite(crestForGlueDb))
    {
        if (crestForGlueDb > profile.crestRange.high + 1.0f)
            targetGlueDepthDb += (crestForGlueDb - profile.crestRange.high) * 0.20f;
        if (metrics.lraLu > 0.0f && metrics.lraLu > 10.0f)
            targetGlueDepthDb += (metrics.lraLu - 10.0f) * 0.10f;
        if (metrics.transientDensity > 4.0f && metrics.attackTimeMs > 0.0f && metrics.attackTimeMs < 14.0f)
            targetGlueDepthDb += juce::jlimit(0.0f, 0.8f, (metrics.transientDensity - 4.0f) * 0.08f);
        if (crestForGlueDb < profile.crestRange.low + 0.5f)
            targetGlueDepthDb *= 0.35f;
    }
    targetGlueDepthDb = juce::jlimit(0.0f, autoMasterMaxGlueReductionDb, targetGlueDepthDb * strength * trust);

    const auto blockSeconds = static_cast<float>(static_cast<double>(numSamples) / currentSampleRate);
    const auto gainSmoothing = smoothingForWindow(blockSeconds, targetGainDb < smoothedAutoMasterGainDb ? 0.28f : 1.6f);
    const auto toneSmoothing = smoothingForWindow(blockSeconds, 2.2f);
    const auto widthSmoothing = smoothingForWindow(blockSeconds, 1.2f);
    const auto glueSmoothing = smoothingForWindow(blockSeconds, 2.8f);
    smoothedAutoMasterGainDb = smoothed(smoothedAutoMasterGainDb, targetGainDb, gainSmoothing);
    smoothedAutoMasterLowShelfDb = smoothed(smoothedAutoMasterLowShelfDb, targetLowShelfDb, toneSmoothing);
    smoothedAutoMasterPresenceDb = smoothed(smoothedAutoMasterPresenceDb, targetPresenceDb, toneSmoothing);
    smoothedAutoMasterAirShelfDb = smoothed(smoothedAutoMasterAirShelfDb, targetAirShelfDb, toneSmoothing);
    smoothedAutoMasterSideGain = smoothed(smoothedAutoMasterSideGain, targetSideGain, widthSmoothing);
    smoothedAutoMasterGlueDepthDb = smoothed(smoothedAutoMasterGlueDepthDb, targetGlueDepthDb, glueSmoothing);

    const auto channelsToProcess = juce::jmin(juce::jmin(channelCount, buffer.getNumChannels()), 2);
    for (auto channel = 0; channel < channelsToProcess; ++channel)
    {
        autoMasterLowShelfFilters[static_cast<size_t>(channel)].setLowShelf(currentSampleRate, 120.0f, smoothedAutoMasterLowShelfDb);
        autoMasterPresenceFilters[static_cast<size_t>(channel)].setPeak(currentSampleRate, 3600.0f, 0.85f, smoothedAutoMasterPresenceDb);
        autoMasterAirShelfFilters[static_cast<size_t>(channel)].setHighShelf(currentSampleRate, 9000.0f, smoothedAutoMasterAirShelfDb);
    }

    const auto outputGain = decibelsToGain(smoothedAutoMasterGainDb);
    const auto ceilingGain = decibelsToGain(autoMasterDefaultCeilingDbTp);
    const auto kneeStart = ceilingGain * 0.94f;
    const auto kneeRange = juce::jmax(ceilingGain - kneeStart, 1.0e-6f);
    const auto sampleTimeSeconds = 1.0f / static_cast<float>(juce::jmax(1.0, currentSampleRate));
    const auto glueAttack = smoothingForWindow(sampleTimeSeconds, 0.018f);
    const auto glueRelease = smoothingForWindow(sampleTimeSeconds, 0.240f);
    const auto limiterAttack = smoothingForWindow(sampleTimeSeconds, 0.0007f);
    const auto limiterRelease = smoothingForWindow(sampleTimeSeconds, 0.085f);
    const auto glueThresholdDb = -18.0f + juce::jlimit(-3.0f, 3.0f, smoothedAutoMasterGainDb * 0.20f);
    const auto glueRatio = 1.0f + (0.65f * strength * trust);
    auto maxGlueReductionDb = 0.0f;
    auto maxLimiterReductionDb = 0.0f;

    auto nextGlueGain = [&] (float linkedPeak) noexcept
    {
        auto wantedGain = 1.0f;
        if (smoothedAutoMasterGlueDepthDb > 0.01f
            && linkedPeak > 1.0e-6f
            && glueRatio > 1.01f)
        {
            const auto levelDb = gainToDecibels(linkedPeak);
            if (levelDb > glueThresholdDb)
            {
                const auto overDb = levelDb - glueThresholdDb;
                const auto reductionDb = juce::jlimit(0.0f,
                                                      smoothedAutoMasterGlueDepthDb,
                                                      overDb * (1.0f - (1.0f / glueRatio)));
                wantedGain = decibelsToGain(-reductionDb);
            }
        }

        const auto smoothing = wantedGain < autoMasterGlueGain ? glueAttack : glueRelease;
        autoMasterGlueGain += (wantedGain - autoMasterGlueGain) * smoothing;
        autoMasterGlueGain = juce::jlimit(0.05f, 1.0f, autoMasterGlueGain);
        maxGlueReductionDb = juce::jmax(maxGlueReductionDb, -gainToDecibels(autoMasterGlueGain));
        return autoMasterGlueGain;
    };

    auto nextLimiterGain = [&] (float linkedPeak) noexcept
    {
        const auto wantedGain = linkedPeak > ceilingGain
            ? juce::jlimit(0.0f, 1.0f, ceilingGain / juce::jmax(linkedPeak, 1.0e-6f))
            : 1.0f;
        const auto smoothing = wantedGain < autoMasterLimiterGain ? limiterAttack : limiterRelease;
        autoMasterLimiterGain += (wantedGain - autoMasterLimiterGain) * smoothing;
        autoMasterLimiterGain = juce::jlimit(0.02f, 1.0f, autoMasterLimiterGain);
        maxLimiterReductionDb = juce::jmax(maxLimiterReductionDb, -gainToDecibels(autoMasterLimiterGain));
        return autoMasterLimiterGain;
    };

    auto guardSample = [&] (float sample) noexcept
    {
        if (! std::isfinite(sample))
            return 0.0f;

        const auto absoluteSample = std::abs(sample);
        if (absoluteSample <= kneeStart)
            return sample;

        const auto over = (absoluteSample - kneeStart) / kneeRange;
        const auto shapedAbs = juce::jmin(ceilingGain, kneeStart + (kneeRange * std::tanh(over)));
        const auto limited = std::copysign(shapedAbs, sample);
        maxLimiterReductionDb = juce::jmax(maxLimiterReductionDb,
                                           gainToDecibels(absoluteSample) - gainToDecibels(std::abs(limited)));
        return limited;
    };

    if (channelsToProcess == 1)
    {
        auto* mono = buffer.getWritePointer(0);
        for (auto sample = 0; sample < numSamples; ++sample)
        {
            auto value = mono[sample];
            value = autoMasterLowShelfFilters[0].process(value);
            value = autoMasterPresenceFilters[0].process(value);
            value = autoMasterAirShelfFilters[0].process(value);
            value *= nextGlueGain(std::abs(value));
            value *= outputGain;
            value *= nextLimiterGain(std::abs(value));
            mono[sample] = guardSample(value);
        }
    }
    else if (channelsToProcess >= 2)
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);
        for (auto sample = 0; sample < numSamples; ++sample)
        {
            auto l = left[sample];
            auto r = right[sample];
            const auto mid = 0.5f * (l + r);
            const auto side = 0.5f * (l - r) * smoothedAutoMasterSideGain;
            l = mid + side;
            r = mid - side;

            l = autoMasterLowShelfFilters[0].process(l);
            l = autoMasterPresenceFilters[0].process(l);
            l = autoMasterAirShelfFilters[0].process(l);
            r = autoMasterLowShelfFilters[1].process(r);
            r = autoMasterPresenceFilters[1].process(r);
            r = autoMasterAirShelfFilters[1].process(r);

            const auto glueGain = nextGlueGain(juce::jmax(std::abs(l), std::abs(r)));
            l *= glueGain * outputGain;
            r *= glueGain * outputGain;

            const auto limiterGain = nextLimiterGain(juce::jmax(std::abs(l), std::abs(r)));
            left[sample] = guardSample(l * limiterGain);
            right[sample] = guardSample(r * limiterGain);
        }
    }

    autoMasterGainDb.store(smoothedAutoMasterGainDb, std::memory_order_relaxed);
    autoMasterLowShelfDb.store(smoothedAutoMasterLowShelfDb, std::memory_order_relaxed);
    autoMasterPresenceDb.store(smoothedAutoMasterPresenceDb, std::memory_order_relaxed);
    autoMasterAirShelfDb.store(smoothedAutoMasterAirShelfDb, std::memory_order_relaxed);
    autoMasterWidthPercent.store(smoothedAutoMasterSideGain * 100.0f, std::memory_order_relaxed);
    const auto glueDisplayRelease = smoothingForWindow(blockSeconds, 1.4f);
    const auto limiterDisplayRelease = smoothingForWindow(blockSeconds, 0.9f);
    displayedAutoMasterGlueReductionDb = maxGlueReductionDb > displayedAutoMasterGlueReductionDb
        ? maxGlueReductionDb
        : smoothed(displayedAutoMasterGlueReductionDb, maxGlueReductionDb, glueDisplayRelease);
    displayedAutoMasterLimiterReductionDb = maxLimiterReductionDb > displayedAutoMasterLimiterReductionDb
        ? maxLimiterReductionDb
        : smoothed(displayedAutoMasterLimiterReductionDb, maxLimiterReductionDb, limiterDisplayRelease);

    autoMasterGlueReductionDb.store(displayedAutoMasterGlueReductionDb, std::memory_order_relaxed);
    autoMasterLimiterReductionDb.store(displayedAutoMasterLimiterReductionDb, std::memory_order_relaxed);

    const auto projectedLufs = hasLoudness
        ? juce::jlimit(-120.0f, 24.0f, metrics.integratedLufs + smoothedAutoMasterGainDb)
        : -120.0f;
    const auto rawProjectedTruePeak = hasTruePeak
        ? truePeakForSafety
            + smoothedAutoMasterGainDb
            - displayedAutoMasterGlueReductionDb
            - displayedAutoMasterLimiterReductionDb
        : -120.0f;
    const auto projectedTruePeak = hasTruePeak
        ? juce::jmin(autoMasterDefaultCeilingDbTp, rawProjectedTruePeak)
        : -120.0f;
    const auto projectedLufsDelta = hasLoudness ? projectedLufs - profile.targetLufs : 0.0f;
    const auto projectedTruePeakMargin = hasTruePeak ? autoMasterDefaultCeilingDbTp - rawProjectedTruePeak : 0.0f;
    auto releaseScore = 0.0f;
    if (hasLoudness && hasTruePeak)
    {
        const auto reductionLoadDb = displayedAutoMasterGlueReductionDb + displayedAutoMasterLimiterReductionDb;
        releaseScore = 100.0f;
        releaseScore -= juce::jlimit(0.0f, 36.0f, std::abs(projectedLufsDelta) * 9.0f);
        releaseScore -= juce::jlimit(0.0f, 22.0f, displayedAutoMasterLimiterReductionDb * 7.0f);
        releaseScore -= juce::jlimit(0.0f, 12.0f, displayedAutoMasterGlueReductionDb * 2.5f);
        releaseScore -= juce::jlimit(0.0f, 14.0f, juce::jmax(0.0f, std::abs(smoothedAutoMasterGainDb) - 5.5f) * 3.5f);
        releaseScore -= juce::jlimit(0.0f, 18.0f, juce::jmax(0.0f, reductionLoadDb - 4.0f) * 4.5f);
        releaseScore -= projectedTruePeakMargin < 0.0f
            ? juce::jlimit(0.0f, 28.0f, -projectedTruePeakMargin * 12.0f)
            : 0.0f;
        releaseScore = juce::jlimit(0.0f, 100.0f, releaseScore * (0.65f + (0.35f * trust)));
    }

    const auto loudnessMatchGainDb = hasLoudness ? -smoothedAutoMasterGainDb : 0.0f;
    const auto abMatchedLufs = hasLoudness
        ? juce::jlimit(-120.0f, 24.0f, projectedLufs + loudnessMatchGainDb)
        : -120.0f;
    const auto abLoudnessDelta = hasLoudness ? abMatchedLufs - metrics.integratedLufs : 0.0f;
    const auto abTruePeakDb = hasTruePeak
        ? juce::jlimit(-120.0f, 24.0f, projectedTruePeak + loudnessMatchGainDb)
        : -120.0f;
    const auto abTruePeakDelta = hasTruePeak ? abTruePeakDb - truePeakForSafety : 0.0f;
    const auto abDynamicsDelta = hasTruePeak
        ? -juce::jlimit(0.0f, 12.0f, displayedAutoMasterGlueReductionDb + displayedAutoMasterLimiterReductionDb)
        : 0.0f;
    auto abScore = 0.0f;
    if (hasLoudness && hasTruePeak)
    {
        auto truePeakRisk = [] (float marginDb) noexcept
        {
            if (! std::isfinite(marginDb))
                return 0.0f;

            if (marginDb < 0.0f)
                return 8.0f + (-marginDb * 6.0f);

            return marginDb < 0.5f ? (0.5f - marginDb) * 4.0f : 0.0f;
        };

        auto stereoRisk = [&profile] (float corr,
                                      float monoLoss,
                                      float lowCorr,
                                      float lowSideDb,
                                      float width) noexcept
        {
            auto risk = 0.0f;
            if (std::isfinite(corr))
                risk += juce::jmax(0.0f, profile.correlationMin - corr) * 20.0f;
            if (std::isfinite(monoLoss))
                risk += juce::jmax(0.0f, -2.5f - monoLoss) * 1.5f;
            if (std::isfinite(lowCorr))
                risk += juce::jmax(0.0f, 0.65f - lowCorr) * 18.0f;
            if (std::isfinite(lowSideDb))
                risk += juce::jmax(0.0f, lowSideDb + 6.0f) * 1.2f;
            if (! profile.wideExpected && std::isfinite(width))
                risk += juce::jmax(0.0f, width - 45.0f) * 0.12f;
            return risk;
        };

        const auto estimatedLowEnd = std::isfinite(lowEndPercent)
            ? juce::jlimit(0.0f, 100.0f, lowEndPercent + (smoothedAutoMasterLowShelfDb * 4.5f))
            : lowEndPercent;
        const auto estimatedPresence = std::isfinite(presencePercent)
            ? juce::jlimit(0.0f, 100.0f, presencePercent + (smoothedAutoMasterPresenceDb * 5.5f))
            : presencePercent;
        const auto toneRiskBefore = distanceOutsideRange(lowEndPercent, profile.lowEndRange)
            + (std::isfinite(presencePercent) ? juce::jmax(0.0f, presencePercent - profile.presenceMax) : 0.0f);
        const auto toneRiskAfter = distanceOutsideRange(estimatedLowEnd, profile.lowEndRange)
            + (std::isfinite(estimatedPresence) ? juce::jmax(0.0f, estimatedPresence - profile.presenceMax) : 0.0f);

        const auto sideGainDeltaDb = gainToDecibels(smoothedAutoMasterSideGain);
        const auto estimatedWidth = std::isfinite(metrics.widthPct)
            ? juce::jmax(0.0f, metrics.widthPct * smoothedAutoMasterSideGain)
            : metrics.widthPct;
        const auto abLowEndCorrelation = fmma::lowEndCorrelationOf(metrics);
        const auto abLowEndSideDb = fmma::lowEndSideDbOf(metrics);
        const auto estimatedCorrelation = std::isfinite(metrics.correlation)
            ? juce::jlimit(-1.0f, 1.0f, metrics.correlation + ((1.0f - smoothedAutoMasterSideGain) * 0.35f))
            : metrics.correlation;
        const auto estimatedMonoLoss = std::isfinite(metrics.monoLossDb)
            ? metrics.monoLossDb + (juce::jmax(0.0f, 1.0f - smoothedAutoMasterSideGain) * 2.0f)
            : metrics.monoLossDb;
        const auto estimatedLowEndCorrelation = std::isfinite(abLowEndCorrelation)
            ? juce::jlimit(-1.0f, 1.0f, abLowEndCorrelation + (juce::jmax(0.0f, 1.0f - smoothedAutoMasterSideGain) * 0.25f))
            : abLowEndCorrelation;
        const auto estimatedLowEndSideDb = std::isfinite(abLowEndSideDb) ? abLowEndSideDb + sideGainDeltaDb
                                                                          : abLowEndSideDb;
        const auto stereoRiskBefore = stereoRisk(metrics.correlation,
                                                 metrics.monoLossDb,
                                                 abLowEndCorrelation,
                                                 abLowEndSideDb,
                                                 metrics.widthPct);
        const auto stereoRiskAfter = stereoRisk(estimatedCorrelation,
                                                estimatedMonoLoss,
                                                estimatedLowEndCorrelation,
                                                estimatedLowEndSideDb,
                                                estimatedWidth);

        const auto originalTruePeakMargin = autoMasterDefaultCeilingDbTp - truePeakForSafety;
        const auto abTruePeakMargin = autoMasterDefaultCeilingDbTp - abTruePeakDb;
        const auto truePeakRiskBefore = truePeakRisk(originalTruePeakMargin);
        const auto truePeakRiskAfter = truePeakRisk(abTruePeakMargin);
        const auto reductionLoadDb = displayedAutoMasterGlueReductionDb + displayedAutoMasterLimiterReductionDb;
        const auto nonGainMovement = std::abs(smoothedAutoMasterLowShelfDb)
            + std::abs(smoothedAutoMasterPresenceDb)
            + std::abs(smoothedAutoMasterAirShelfDb)
            + (std::abs(smoothedAutoMasterSideGain - 1.0f) * 10.0f)
            + reductionLoadDb;

        abScore = 50.0f;
        abScore += juce::jlimit(-18.0f, 20.0f, (toneRiskBefore - toneRiskAfter) * 3.0f);
        abScore += juce::jlimit(-15.0f, 18.0f, (stereoRiskBefore - stereoRiskAfter) * 2.5f);
        abScore += juce::jlimit(-14.0f, 18.0f, (truePeakRiskBefore - truePeakRiskAfter) * 2.2f);
        abScore -= juce::jlimit(0.0f, 20.0f, std::abs(abLoudnessDelta) * 30.0f);
        abScore -= juce::jlimit(0.0f, 15.0f, juce::jmax(0.0f, reductionLoadDb - 4.0f) * 5.0f);
        abScore -= juce::jlimit(0.0f, 10.0f, juce::jmax(0.0f, nonGainMovement - 6.0f) * 2.0f);

        if (nonGainMovement < 0.35f && std::abs(smoothedAutoMasterGainDb) > 1.0f)
            abScore = juce::jmin(abScore, 58.0f);

        if (releaseScore > 0.0f)
            abScore = juce::jmin(abScore, releaseScore + 18.0f);

        abScore = juce::jlimit(0.0f, 100.0f, abScore * (0.65f + (0.35f * trust)));
    }

    autoMasterProjectedLufs.store(projectedLufs, std::memory_order_relaxed);
    autoMasterProjectedTruePeakDbTp.store(projectedTruePeak, std::memory_order_relaxed);
    autoMasterLoudnessMatchGainDb.store(loudnessMatchGainDb, std::memory_order_relaxed);
    autoMasterLufsDeltaDb.store(projectedLufsDelta, std::memory_order_relaxed);
    autoMasterTruePeakMarginDb.store(projectedTruePeakMargin, std::memory_order_relaxed);
    autoMasterReleaseScore.store(releaseScore, std::memory_order_relaxed);
    autoMasterAbLoudnessDeltaDb.store(abLoudnessDelta, std::memory_order_relaxed);
    autoMasterAbTruePeakDbTp.store(abTruePeakDb, std::memory_order_relaxed);
    autoMasterAbTruePeakDeltaDb.store(abTruePeakDelta, std::memory_order_relaxed);
    autoMasterAbDynamicsDeltaDb.store(abDynamicsDelta, std::memory_order_relaxed);
    autoMasterAbScore.store(abScore, std::memory_order_relaxed);
}

void FunkyMooseMixAnalyzerAudioProcessor::timerCallback()
{
    const auto m = getMetrics();
    const auto genreIndex = static_cast<int>(*parameters.getRawParameterValue("genre"));
    const auto instrumental = static_cast<bool>(*parameters.getRawParameterValue("instrumental"));
    const auto profile = fmma::getGenreProfile(genreIndex);
    
    auto input = makeAssessmentInput();
    input.instrumental = instrumental;
    
    const auto assessment = fmma::assessMix(input, profile);
    
    oscSender.send(m, assessment);
}

fmma::MixAssessmentInput FunkyMooseMixAnalyzerAudioProcessor::makeAssessmentInput() const
{
    const auto m = getMetrics();
    fmma::MixAssessmentInput input;
    input.integratedLufs = m.integratedLufs;
    input.crestDb = m.crestDb;
    input.correlation = m.correlation;
    input.msRatioDb = m.msRatioDb;
    input.monoLossDb = m.monoLossDb;
    input.truePeakDb = m.truePeakDb;
    input.clippedPercent = m.clippedPercent;
    input.worstTruePeakDb = m.worstTruePeakDb;
    input.worstCorrelation = m.worstCorrelation;
    input.worstMonoLossDb = m.worstMonoLossDb;
    input.worstClippedPercent = m.worstClippedPercent;
    input.worstLowMidPercent = m.worstLowMidPercent;
    input.worstResonanceFreqHz = m.worstResonanceFreqHz;
    input.worstResonanceGainDb = m.worstResonanceGainDb;
    input.subPercent = m.bandPercents[0];
    input.bassPercent = m.bandPercents[1];
    input.lowMidPercent = m.bandPercents[2];
    input.midPercent = m.bandPercents[3];
    input.lowEndPercent = fmma::lowEndOf(m);
    input.presencePercent = fmma::presenceOf(m);
    input.airPercent = m.bandPercents[5];
    input.lowEndCorrelation = fmma::lowEndCorrelationOf(m);
    input.lowEndSideDb = fmma::lowEndSideDbOf(m);
    input.widthPct = m.widthPct;
    input.lraLu = m.lraLu;
    input.transientDensity = m.transientDensity;
    input.attackTimeMs = m.attackTimeMs;
    input.spectralCentroidHz = m.spectralCentroidHz;
    input.spectralRolloffHz = m.spectralRolloffHz;
    input.resonanceFreqHz = m.resonanceFreqHz;
    input.resonanceGainDb = m.resonanceGainDb;
    input.analysisSeconds = m.analysisSeconds;
    input.fullPassSeconds = m.fullPassSeconds;
    input.fullPassActive = m.fullPassActive;
    input.fullPassCompleted = m.fullPassCompleted;
    input.analysisFrozen = m.analysisFrozen;
    return input;
}

void FunkyMooseMixAnalyzerAudioProcessor::publishMetric(std::atomic<float>& target,
                                                        float value,
                                                        float smoothing) noexcept
{
    const auto current = target.load(std::memory_order_relaxed);
    target.store(smoothed(current, value, smoothing), std::memory_order_relaxed);
}

void FunkyMooseMixAnalyzerAudioProcessor::pushLoudnessPower(double power) noexcept
{
    if (! std::isfinite(power) || power < 0.0)
        power = 0.0;

    if (! momentaryPowerRing.empty())
    {
        momentaryPowerSum -= momentaryPowerRing[momentaryPowerIndex];
        momentaryPowerRing[momentaryPowerIndex] = power;
        momentaryPowerSum += power;
        momentaryPowerIndex = (momentaryPowerIndex + 1) % momentaryPowerRing.size();
        momentaryPowerFilled = juce::jmin(momentaryPowerFilled + 1, momentaryPowerRing.size());
    }

    if (! shortTermPowerRing.empty())
    {
        shortTermPowerSum -= shortTermPowerRing[shortTermPowerIndex];
        shortTermPowerRing[shortTermPowerIndex] = power;
        shortTermPowerSum += power;
        shortTermPowerIndex = (shortTermPowerIndex + 1) % shortTermPowerRing.size();
        shortTermPowerFilled = juce::jmin(shortTermPowerFilled + 1, shortTermPowerRing.size());
    }

    --integratedSamplesUntilHop;
    if (integratedSamplesUntilHop <= 0)
    {
        integratedSamplesUntilHop += integratedHopSamples;

        if (momentaryPowerFilled == momentaryPowerRing.size() && momentaryPowerFilled > 0)
            addIntegratedLoudnessBlock(momentaryPowerSum / static_cast<double>(momentaryPowerFilled));

        if (shortTermPowerFilled == shortTermPowerRing.size() && shortTermPowerFilled > 0)
            addLraShortTermBlock(shortTermPowerSum / static_cast<double>(shortTermPowerFilled));
    }
}

void FunkyMooseMixAnalyzerAudioProcessor::addIntegratedLoudnessBlock(double meanPower) noexcept
{
    if (powerToLufs(meanPower) < static_cast<float>(absoluteGateLufs))
        return;

    integratedBlockPowers[integratedBlockWriteIndex] = static_cast<float>(meanPower);
    integratedBlockWriteIndex = (integratedBlockWriteIndex + 1) % integratedBlockPowers.size();
    integratedBlockCount = juce::jmin(integratedBlockCount + 1, integratedBlockPowers.size());
    integratedLoudnessDirty = true;
}

int FunkyMooseMixAnalyzerAudioProcessor::lraBinFor(float lufs) noexcept
{
    return juce::jlimit(
        0,
        lraHistogramBinCount - 1,
        static_cast<int>(std::round((lufs - lraHistogramMinLufs) / lraHistogramStepLu)));
}

float FunkyMooseMixAnalyzerAudioProcessor::lraBinCenter(int bin) noexcept
{
    return lraHistogramMinLufs + (static_cast<float>(bin) * lraHistogramStepLu);
}

void FunkyMooseMixAnalyzerAudioProcessor::addLraShortTermBlock(double meanPower) noexcept
{
    const auto lufs = powerToLufs(meanPower);
    if (lufs < static_cast<float>(absoluteGateLufs))
        return;

    if (lraBlockCount == integratedBlockCapacity)
    {
        const auto oldBin = lraBlockBins[lraBlockWriteIndex];
        if (oldBin >= 0 && oldBin < lraHistogramBinCount && lraHistogram[static_cast<size_t>(oldBin)] > 0)
            --lraHistogram[static_cast<size_t>(oldBin)];
        lraPowerSum -= static_cast<double>(lraBlockPowers[lraBlockWriteIndex]);
    }
    else
    {
        ++lraBlockCount;
    }

    const auto bin = lraBinFor(lufs);
    lraBlockPowers[lraBlockWriteIndex] = static_cast<float>(meanPower);
    lraBlockBins[lraBlockWriteIndex] = bin;
    ++lraHistogram[static_cast<size_t>(bin)];
    lraPowerSum += meanPower;
    lraBlockWriteIndex = (lraBlockWriteIndex + 1) % integratedBlockCapacity;
}

double FunkyMooseMixAnalyzerAudioProcessor::calculateIntegratedLoudnessPower() const noexcept
{
    if (integratedBlockCount == 0)
        return 0.0;

    auto absoluteGatedPowerSum = 0.0;
    auto absoluteGatedBlocks = 0;

    for (auto i = size_t { 0 }; i < integratedBlockCount; ++i)
    {
        const auto power = static_cast<double>(integratedBlockPowers[i]);
        if (powerToLufs(power) >= static_cast<float>(absoluteGateLufs))
        {
            absoluteGatedPowerSum += power;
            ++absoluteGatedBlocks;
        }
    }

    if (absoluteGatedBlocks == 0)
        return 0.0;

    const auto preliminaryPower = absoluteGatedPowerSum / static_cast<double>(absoluteGatedBlocks);
    const auto relativeGate = juce::jmax(absoluteGateLufs, static_cast<double>(powerToLufs(preliminaryPower)) - 10.0);
    auto relativeGatedPowerSum = 0.0;
    auto relativeGatedBlocks = 0;

    for (auto i = size_t { 0 }; i < integratedBlockCount; ++i)
    {
        const auto power = static_cast<double>(integratedBlockPowers[i]);
        if (powerToLufs(power) >= static_cast<float>(relativeGate))
        {
            relativeGatedPowerSum += power;
            ++relativeGatedBlocks;
        }
    }

    return relativeGatedBlocks > 0 ? relativeGatedPowerSum / static_cast<double>(relativeGatedBlocks)
                                   : preliminaryPower;
}

float FunkyMooseMixAnalyzerAudioProcessor::calculateLraLu() const noexcept
{
    if (lraBlockCount < 2 || lraPowerSum <= minimumPower)
        return 0.0f;

    const auto relativeGate = powerToLufs(lraPowerSum / static_cast<double>(lraBlockCount)) - 20.0f;
    const auto gateBin = lraBinFor(juce::jmax(static_cast<float>(absoluteGateLufs), relativeGate));

    auto gatedCount = 0;
    for (auto bin = gateBin; bin < lraHistogramBinCount; ++bin)
        gatedCount += lraHistogram[static_cast<size_t>(bin)];

    if (gatedCount < 2)
        return 0.0f;

    const auto lowerRank = juce::jlimit(0, gatedCount - 1, static_cast<int>(std::floor(gatedCount * 0.10f)));
    const auto upperRank = juce::jlimit(0, gatedCount - 1, static_cast<int>(std::ceil(gatedCount * 0.95f)) - 1);
    auto cumulative = 0;
    auto lowerBin = gateBin;
    auto upperBin = gateBin;

    for (auto bin = gateBin; bin < lraHistogramBinCount; ++bin)
    {
        cumulative += lraHistogram[static_cast<size_t>(bin)];
        if (cumulative > lowerRank)
        {
            lowerBin = bin;
            break;
        }
    }

    cumulative = 0;
    for (auto bin = gateBin; bin < lraHistogramBinCount; ++bin)
    {
        cumulative += lraHistogram[static_cast<size_t>(bin)];
        if (cumulative > upperRank)
        {
            upperBin = bin;
            break;
        }
    }

    return juce::jlimit(0.0f, 40.0f, lraBinCenter(upperBin) - lraBinCenter(lowerBin));
}

float FunkyMooseMixAnalyzerAudioProcessor::calculateTruePeakDb(const juce::AudioBuffer<float>& buffer,
                                                               int channelCount) noexcept
{
    if (truePeakOversampler == nullptr)
        return -120.0f;

    const auto channelsToRead = juce::jlimit(1, truePeakChannelCount, channelCount);
    juce::dsp::AudioBlock<const float> sourceBlock(buffer);
    const auto inputBlock = sourceBlock.getSubsetChannelBlock(0, static_cast<size_t>(channelsToRead));
    const auto oversampledBlock = truePeakOversampler->processSamplesUp(inputBlock);

    auto peak = 0.0f;
    for (auto channel = size_t { 0 }; channel < oversampledBlock.getNumChannels(); ++channel)
    {
        const auto* samples = oversampledBlock.getChannelPointer(channel);
        for (auto sample = size_t { 0 }; sample < oversampledBlock.getNumSamples(); ++sample)
            peak = juce::jmax(peak, std::abs(samples[sample]));
    }

    return toDb(peak);
}

void FunkyMooseMixAnalyzerAudioProcessor::pushSpectrumSample(float sample) noexcept
{
    spectrumFifo[static_cast<size_t>(spectrumFifoIndex)] = sample;
    ++spectrumFifoIndex;

    if (spectrumFifoIndex >= spectrumFftSize)
    {
        analyseSpectrumFrame();
        spectrumFifoIndex = 0;
    }
}

void FunkyMooseMixAnalyzerAudioProcessor::analyseSpectrumFrame() noexcept
{
    spectrumData.fill(0.0f);
    std::copy(spectrumFifo.begin(), spectrumFifo.end(), spectrumData.begin());
    spectrumWindow.multiplyWithWindowingTable(spectrumData.data(), spectrumFftSize);
    spectrumFft.performFrequencyOnlyForwardTransform(spectrumData.data(), true);

    const auto nyquist = static_cast<float>(currentSampleRate * 0.5);
    const auto binHz = static_cast<float>(currentSampleRate / static_cast<double>(spectrumFftSize));
    const auto maxBin = juce::jlimit(1, spectrumFftSize / 2, static_cast<int>(std::floor(juce::jmin(nyquist, 20000.0f) / binHz)));

    auto totalPower = 0.0;
    auto centroidNumerator = 0.0;
    std::array<double, fmma::bandCount> fftBandEnergy {};

    for (auto bin = 1; bin <= maxBin; ++bin)
    {
        const auto freq = static_cast<float>(bin) * binHz;
        if (freq < 20.0f)
            continue;

        const auto magnitude = spectrumData[static_cast<size_t>(bin)];
        const auto power = static_cast<double>(magnitude) * magnitude;
        totalPower += power;
        centroidNumerator += static_cast<double>(freq) * power;

        for (auto band = 0; band < fmma::bandCount; ++band)
        {
            const auto [low, high] = bandRanges[static_cast<size_t>(band)];
            if (freq >= low && freq < juce::jmin(high, nyquist + 1.0f))
                fftBandEnergy[static_cast<size_t>(band)] += power;
        }
    }

    if (totalPower <= 1.0e-12)
    {
        publishMetric(spectralCentroidHz, 0.0f, 0.08f);
        publishMetric(spectralRolloffHz, 0.0f, 0.08f);
        publishMetric(resonanceFreqHz, 0.0f, 0.08f);
        publishMetric(resonanceGainDb, 0.0f, 0.08f);
        for (auto& band : bandPercents)
            publishMetric(band, 0.0f, 0.08f);
        return;
    }

    auto cumulativePower = 0.0;
    auto rolloffHz = 0.0f;
    for (auto bin = 1; bin <= maxBin; ++bin)
    {
        const auto freq = static_cast<float>(bin) * binHz;
        if (freq < 20.0f)
            continue;

        const auto magnitude = spectrumData[static_cast<size_t>(bin)];
        cumulativePower += static_cast<double>(magnitude) * magnitude;
        if (cumulativePower >= totalPower * 0.85)
        {
            rolloffHz = freq;
            break;
        }
    }

    auto bestResonanceFreq = 0.0f;
    auto bestResonanceGain = 0.0f;
    for (auto bin = 3; bin <= maxBin - 3; ++bin)
    {
        const auto freq = static_cast<float>(bin) * binHz;
        if (freq < 120.0f || freq > 12000.0f)
            continue;

        const auto peakPower = static_cast<double>(spectrumData[static_cast<size_t>(bin)])
                             * spectrumData[static_cast<size_t>(bin)];
        if (peakPower <= 1.0e-18)
            continue;

        const auto lowFreq = freq / 1.41421356f;
        const auto highFreq = freq * 1.41421356f;
        const auto lowBin = juce::jmax(1, static_cast<int>(std::floor(lowFreq / binHz)));
        const auto highBin = juce::jmin(maxBin, static_cast<int>(std::ceil(highFreq / binHz)));
        auto surroundingPower = 0.0;
        auto surroundingCount = 0;

        for (auto otherBin = lowBin; otherBin <= highBin; ++otherBin)
        {
            if (std::abs(otherBin - bin) <= 2)
                continue;

            const auto magnitude = spectrumData[static_cast<size_t>(otherBin)];
            surroundingPower += static_cast<double>(magnitude) * magnitude;
            ++surroundingCount;
        }

        if (surroundingCount < 8)
            continue;

        const auto averageSurroundingPower = surroundingPower / static_cast<double>(surroundingCount);
        if (averageSurroundingPower <= 1.0e-18)
            continue;

        const auto prominence = static_cast<float>(10.0 * std::log10(peakPower / averageSurroundingPower));
        if (prominence > bestResonanceGain)
        {
            bestResonanceGain = prominence;
            bestResonanceFreq = freq;
        }
    }

    publishMetric(spectralCentroidHz, static_cast<float>(centroidNumerator / totalPower), 0.14f);
    publishMetric(spectralRolloffHz, rolloffHz, 0.14f);
    publishMetric(resonanceFreqHz, bestResonanceGain >= 6.0f ? bestResonanceFreq : 0.0f, 0.14f);
    publishMetric(resonanceGainDb, bestResonanceGain >= 6.0f ? bestResonanceGain : 0.0f, 0.14f);
    if (bestResonanceGain >= 6.0f && bestResonanceGain > worstResonanceGainDb.load(std::memory_order_relaxed))
    {
        worstResonanceGainDb.store(bestResonanceGain, std::memory_order_relaxed);
        worstResonanceFreqHz.store(bestResonanceFreq, std::memory_order_relaxed);
    }

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        const auto percent = static_cast<float>((fftBandEnergy[static_cast<size_t>(band)] / totalPower) * 100.0);
        publishMetric(bandPercents[static_cast<size_t>(band)], percent, 0.14f);
        if (band == 2)
            updateMaximum(worstLowMidPercent, percent);
    }
}

void FunkyMooseMixAnalyzerAudioProcessor::analysePhaseFrame() noexcept
{
    phaseDataLeft.fill(0.0f);
    phaseDataRight.fill(0.0f);
    std::copy(phaseFifoLeft.begin(), phaseFifoLeft.end(), phaseDataLeft.begin());
    std::copy(phaseFifoRight.begin(), phaseFifoRight.end(), phaseDataRight.begin());

    phaseFft.performRealOnlyForwardTransform(phaseDataLeft.data());
    phaseFft.performRealOnlyForwardTransform(phaseDataRight.data());

    double phaseCorrSum = 0.0;
    auto binCount = 0;
    for (auto i = 0; i < spectrumFftSize / 2; ++i)
    {
        const auto bin = static_cast<size_t>(i);
        const auto imaginaryBin = static_cast<size_t>(i + spectrumFftSize / 2);
        const auto leftReal = phaseDataLeft[bin];
        const auto leftImag = phaseDataLeft[imaginaryBin];
        const auto rightReal = phaseDataRight[bin];
        const auto rightImag = phaseDataRight[imaginaryBin];

        const auto leftMag = std::sqrt(leftReal * leftReal + leftImag * leftImag);
        const auto rightMag = std::sqrt(rightReal * rightReal + rightImag * rightImag);

        if (leftMag > 1.0e-6f && rightMag > 1.0e-6f)
        {
            const auto leftPhase = std::atan2(leftImag, leftReal);
            const auto rightPhase = std::atan2(rightImag, rightReal);
            phaseCorrSum += std::cos(leftPhase - rightPhase);
            ++binCount;
        }
    }

    phaseCorrelation.store(static_cast<float>(binCount > 0 ? phaseCorrSum / static_cast<double>(binCount) : 1.0),
                           std::memory_order_relaxed);
}

void FunkyMooseMixAnalyzerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                       juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    auto hostPlayingNow = false;
    if (auto* currentPlayHead = getPlayHead())
    {
        const auto position = currentPlayHead->getPosition();
        if (position.hasValue())
            hostPlayingNow = position->getIsPlaying();
    }

    const auto wasHostPlaying = hostTransportPlaying.exchange(hostPlayingNow, std::memory_order_acq_rel);
    const auto shouldStartFullPass = fullPassStartRequested.exchange(false, std::memory_order_acq_rel);
    const auto shouldReset = resetRequested.exchange(false, std::memory_order_acq_rel);
    const auto shouldStartHostPass = hostPlayingNow
                                  && ! wasHostPlaying
                                  && ! shouldStartFullPass
                                  && ! shouldReset
                                  && ! fullPassActive.load(std::memory_order_relaxed);
    const auto shouldFinishHostPass = ! hostPlayingNow
                                   && wasHostPlaying
                                   && hostAutoPassActive.load(std::memory_order_relaxed)
                                   && ! shouldReset
                                   && ! shouldStartFullPass;

    if (shouldReset || shouldStartFullPass || shouldStartHostPass)
    {
        resetMeterState();

        if (shouldStartFullPass || shouldStartHostPass)
        {
            fullPassActive.store(true, std::memory_order_relaxed);
            fullPassCompleted.store(false, std::memory_order_relaxed);
            analysisFrozen.store(false, std::memory_order_relaxed);
            hostAutoPassActive.store(shouldStartHostPass, std::memory_order_relaxed);
            fullPassSeconds.store(0.0f, std::memory_order_relaxed);
        }
    }

    if (fullPassFinishRequested.exchange(false, std::memory_order_acq_rel) || shouldFinishHostPass)
    {
        fullPassActive.store(false, std::memory_order_relaxed);
        fullPassCompleted.store(processedSamplesSinceReset > 0, std::memory_order_relaxed);
        fullPassSeconds.store(static_cast<float>(static_cast<double>(processedSamplesSinceReset) / currentSampleRate),
                              std::memory_order_relaxed);
        analysisFrozen.store(true, std::memory_order_relaxed);
        hostAutoPassActive.store(false, std::memory_order_relaxed);
    }

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, numSamples);

    if (numSamples <= 0 || totalInputChannels <= 0)
        return;

    if (analysisFrozen.load(std::memory_order_relaxed))
    {
        applyAutoMaster(buffer, totalOutputChannels, numSamples);
        return;
    }

    const auto* left = buffer.getReadPointer(0);
    const auto* right = totalInputChannels > 1 ? buffer.getReadPointer(1) : left;

    auto sumLeft = 0.0;
    auto sumRight = 0.0;
    auto sumLeftSquares = 0.0;
    auto sumRightSquares = 0.0;
    auto sumLeftRight = 0.0;
    auto combinedSquares = 0.0;
    auto midSquares = 0.0;
    auto sideSquares = 0.0;
    auto monoSum = 0.0;
    std::array<double, fmma::bandCount> bandLeftSums {};
    std::array<double, fmma::bandCount> bandRightSums {};
    std::array<double, fmma::bandCount> bandLeftSquares {};
    std::array<double, fmma::bandCount> bandRightSquares {};
    std::array<double, fmma::bandCount> bandLeftRight {};
    std::array<double, fmma::bandCount> bandMidSquares {};
    std::array<double, fmma::bandCount> bandSideSquares {};

    auto monoPeak = 0.0f;
    auto clippedSamples = 0;
    auto silentSamples = 0;
    auto transientEvents = 0;
    auto transientEnergy = 0.0;
    auto transientAttackMsSum = 0.0;

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto l = left[sample];
        const auto r = right[sample];
        const auto mono = 0.5f * (l + r);
        const auto mid = mono;
        const auto side = 0.5f * (l - r);
        pushSpectrumSample(mono);
        auto weightedLeft = kWeightPreFilters[0].processSample(l);
        weightedLeft = kWeightHighPassFilters[0].processSample(weightedLeft);

        sumLeft += l;
        sumRight += r;
        sumLeftSquares += static_cast<double>(l) * l;
        sumRightSquares += static_cast<double>(r) * r;
        sumLeftRight += static_cast<double>(l) * r;
        combinedSquares += 0.5 * ((static_cast<double>(l) * l) + (static_cast<double>(r) * r));
        midSquares += static_cast<double>(mid) * mid;
        sideSquares += static_cast<double>(side) * side;
        monoSum += mono;

        for (auto band = 0; band < fmma::bandCount; ++band)
        {
            const auto bandIndex = static_cast<size_t>(band);
            const auto bandLeft = leftBandFilters[bandIndex].processSample(l);
            const auto bandRight = rightBandFilters[bandIndex].processSample(r);
            const auto bandMid = 0.5f * (bandLeft + bandRight);
            const auto bandSide = 0.5f * (bandLeft - bandRight);

            bandLeftSums[bandIndex] += bandLeft;
            bandRightSums[bandIndex] += bandRight;
            bandLeftSquares[bandIndex] += static_cast<double>(bandLeft) * bandLeft;
            bandRightSquares[bandIndex] += static_cast<double>(bandRight) * bandRight;
            bandLeftRight[bandIndex] += static_cast<double>(bandLeft) * bandRight;
            bandMidSquares[bandIndex] += static_cast<double>(bandMid) * bandMid;
            bandSideSquares[bandIndex] += static_cast<double>(bandSide) * bandSide;
        }

        monoPeak = juce::jmax(monoPeak, std::abs(mono));
        clippedSamples += std::abs(l) >= 0.999f ? 1 : 0;
        if (totalInputChannels > 1)
            clippedSamples += std::abs(r) >= 0.999f ? 1 : 0;
        silentSamples += std::abs(mono) <= 0.001f ? 1 : 0;

        const auto monoAbs = std::abs(mono);
        transientFastEnvelope += (monoAbs - transientFastEnvelope) * 0.28f;
        transientSlowEnvelope += (monoAbs - transientSlowEnvelope) * 0.012f;
        if (transientCooldownSamples > 0)
            --transientCooldownSamples;

        const auto transientRise = transientFastEnvelope - transientSlowEnvelope;
        if (transientCooldownSamples <= 0 && monoAbs > 0.02f && transientRise > (transientSlowEnvelope * 0.75f + 0.01f))
        {
            ++transientEvents;
            transientEnergy += static_cast<double>(mono) * mono;
            const auto sharpness = juce::jlimit(0.0f, 1.0f, transientRise / (transientSlowEnvelope + 0.04f));
            transientAttackMsSum += juce::jmap(sharpness, 36.0f, 4.0f);
            transientCooldownSamples = static_cast<int>(currentSampleRate * 0.045);
        }

        auto loudnessPower = static_cast<double>(weightedLeft) * weightedLeft;
        if (totalInputChannels > 1)
        {
            auto weightedRight = kWeightPreFilters[1].processSample(r);
            weightedRight = kWeightHighPassFilters[1].processSample(weightedRight);
            loudnessPower += static_cast<double>(weightedRight) * weightedRight;
        }

        pushLoudnessPower(loudnessPower);

        const auto phaseIndex = static_cast<size_t>(phaseFifoIndex);
        phaseFifoLeft[phaseIndex] = l;
        phaseFifoRight[phaseIndex] = r;
        ++phaseFifoIndex;
        if (phaseFifoIndex >= spectrumFftSize)
        {
            phaseFifoIndex = 0;
            if (totalInputChannels > 1)
                analysePhaseFrame();
        }
    }

    const auto n = static_cast<double>(numSamples);
    const auto leftPeak = peakOf(left, numSamples);
    const auto rightPeak = totalInputChannels > 1 ? peakOf(right, numSamples) : leftPeak;
    const auto maxPeak = juce::jmax(leftPeak, rightPeak);
    const auto rms = std::sqrt(combinedSquares / n);
    const auto peakDbValue = toDb(maxPeak);
    const auto rmsDbValue = toDb(static_cast<float>(rms));
    const auto truePeakDbValue = calculateTruePeakDb(buffer, totalInputChannels);
    const auto currentTruePeakHold = truePeakHoldDb.load(std::memory_order_relaxed);

    const auto numerator = (n * sumLeftRight) - (sumLeft * sumRight);
    const auto leftDen = (n * sumLeftSquares) - (sumLeft * sumLeft);
    const auto rightDen = (n * sumRightSquares) - (sumRight * sumRight);
    const auto denominator = std::sqrt(juce::jmax(0.0, leftDen * rightDen));
    const auto corr = denominator > 1.0e-12 ? juce::jlimit(-1.0f, 1.0f, static_cast<float>(numerator / denominator))
                                            : 1.0f;

    const auto midRms = std::sqrt(midSquares / n) + 1.0e-12;
    const auto sideRms = std::sqrt(sideSquares / n) + 1.0e-12;
    const auto msRatio = static_cast<float>(sideRms / midRms);
    const auto monoRmsDbValue = toDb(static_cast<float>(midRms));
    const auto monoLossDbValue = monoRmsDbValue - rmsDbValue;

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        const auto bandIndex = static_cast<size_t>(band);
        if (bandLeftSquares[bandIndex] + bandRightSquares[bandIndex] <= 1.0e-14)
        {
            publishMetric(bandCorrelations[bandIndex], 1.0f, 0.10f);
            publishMetric(bandSideRatiosDb[bandIndex], -120.0f, 0.10f);
            continue;
        }

        const auto bandNumerator = (n * bandLeftRight[bandIndex]) - (bandLeftSums[bandIndex] * bandRightSums[bandIndex]);
        const auto bandLeftDen = (n * bandLeftSquares[bandIndex]) - (bandLeftSums[bandIndex] * bandLeftSums[bandIndex]);
        const auto bandRightDen = (n * bandRightSquares[bandIndex]) - (bandRightSums[bandIndex] * bandRightSums[bandIndex]);
        const auto bandDenominator = std::sqrt(juce::jmax(0.0, bandLeftDen * bandRightDen));
        const auto bandCorr = bandDenominator > 1.0e-12
            ? juce::jlimit(-1.0f, 1.0f, static_cast<float>(bandNumerator / bandDenominator))
            : 1.0f;
        const auto bandMidRms = std::sqrt(bandMidSquares[bandIndex] / n) + 1.0e-12;
        const auto bandSideRms = std::sqrt(bandSideSquares[bandIndex] / n) + 1.0e-12;
        const auto bandSideRatio = static_cast<float>(bandSideRms / bandMidRms);

        publishMetric(bandCorrelations[bandIndex], bandCorr, 0.10f);
        publishMetric(bandSideRatiosDb[bandIndex], 20.0f * std::log10(juce::jmax(bandSideRatio, 1.0e-6f)), 0.10f);
    }

    const auto leftRms = std::sqrt(sumLeftSquares / n);
    const auto rightRms = std::sqrt(sumRightSquares / n);
    const auto balance = 20.0f * std::log10(static_cast<float>((leftRms + 1.0e-12) / (rightRms + 1.0e-12)));
    const auto momentaryMeanPower = momentaryPowerFilled > 0
        ? momentaryPowerSum / static_cast<double>(momentaryPowerFilled)
        : 0.0;
    const auto shortTermMeanPower = shortTermPowerFilled > 0
        ? shortTermPowerSum / static_cast<double>(shortTermPowerFilled)
        : 0.0;
    const auto shortTermLufsValue = powerToLufs(shortTermMeanPower);
    const auto channelSampleCount = static_cast<float>(numSamples * juce::jlimit(1, 2, totalInputChannels));
    const auto clippedPercentValue = channelSampleCount > 0.0f
        ? (static_cast<float>(clippedSamples) / channelSampleCount) * 100.0f
        : 0.0f;
    const auto silencePercentValue = numSamples > 0
        ? (static_cast<float>(silentSamples) / static_cast<float>(numSamples)) * 100.0f
        : 0.0f;
    const auto blockSeconds = static_cast<float>(n / currentSampleRate);
    const auto transientDensityValue = blockSeconds > 0.0f ? static_cast<float>(transientEvents) / blockSeconds : 0.0f;
    const auto attackTimeValue = transientEvents > 0 ? static_cast<float>(transientAttackMsSum / transientEvents)
                                                     : rollingAttackTimeMs;
    const auto percussionEnergyValue = combinedSquares > 1.0e-18
        ? static_cast<float>((transientEnergy / combinedSquares) * 100.0)
        : 0.0f;
    const auto transientAlpha = smoothingForWindow(blockSeconds, 4.0f);
    rollingTransientDensity += (transientDensityValue - rollingTransientDensity) * transientAlpha;
    rollingPercussionEnergyPct += (percussionEnergyValue - rollingPercussionEnergyPct) * transientAlpha;
    if (transientEvents > 0)
    {
        const auto attackAlpha = rollingAttackTimeMs <= 0.0f ? 1.0f : transientAlpha;
        rollingAttackTimeMs += (attackTimeValue - rollingAttackTimeMs) * attackAlpha;
    }
    processedSamplesSinceReset += static_cast<juce::uint64>(numSamples);

    this->momentaryLufs.store(powerToLufs(momentaryMeanPower), std::memory_order_relaxed);
    this->shortTermLufs.store(shortTermLufsValue, std::memory_order_relaxed);
    if (integratedLoudnessDirty)
    {
        this->integratedLufs.store(powerToLufs(calculateIntegratedLoudnessPower()), std::memory_order_relaxed);
        integratedLoudnessDirty = false;
    }
    if (shortTermPowerFilled == shortTermPowerRing.size() && shortTermLufsValue > -119.0f)
        this->lraLu.store(calculateLraLu(), std::memory_order_relaxed);

    this->truePeakDb.store(truePeakDbValue, std::memory_order_relaxed);
    if (truePeakDbValue > currentTruePeakHold)
        truePeakHoldDb.store(truePeakDbValue, std::memory_order_relaxed);
    updateMaximum(worstTruePeakDb, truePeakDbValue);
    publishMetric(this->rmsDb, rmsDbValue, 0.14f);
    publishMetric(this->peakDb, peakDbValue, 0.28f);
    publishMetric(this->crestDb, peakDbValue - rmsDbValue, 0.12f);
    publishMetric(this->leftPeakDb, toDb(leftPeak), 0.28f);
    publishMetric(this->rightPeakDb, toDb(rightPeak), 0.28f);
    publishMetric(this->monoPeakDb, toDb(monoPeak), 0.28f);
    publishMetric(this->monoRmsDb, monoRmsDbValue, 0.14f);
    publishMetric(this->monoLossDb, monoLossDbValue, 0.10f);
    publishMetric(this->correlation, corr, 0.08f);
    updateMinimum(worstCorrelation, corr);
    updateMinimum(worstMonoLossDb, monoLossDbValue);
    publishMetric(this->widthPct, msRatio * 100.0f, 0.08f);
    publishMetric(this->msRatioDb, 20.0f * std::log10(juce::jmax(msRatio, 1.0e-6f)), 0.08f);
    publishMetric(this->stereoBalanceDb, balance, 0.10f);
    publishMetric(this->dcOffset, static_cast<float>(monoSum / n), 0.06f);
    this->clippedPercent.store(clippedPercentValue, std::memory_order_relaxed);
    updateMaximum(worstClippedPercent, clippedPercentValue);
    this->silencePercent.store(silencePercentValue, std::memory_order_relaxed);
    publishMetric(this->transientDensity, rollingTransientDensity, 1.0f);
    publishMetric(this->attackTimeMs, rollingAttackTimeMs, 1.0f);
    publishMetric(this->percussionEnergyPct, rollingPercussionEnergyPct, 1.0f);
    analysisSeconds.store(static_cast<float>(static_cast<double>(processedSamplesSinceReset) / currentSampleRate),
                          std::memory_order_relaxed);
    if (fullPassActive.load(std::memory_order_relaxed))
        fullPassSeconds.store(static_cast<float>(static_cast<double>(processedSamplesSinceReset) / currentSampleRate),
                              std::memory_order_relaxed);

    applyAutoMaster(buffer, totalOutputChannels, numSamples);
}

fmma::AnalyzerMetrics FunkyMooseMixAnalyzerAudioProcessor::getMetrics() const
{
    fmma::AnalyzerMetrics metrics;
    metrics.momentaryLufs = momentaryLufs.load(std::memory_order_relaxed);
    metrics.shortTermLufs = shortTermLufs.load(std::memory_order_relaxed);
    metrics.integratedLufs = integratedLufs.load(std::memory_order_relaxed);
    metrics.truePeakDb = truePeakDb.load(std::memory_order_relaxed);
    metrics.truePeakHoldDb = truePeakHoldDb.load(std::memory_order_relaxed);
    metrics.worstTruePeakDb = worstTruePeakDb.load(std::memory_order_relaxed);
    metrics.lraLu = lraLu.load(std::memory_order_relaxed);
    metrics.rmsDb = rmsDb.load(std::memory_order_relaxed);
    metrics.peakDb = peakDb.load(std::memory_order_relaxed);
    metrics.crestDb = crestDb.load(std::memory_order_relaxed);
    metrics.leftPeakDb = leftPeakDb.load(std::memory_order_relaxed);
    metrics.rightPeakDb = rightPeakDb.load(std::memory_order_relaxed);
    metrics.monoPeakDb = monoPeakDb.load(std::memory_order_relaxed);
    metrics.monoRmsDb = monoRmsDb.load(std::memory_order_relaxed);
    metrics.monoLossDb = monoLossDb.load(std::memory_order_relaxed);
    metrics.correlation = correlation.load(std::memory_order_relaxed);
    metrics.worstCorrelation = worstCorrelation.load(std::memory_order_relaxed);
    metrics.worstMonoLossDb = worstMonoLossDb.load(std::memory_order_relaxed);
    metrics.widthPct = widthPct.load(std::memory_order_relaxed);
    metrics.msRatioDb = msRatioDb.load(std::memory_order_relaxed);
    metrics.stereoBalanceDb = stereoBalanceDb.load(std::memory_order_relaxed);
    metrics.dcOffset = dcOffset.load(std::memory_order_relaxed);
    metrics.clippedPercent = clippedPercent.load(std::memory_order_relaxed);
    metrics.worstClippedPercent = worstClippedPercent.load(std::memory_order_relaxed);
    metrics.silencePercent = silencePercent.load(std::memory_order_relaxed);
    metrics.transientDensity = transientDensity.load(std::memory_order_relaxed);
    metrics.attackTimeMs = attackTimeMs.load(std::memory_order_relaxed);
    metrics.percussionEnergyPct = percussionEnergyPct.load(std::memory_order_relaxed);
    metrics.spectralCentroidHz = spectralCentroidHz.load(std::memory_order_relaxed);
    metrics.spectralRolloffHz = spectralRolloffHz.load(std::memory_order_relaxed);
    metrics.resonanceFreqHz = resonanceFreqHz.load(std::memory_order_relaxed);
    metrics.resonanceGainDb = resonanceGainDb.load(std::memory_order_relaxed);
    metrics.worstResonanceFreqHz = worstResonanceFreqHz.load(std::memory_order_relaxed);
    metrics.worstResonanceGainDb = worstResonanceGainDb.load(std::memory_order_relaxed);
    metrics.worstLowMidPercent = worstLowMidPercent.load(std::memory_order_relaxed);
    metrics.phaseCorrelation = phaseCorrelation.load(std::memory_order_relaxed);
    metrics.analysisSeconds = analysisSeconds.load(std::memory_order_relaxed);
    metrics.fullPassSeconds = fullPassSeconds.load(std::memory_order_relaxed);
    metrics.fullPassActive = fullPassActive.load(std::memory_order_relaxed);
    metrics.fullPassCompleted = fullPassCompleted.load(std::memory_order_relaxed);
    metrics.analysisFrozen = analysisFrozen.load(std::memory_order_relaxed);
    metrics.hostTransportPlaying = hostTransportPlaying.load(std::memory_order_relaxed);
    metrics.hostAutoPassActive = hostAutoPassActive.load(std::memory_order_relaxed);
    metrics.autoMasterEnabled = parameters.getRawParameterValue("autoMasterEnabled")->load() >= 0.5f;
    metrics.autoMasterStrength = autoMasterStrengthPct.load(std::memory_order_relaxed);
    metrics.autoMasterTargetLufs = autoMasterTargetLufs.load(std::memory_order_relaxed);
    metrics.autoMasterCeilingDbTp = autoMasterCeilingDbTp.load(std::memory_order_relaxed);
    metrics.autoMasterGainDb = autoMasterGainDb.load(std::memory_order_relaxed);
    metrics.autoMasterLowShelfDb = autoMasterLowShelfDb.load(std::memory_order_relaxed);
    metrics.autoMasterPresenceDb = autoMasterPresenceDb.load(std::memory_order_relaxed);
    metrics.autoMasterAirShelfDb = autoMasterAirShelfDb.load(std::memory_order_relaxed);
    metrics.autoMasterWidthPercent = autoMasterWidthPercent.load(std::memory_order_relaxed);
    metrics.autoMasterGlueReductionDb = autoMasterGlueReductionDb.load(std::memory_order_relaxed);
    metrics.autoMasterLimiterReductionDb = autoMasterLimiterReductionDb.load(std::memory_order_relaxed);
    metrics.autoMasterProjectedLufs = autoMasterProjectedLufs.load(std::memory_order_relaxed);
    metrics.autoMasterProjectedTruePeakDbTp = autoMasterProjectedTruePeakDbTp.load(std::memory_order_relaxed);
    metrics.autoMasterLoudnessMatchGainDb = autoMasterLoudnessMatchGainDb.load(std::memory_order_relaxed);
    metrics.autoMasterLufsDeltaDb = autoMasterLufsDeltaDb.load(std::memory_order_relaxed);
    metrics.autoMasterTruePeakMarginDb = autoMasterTruePeakMarginDb.load(std::memory_order_relaxed);
    metrics.autoMasterReleaseScore = autoMasterReleaseScore.load(std::memory_order_relaxed);
    metrics.autoMasterAbLoudnessDeltaDb = autoMasterAbLoudnessDeltaDb.load(std::memory_order_relaxed);
    metrics.autoMasterAbTruePeakDbTp = autoMasterAbTruePeakDbTp.load(std::memory_order_relaxed);
    metrics.autoMasterAbTruePeakDeltaDb = autoMasterAbTruePeakDeltaDb.load(std::memory_order_relaxed);
    metrics.autoMasterAbDynamicsDeltaDb = autoMasterAbDynamicsDeltaDb.load(std::memory_order_relaxed);
    metrics.autoMasterAbScore = autoMasterAbScore.load(std::memory_order_relaxed);

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        metrics.bandPercents[static_cast<size_t>(band)] = bandPercents[static_cast<size_t>(band)].load(std::memory_order_relaxed);
        metrics.bandCorrelations[static_cast<size_t>(band)] = bandCorrelations[static_cast<size_t>(band)].load(std::memory_order_relaxed);
        metrics.bandSideRatiosDb[static_cast<size_t>(band)] = bandSideRatiosDb[static_cast<size_t>(band)].load(std::memory_order_relaxed);
    }

    return metrics;
}

bool FunkyMooseMixAnalyzerAudioProcessor::getStoredReferenceMetrics(fmma::AnalyzerMetrics& target) const
{
    const juce::ScopedLock lock(storedAnalysisLock);
    if (! hasStoredReferenceMetrics)
        return false;

    target = storedReferenceMetrics;
    return true;
}

bool FunkyMooseMixAnalyzerAudioProcessor::getStoredSnapshotA(fmma::AnalyzerMetrics& target) const
{
    const juce::ScopedLock lock(storedAnalysisLock);
    if (! hasStoredSnapshotA)
        return false;

    target = storedSnapshotA;
    return true;
}

bool FunkyMooseMixAnalyzerAudioProcessor::getStoredSnapshotB(fmma::AnalyzerMetrics& target) const
{
    const juce::ScopedLock lock(storedAnalysisLock);
    if (! hasStoredSnapshotB)
        return false;

    target = storedSnapshotB;
    return true;
}

void FunkyMooseMixAnalyzerAudioProcessor::storeReferenceMetrics(const fmma::AnalyzerMetrics& source)
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedReferenceMetrics = source;
        hasStoredReferenceMetrics = true;
    }

#ifndef MIX_ANALYZER_HEADLESS_TESTS
    updateHostDisplay();
#endif
}

void FunkyMooseMixAnalyzerAudioProcessor::storeSnapshotA(const fmma::AnalyzerMetrics& source)
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedSnapshotA = source;
        hasStoredSnapshotA = true;
    }

#ifndef MIX_ANALYZER_HEADLESS_TESTS
    updateHostDisplay();
#endif
}

void FunkyMooseMixAnalyzerAudioProcessor::storeSnapshotB(const fmma::AnalyzerMetrics& source)
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedSnapshotB = source;
        hasStoredSnapshotB = true;
    }

#ifndef MIX_ANALYZER_HEADLESS_TESTS
    updateHostDisplay();
#endif
}

void FunkyMooseMixAnalyzerAudioProcessor::clearStoredReferenceMetrics()
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedReferenceMetrics = {};
        hasStoredReferenceMetrics = false;
    }

#ifndef MIX_ANALYZER_HEADLESS_TESTS
    updateHostDisplay();
#endif
}

void FunkyMooseMixAnalyzerAudioProcessor::clearStoredSnapshots()
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedSnapshotA = {};
        storedSnapshotB = {};
        hasStoredSnapshotA = false;
        hasStoredSnapshotB = false;
    }

#ifndef MIX_ANALYZER_HEADLESS_TESTS
    updateHostDisplay();
#endif
}

void FunkyMooseMixAnalyzerAudioProcessor::appendStoredAnalysisState(juce::ValueTree& state) const
{
    auto existing = state.getChildWithName(storedAnalysisStateId);
    if (existing.isValid())
        state.removeChild(existing, nullptr);

    juce::ValueTree storedState { storedAnalysisStateId };
    storedState.setProperty("version", 1, nullptr);

    const juce::ScopedLock lock(storedAnalysisLock);
    storedState.setProperty("hasReferenceMetrics", hasStoredReferenceMetrics, nullptr);
    storedState.setProperty("hasSnapshotA", hasStoredSnapshotA, nullptr);
    storedState.setProperty("hasSnapshotB", hasStoredSnapshotB, nullptr);

    if (hasStoredReferenceMetrics)
        storedState.addChild(metricsToValueTree(referenceMetricsId, storedReferenceMetrics), -1, nullptr);
    if (hasStoredSnapshotA)
        storedState.addChild(metricsToValueTree(snapshotAId, storedSnapshotA), -1, nullptr);
    if (hasStoredSnapshotB)
        storedState.addChild(metricsToValueTree(snapshotBId, storedSnapshotB), -1, nullptr);

    state.addChild(storedState, -1, nullptr);
}

void FunkyMooseMixAnalyzerAudioProcessor::restoreStoredAnalysisState(const juce::ValueTree& state)
{
    const auto storedState = state.getChildWithName(storedAnalysisStateId);

    if (! storedState.isValid())
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        hasStoredReferenceMetrics = false;
        hasStoredSnapshotA = false;
        hasStoredSnapshotB = false;
        storedReferenceMetrics = {};
        storedSnapshotA = {};
        storedSnapshotB = {};
        return;
    }

    const auto reference = storedState.getChildWithName(referenceMetricsId);
    const auto snapshotA = storedState.getChildWithName(snapshotAId);
    const auto snapshotB = storedState.getChildWithName(snapshotBId);

    const juce::ScopedLock lock(storedAnalysisLock);
    hasStoredReferenceMetrics = storedState.getProperty("hasReferenceMetrics", false) && reference.isValid();
    hasStoredSnapshotA = storedState.getProperty("hasSnapshotA", false) && snapshotA.isValid();
    hasStoredSnapshotB = storedState.getProperty("hasSnapshotB", false) && snapshotB.isValid();

    storedReferenceMetrics = hasStoredReferenceMetrics ? valueTreeToMetrics(reference) : fmma::AnalyzerMetrics {};
    storedSnapshotA = hasStoredSnapshotA ? valueTreeToMetrics(snapshotA) : fmma::AnalyzerMetrics {};
    storedSnapshotB = hasStoredSnapshotB ? valueTreeToMetrics(snapshotB) : fmma::AnalyzerMetrics {};
}

void FunkyMooseMixAnalyzerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    appendStoredAnalysisState(state);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FunkyMooseMixAnalyzerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
    {
        const auto state = juce::ValueTree::fromXml(*xmlState);
        restoreStoredAnalysisState(state);

        auto parameterState = state.createCopy();
        if (auto storedState = parameterState.getChildWithName(storedAnalysisStateId); storedState.isValid())
            parameterState.removeChild(storedState, nullptr);

        parameters.replaceState(parameterState);
    }
}

juce::AudioProcessorEditor* FunkyMooseMixAnalyzerAudioProcessor::createEditor()
{
#ifdef MIX_ANALYZER_HEADLESS_TESTS
    return nullptr;
#else
    return new FunkyMooseMixAnalyzerAudioProcessorEditor(*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FunkyMooseMixAnalyzerAudioProcessor();
}
