#pragma once

#include "AnalysisModel.h"

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace fmma
{
struct AnalyzerMetrics
{
    float momentaryLufs = -120.0f;
    float shortTermLufs = -120.0f;
    float integratedLufs = -120.0f;
    float truePeakDb = -120.0f;
    float truePeakHoldDb = -120.0f;
    float worstTruePeakDb = -120.0f;
    float lraLu = 0.0f;
    float rmsDb = -120.0f;
    float peakDb = -120.0f;
    float crestDb = 0.0f;
    float leftPeakDb = -120.0f;
    float rightPeakDb = -120.0f;
    float monoPeakDb = -120.0f;
    float monoRmsDb = -120.0f;
    float monoLossDb = 0.0f;
    float correlation = 1.0f;
    float worstCorrelation = 1.0f;
    float worstMonoLossDb = 0.0f;
    float widthPct = 0.0f;
    float msRatioDb = 0.0f;
    float stereoBalanceDb = 0.0f;
    float dcOffset = 0.0f;
    float clippedPercent = 0.0f;
    float worstClippedPercent = 0.0f;
    float silencePercent = 0.0f;
    float transientDensity = 0.0f;
    float attackTimeMs = 0.0f;
    float percussionEnergyPct = 0.0f;
    float spectralCentroidHz = 0.0f;
    float spectralRolloffHz = 0.0f;
    float resonanceFreqHz = 0.0f;
    float resonanceGainDb = 0.0f;
    float worstResonanceFreqHz = 0.0f;
    float worstResonanceGainDb = 0.0f;
    float worstLowMidPercent = 0.0f;
    float analysisSeconds = 0.0f;
    float fullPassSeconds = 0.0f;
    bool fullPassActive = false;
    bool fullPassCompleted = false;
    bool analysisFrozen = false;
    bool hostTransportPlaying = false;
    bool hostAutoPassActive = false;
    std::array<float, bandCount> bandPercents {};
    std::array<float, bandCount> bandCorrelations { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<float, bandCount> bandSideRatiosDb { -120.0f, -120.0f, -120.0f, -120.0f, -120.0f, -120.0f };
};
}

class FunkyMooseMixAnalyzerAudioProcessor final : public juce::AudioProcessor
{
public:
    FunkyMooseMixAnalyzerAudioProcessor();
    ~FunkyMooseMixAnalyzerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override
    {
        juce::ignoreUnused(index);
        return {};
    }
    void changeProgramName(int index, const juce::String& newName) override
    {
        juce::ignoreUnused(index, newName);
    }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    fmma::AnalyzerMetrics getMetrics() const;
    bool getStoredReferenceMetrics(fmma::AnalyzerMetrics& target) const;
    bool getStoredSnapshotA(fmma::AnalyzerMetrics& target) const;
    bool getStoredSnapshotB(fmma::AnalyzerMetrics& target) const;
    void storeReferenceMetrics(const fmma::AnalyzerMetrics& source);
    void storeSnapshotA(const fmma::AnalyzerMetrics& source);
    void storeSnapshotB(const fmma::AnalyzerMetrics& source);
    void clearStoredReferenceMetrics();
    void clearStoredSnapshots();
    void requestAnalyzerReset() noexcept;
    void requestFullPassStart() noexcept;
    void requestFullPassFinish() noexcept;

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void configureBandFilters(double newSampleRate);
    void configureLoudnessMeter(double newSampleRate, int samplesPerBlock);
    void pushLoudnessPower(double power) noexcept;
    void addIntegratedLoudnessBlock(double meanPower) noexcept;
    void addLraShortTermBlock(double meanPower) noexcept;
    double calculateIntegratedLoudnessPower() const noexcept;
    float calculateLraLu() const noexcept;
    float calculateTruePeakDb(const juce::AudioBuffer<float>& buffer, int channelCount) noexcept;
    void pushSpectrumSample(float sample) noexcept;
    void analyseSpectrumFrame() noexcept;
    void resetMeterState();
    void publishMetric(std::atomic<float>& target, float value, float smoothing = 0.25f) noexcept;
    void appendStoredAnalysisState(juce::ValueTree& state) const;
    void restoreStoredAnalysisState(const juce::ValueTree& state);
    static int lraBinFor(float lufs) noexcept;
    static float lraBinCenter(int bin) noexcept;

    double currentSampleRate = 48000.0;
    std::array<juce::dsp::IIR::Filter<float>, fmma::bandCount> leftBandFilters;
    std::array<juce::dsp::IIR::Filter<float>, fmma::bandCount> rightBandFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> kWeightPreFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> kWeightHighPassFilters;

    std::vector<double> momentaryPowerRing;
    std::vector<double> shortTermPowerRing;
    size_t momentaryPowerIndex = 0;
    size_t shortTermPowerIndex = 0;
    size_t momentaryPowerFilled = 0;
    size_t shortTermPowerFilled = 0;
    double momentaryPowerSum = 0.0;
    double shortTermPowerSum = 0.0;
    int integratedHopSamples = 4800;
    int integratedSamplesUntilHop = 4800;
    bool integratedLoudnessDirty = false;

    static constexpr size_t integratedBlockCapacity = 18000;
    static constexpr int lraHistogramBinCount = 801;
    static constexpr float lraHistogramMinLufs = -70.0f;
    static constexpr float lraHistogramStepLu = 0.1f;
    static constexpr int spectrumFftOrder = 12;
    static constexpr int spectrumFftSize = 1 << spectrumFftOrder;

    std::array<float, integratedBlockCapacity> integratedBlockPowers {};
    size_t integratedBlockCount = 0;
    size_t integratedBlockWriteIndex = 0;
    std::array<float, integratedBlockCapacity> lraBlockPowers {};
    std::array<int, integratedBlockCapacity> lraBlockBins {};
    std::array<int, lraHistogramBinCount> lraHistogram {};
    size_t lraBlockCount = 0;
    size_t lraBlockWriteIndex = 0;
    double lraPowerSum = 0.0;
    juce::uint64 processedSamplesSinceReset = 0;

    std::unique_ptr<juce::dsp::Oversampling<float>> truePeakOversampler;
    int truePeakChannelCount = 2;
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> fullPassStartRequested { false };
    std::atomic<bool> fullPassFinishRequested { false };
    std::atomic<bool> fullPassActive { false };
    std::atomic<bool> fullPassCompleted { false };
    std::atomic<bool> analysisFrozen { false };
    std::atomic<bool> hostTransportPlaying { false };
    std::atomic<bool> hostAutoPassActive { false };
    std::atomic<float> fullPassSeconds {0.0f};
    float transientFastEnvelope = 0.0f;
    float transientSlowEnvelope = 0.0f;
    int transientCooldownSamples = 0;
    float rollingTransientDensity = 0.0f;
    float rollingAttackTimeMs = 0.0f;
    float rollingPercussionEnergyPct = 0.0f;
    juce::dsp::FFT spectrumFft { spectrumFftOrder };
    juce::dsp::WindowingFunction<float> spectrumWindow {
        spectrumFftSize,
        juce::dsp::WindowingFunction<float>::hann,
        false
    };
    std::array<float, spectrumFftSize> spectrumFifo {};
    std::array<float, spectrumFftSize * 2> spectrumData {};
    int spectrumFifoIndex = 0;

    std::atomic<float> momentaryLufs {-120.0f};
    std::atomic<float> shortTermLufs {-120.0f};
    std::atomic<float> integratedLufs {-120.0f};
    std::atomic<float> truePeakDb {-120.0f};
    std::atomic<float> truePeakHoldDb {-120.0f};
    std::atomic<float> worstTruePeakDb {-120.0f};
    std::atomic<float> lraLu {0.0f};
    std::atomic<float> rmsDb {-120.0f};
    std::atomic<float> peakDb {-120.0f};
    std::atomic<float> crestDb {0.0f};
    std::atomic<float> leftPeakDb {-120.0f};
    std::atomic<float> rightPeakDb {-120.0f};
    std::atomic<float> monoPeakDb {-120.0f};
    std::atomic<float> monoRmsDb {-120.0f};
    std::atomic<float> monoLossDb {0.0f};
    std::atomic<float> correlation {1.0f};
    std::atomic<float> worstCorrelation {1.0f};
    std::atomic<float> worstMonoLossDb {0.0f};
    std::atomic<float> widthPct {0.0f};
    std::atomic<float> msRatioDb {0.0f};
    std::atomic<float> stereoBalanceDb {0.0f};
    std::atomic<float> dcOffset {0.0f};
    std::atomic<float> clippedPercent {0.0f};
    std::atomic<float> worstClippedPercent {0.0f};
    std::atomic<float> silencePercent {0.0f};
    std::atomic<float> transientDensity {0.0f};
    std::atomic<float> attackTimeMs {0.0f};
    std::atomic<float> percussionEnergyPct {0.0f};
    std::atomic<float> spectralCentroidHz {0.0f};
    std::atomic<float> spectralRolloffHz {0.0f};
    std::atomic<float> resonanceFreqHz {0.0f};
    std::atomic<float> resonanceGainDb {0.0f};
    std::atomic<float> worstResonanceFreqHz {0.0f};
    std::atomic<float> worstResonanceGainDb {0.0f};
    std::atomic<float> worstLowMidPercent {0.0f};
    std::atomic<float> analysisSeconds {0.0f};
    std::array<std::atomic<float>, fmma::bandCount> bandPercents {};
    std::array<std::atomic<float>, fmma::bandCount> bandCorrelations {};
    std::array<std::atomic<float>, fmma::bandCount> bandSideRatiosDb {};

    mutable juce::CriticalSection storedAnalysisLock;
    fmma::AnalyzerMetrics storedReferenceMetrics;
    fmma::AnalyzerMetrics storedSnapshotA;
    fmma::AnalyzerMetrics storedSnapshotB;
    bool hasStoredReferenceMetrics = false;
    bool hasStoredSnapshotA = false;
    bool hasStoredSnapshotB = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseMixAnalyzerAudioProcessor)
};
