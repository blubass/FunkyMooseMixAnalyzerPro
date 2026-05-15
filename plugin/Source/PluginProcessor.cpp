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

    return { params.begin(), params.end() };
}

void FunkyMooseMixAnalyzerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    configureBandFilters(currentSampleRate);
    configureLoudnessMeter(currentSampleRate, samplesPerBlock);
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

void FunkyMooseMixAnalyzerAudioProcessor::configureBandFilters(double newSampleRate)
{
    juce::dsp::ProcessSpec spec { newSampleRate, 512, 1 };

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
    const auto blockSize = static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock));
    juce::dsp::ProcessSpec monoSpec { newSampleRate, blockSize, 1 };

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
    truePeakOversampler->initProcessing(static_cast<size_t>(blockSize));
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

void FunkyMooseMixAnalyzerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                       juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    auto hostPlayingNow = false;
    if (auto* playHead = getPlayHead())
    {
        const auto position = playHead->getPosition();
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

    if (analysisFrozen.load(std::memory_order_relaxed))
        return;

    if (numSamples <= 0 || totalInputChannels <= 0)
        return;

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
        phaseFifoIndex = (phaseFifoIndex + 1) % spectrumFftSize;
    }

    if (phaseFifoIndex == 0 && totalInputChannels > 1)
    {
        std::copy(phaseFifoLeft.begin(), phaseFifoLeft.end(), phaseDataLeft.begin());
        std::copy(phaseFifoRight.begin(), phaseFifoRight.end(), phaseDataRight.begin());

        phaseFft.performRealOnlyForwardTransform(phaseDataLeft.data());
        phaseFft.performRealOnlyForwardTransform(phaseDataRight.data());

        double phaseCorrSum = 0.0;
        int binCount = 0;
        for (int i = 0; i < spectrumFftSize / 2; ++i)
        {
            const auto bin = static_cast<size_t>(i);
            const auto imaginaryBin = static_cast<size_t>(i + spectrumFftSize / 2);
            const auto leftReal = phaseDataLeft[bin];
            const auto leftImag = phaseDataLeft[imaginaryBin];
            const auto rightReal = phaseDataRight[bin];
            const auto rightImag = phaseDataRight[imaginaryBin];

            const auto leftMag = std::sqrt(leftReal * leftReal + leftImag * leftImag);
            const auto rightMag = std::sqrt(rightReal * rightReal + rightImag * rightImag);

            if (leftMag > 1e-6 && rightMag > 1e-6)
            {
                const auto leftPhase = std::atan2(leftImag, leftReal);
                const auto rightPhase = std::atan2(rightImag, rightReal);
                const auto phaseDiff = leftPhase - rightPhase;
                phaseCorrSum += std::cos(phaseDiff);
                ++binCount;
            }
        }
        const auto phaseCorr = binCount > 0 ? phaseCorrSum / binCount : 1.0;
        phaseCorrelation.store(static_cast<float>(phaseCorr), std::memory_order_relaxed);
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

    updateHostDisplay();
}

void FunkyMooseMixAnalyzerAudioProcessor::storeSnapshotA(const fmma::AnalyzerMetrics& source)
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedSnapshotA = source;
        hasStoredSnapshotA = true;
    }

    updateHostDisplay();
}

void FunkyMooseMixAnalyzerAudioProcessor::storeSnapshotB(const fmma::AnalyzerMetrics& source)
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedSnapshotB = source;
        hasStoredSnapshotB = true;
    }

    updateHostDisplay();
}

void FunkyMooseMixAnalyzerAudioProcessor::clearStoredReferenceMetrics()
{
    {
        const juce::ScopedLock lock(storedAnalysisLock);
        storedReferenceMetrics = {};
        hasStoredReferenceMetrics = false;
    }

    updateHostDisplay();
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

    updateHostDisplay();
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
