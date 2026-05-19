#pragma once

#include "AnalysisModel.h"
#include "IPC/OscSender.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class FunkyMooseMixAnalyzerAudioProcessor final : public juce::AudioProcessor,
                                                 private juce::Timer
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

    const juce::String getName() const override { return "Funky Moose Mix Analyzer"; }
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

    fmma::MixAssessmentInput makeAssessmentInput() const;

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void configureBandFilters(double newSampleRate, int samplesPerBlock);
    void timerCallback() override;
    void configureLoudnessMeter(double newSampleRate, int samplesPerBlock);
    void pushLoudnessPower(double power) noexcept;
    void addIntegratedLoudnessBlock(double meanPower) noexcept;
    void addLraShortTermBlock(double meanPower) noexcept;
    double calculateIntegratedLoudnessPower() const noexcept;
    float calculateLraLu() const noexcept;
    float calculateTruePeakDb(const juce::AudioBuffer<float>& buffer, int channelCount) noexcept;
    void pushSpectrumSample(float sample) noexcept;
    void analyseSpectrumFrame() noexcept;
    void analysePhaseFrame() noexcept;
    void resetMeterState();
    void resetAutoMasterState() noexcept;
    void applyAutoMaster(juce::AudioBuffer<float>& buffer, int channelCount, int numSamples) noexcept;
    void publishMetric(std::atomic<float>& target, float value, float smoothing = 0.25f) noexcept;
    void appendStoredAnalysisState(juce::ValueTree& state) const;
    void restoreStoredAnalysisState(const juce::ValueTree& state);
    static int lraBinFor(float lufs) noexcept;
    static float lraBinCenter(int bin) noexcept;

    struct AutoMasterBiquad
    {
        void reset() noexcept;
        void setIdentity() noexcept;
        void setLowShelf(double sampleRate, float frequencyHz, float gainDb) noexcept;
        void setHighShelf(double sampleRate, float frequencyHz, float gainDb) noexcept;
        void setPeak(double sampleRate, float frequencyHz, float q, float gainDb) noexcept;
        float process(float sample) noexcept;

        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    double currentSampleRate = 48000.0;
    std::array<juce::dsp::IIR::Filter<float>, fmma::bandCount> leftBandFilters;
    std::array<juce::dsp::IIR::Filter<float>, fmma::bandCount> rightBandFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> kWeightPreFilters;
    std::array<juce::dsp::IIR::Filter<float>, 2> kWeightHighPassFilters;
    std::array<AutoMasterBiquad, 2> autoMasterLowShelfFilters;
    std::array<AutoMasterBiquad, 2> autoMasterPresenceFilters;
    std::array<AutoMasterBiquad, 2> autoMasterAirShelfFilters;

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

    juce::dsp::FFT phaseFft { spectrumFftOrder };
    std::array<float, spectrumFftSize> phaseFifoLeft {};
    std::array<float, spectrumFftSize> phaseFifoRight {};
    std::array<float, spectrumFftSize * 2> phaseDataLeft {};
    std::array<float, spectrumFftSize * 2> phaseDataRight {};
    int phaseFifoIndex = 0;

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
    std::atomic<float> phaseCorrelation {1.0f};
    std::atomic<float> analysisSeconds {0.0f};
    std::atomic<float> autoMasterStrengthPct {0.0f};
    std::atomic<float> autoMasterTargetLufs {-14.0f};
    std::atomic<float> autoMasterCeilingDbTp {-1.0f};
    std::atomic<float> autoMasterGainDb {0.0f};
    std::atomic<float> autoMasterLowShelfDb {0.0f};
    std::atomic<float> autoMasterPresenceDb {0.0f};
    std::atomic<float> autoMasterAirShelfDb {0.0f};
    std::atomic<float> autoMasterWidthPercent {100.0f};
    std::atomic<float> autoMasterGlueReductionDb {0.0f};
    std::atomic<float> autoMasterLimiterReductionDb {0.0f};
    std::atomic<float> autoMasterProjectedLufs {-120.0f};
    std::atomic<float> autoMasterProjectedTruePeakDbTp {-120.0f};
    std::atomic<float> autoMasterLoudnessMatchGainDb {0.0f};
    std::atomic<float> autoMasterLufsDeltaDb {0.0f};
    std::atomic<float> autoMasterTruePeakMarginDb {0.0f};
    std::atomic<float> autoMasterReleaseScore {0.0f};
    std::array<std::atomic<float>, fmma::bandCount> bandPercents {};
    std::array<std::atomic<float>, fmma::bandCount> bandCorrelations {};
    std::array<std::atomic<float>, fmma::bandCount> bandSideRatiosDb {};

    float smoothedAutoMasterGainDb = 0.0f;
    float smoothedAutoMasterLowShelfDb = 0.0f;
    float smoothedAutoMasterPresenceDb = 0.0f;
    float smoothedAutoMasterAirShelfDb = 0.0f;
    float smoothedAutoMasterSideGain = 1.0f;
    float smoothedAutoMasterGlueDepthDb = 0.0f;
    float autoMasterGlueGain = 1.0f;
    float autoMasterLimiterGain = 1.0f;
    float displayedAutoMasterGlueReductionDb = 0.0f;
    float displayedAutoMasterLimiterReductionDb = 0.0f;

    mutable juce::CriticalSection storedAnalysisLock;
    fmma::AnalyzerMetrics storedReferenceMetrics;
    fmma::AnalyzerMetrics storedSnapshotA;
    fmma::AnalyzerMetrics storedSnapshotB;
    bool hasStoredReferenceMetrics = false;
    bool hasStoredSnapshotA = false;
    bool hasStoredSnapshotB = false;

    OscSender oscSender;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseMixAnalyzerAudioProcessor)
};
