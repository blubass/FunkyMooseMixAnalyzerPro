#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace fmma
{
constexpr int bandCount = 6;
constexpr std::array<const char*, bandCount> bandNames {{
    "Sub", "Bass", "Low-Mids", "Mids", "Presence", "Air"
}};

struct Range
{
    float low = 0.0f;
    float high = 0.0f;

    bool contains(float value) const noexcept { return value >= low && value <= high; }
};

struct GenreProfile
{
    const char* name = "Streaming / General";
    const char* group = "General";
    float targetLufs = -14.0f;
    Range lufsRange { -16.0f, -12.0f };
    Range crestRange { 7.0f, 16.0f };
    Range lowEndRange { 6.0f, 30.0f };
    float presenceMax = 38.0f;
    float correlationMin = 0.3f;
    bool wideExpected = true;
};

struct MixAssessmentInput
{
    float integratedLufs = -120.0f;
    float crestDb = 0.0f;
    float correlation = 1.0f;
    float msRatioDb = -120.0f;
    float monoLossDb = 0.0f;
    float truePeakDb = -120.0f;
    float clippedPercent = 0.0f;
    float worstTruePeakDb = -120.0f;
    float worstCorrelation = 1.0f;
    float worstMonoLossDb = 0.0f;
    float worstClippedPercent = 0.0f;
    float worstLowMidPercent = 0.0f;
    float worstResonanceFreqHz = 0.0f;
    float worstResonanceGainDb = 0.0f;
    float subPercent = 0.0f;
    float bassPercent = 0.0f;
    float lowMidPercent = 0.0f;
    float midPercent = 0.0f;
    float lowEndPercent = 0.0f;
    float presencePercent = 0.0f;
    float airPercent = 0.0f;
    float lowEndCorrelation = 1.0f;
    float lowEndSideDb = -120.0f;
    float widthPct = 0.0f;
    float lraLu = 0.0f;
    float transientDensity = 0.0f;
    float attackTimeMs = 0.0f;
    float spectralCentroidHz = 0.0f;
    float spectralRolloffHz = 0.0f;
    float resonanceFreqHz = 0.0f;
    float resonanceGainDb = 0.0f;
    float analysisSeconds = 0.0f;
    float fullPassSeconds = 0.0f;
    bool fullPassActive = false;
    bool fullPassCompleted = false;
    bool analysisFrozen = false;
    bool instrumental = false;
};

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
    float phaseCorrelation = 1.0f;
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

struct MixAssessment
{
    int overallScore = 50;
    int lufsScore = 50;
    int correlationScore = 50;
    int lowEndScore = 70;
    int crestScore = 70;
    int clippingScore = 100;
    int headphoneScore = 70;
    int speakerScore = 70;
    int lraScore = 70;
    int transientScore = 70;
    int toneScore = 70;
    int confidenceScore = 0;
    int loudnessConfidenceScore = 0;
    int dynamicsConfidenceScore = 0;
    int stereoConfidenceScore = 0;
    int toneConfidenceScore = 0;
    int deliveryConfidenceScore = 0;
    int releaseGateScore = 0;

    juce::String verdictKey = "measurement-limited";
    juce::String verdictTitle = "Warming up";
    juce::String statusLine = "Play audio to build a reliable live reading.";
    juce::String confidenceLabel = "No signal";
    juce::String confidenceText = "Waiting for usable audio.";
    juce::String confidenceBreakdownText = "Loudness 0, Dynamics 0, Stereo 0, Tone 0, Delivery 0";
    juce::String confidenceCompactText = "0/0/0/0/0";
    juce::String releaseGateTitle = "Measure first";
    juce::String releaseGateText = "Run a full pass before release decisions.";
    juce::String analysisScope = "Live";
    juce::String lufsTargetText;
    juce::String lowEndTargetText;
    juce::String crestTargetText;
    juce::String lraTargetText;
    juce::String correlationTargetText;

    std::array<juce::String, 5> priorityActions {};
    int priorityActionCount = 0;
    std::array<juce::String, 6> releaseBlockers {};
    int releaseBlockerCount = 0;

    float lufsDelta = 0.0f;
    float effectivePresenceMax = 38.0f;
    bool measurementReady = false;
    bool lufsOk = false;
    bool crestOk = false;
    bool lraOk = false;
    bool lowEndOk = false;
    bool lowEndPhaseOk = true;
    bool presenceOk = false;
    bool correlationOk = false;
    bool truePeakOk = false;
    bool clippingOk = true;
    bool releaseReady = false;
};

juce::StringArray getGenreNames();
const GenreProfile& getGenreProfile(int index) noexcept;
int getNumGenreProfiles() noexcept;
MixAssessment assessMix(const MixAssessmentInput& input, const GenreProfile& profile);
juce::String formatRange(const Range& range, const juce::String& suffix, int decimals = 0);

float lowEndOf(const AnalyzerMetrics& source) noexcept;
float presenceOf(const AnalyzerMetrics& source) noexcept;
float lowEndCorrelationOf(const AnalyzerMetrics& source) noexcept;
float lowEndSideDbOf(const AnalyzerMetrics& source) noexcept;
}
