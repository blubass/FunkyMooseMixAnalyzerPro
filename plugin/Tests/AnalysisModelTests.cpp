#include "AnalysisModel.h"

#include <iostream>

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

void expectContains(const juce::String& text, const juce::String& needle, const char* message)
{
    expect(text.containsIgnoreCase(needle), message);
}

void expectBlockerContains(const fmma::MixAssessment& result, const juce::String& needle, const char* message)
{
    for (auto i = 0; i < result.releaseBlockerCount; ++i)
    {
        if (result.releaseBlockers[static_cast<size_t>(i)].containsIgnoreCase(needle))
            return;
    }

    expect(false, message);
}

const fmma::GenreProfile& profileNamed(const char* name)
{
    for (auto i = 0; i < fmma::getNumGenreProfiles(); ++i)
    {
        const auto& profile = fmma::getGenreProfile(i);
        if (juce::String(profile.name) == name)
            return profile;
    }

    std::cerr << "FAIL: missing profile " << name << '\n';
    ++failures;
    return fmma::getGenreProfile(0);
}

fmma::MixAssessmentInput makeHealthyStreamingInput()
{
    fmma::MixAssessmentInput input;
    input.integratedLufs = -14.0f;
    input.crestDb = 10.0f;
    input.correlation = 0.72f;
    input.msRatioDb = -9.0f;
    input.monoLossDb = -0.5f;
    input.truePeakDb = -1.5f;
    input.clippedPercent = 0.0f;
    input.worstTruePeakDb = -1.4f;
    input.worstCorrelation = 0.70f;
    input.worstMonoLossDb = -0.8f;
    input.worstClippedPercent = 0.0f;
    input.worstLowMidPercent = 14.0f;
    input.worstResonanceFreqHz = 4200.0f;
    input.worstResonanceGainDb = 4.0f;
    input.subPercent = 5.0f;
    input.bassPercent = 8.0f;
    input.lowMidPercent = 12.0f;
    input.midPercent = 30.0f;
    input.lowEndPercent = 20.0f;
    input.presencePercent = 24.0f;
    input.airPercent = 8.0f;
    input.lowEndCorrelation = 0.90f;
    input.lowEndSideDb = -18.0f;
    input.widthPct = 65.0f;
    input.lraLu = 6.0f;
    input.transientDensity = 3.0f;
    input.attackTimeMs = 10.0f;
    input.spectralCentroidHz = 2500.0f;
    input.spectralRolloffHz = 12000.0f;
    input.resonanceFreqHz = 4200.0f;
    input.resonanceGainDb = 4.0f;
    input.analysisSeconds = 180.0f;
    input.fullPassSeconds = 180.0f;
    input.fullPassCompleted = true;
    return input;
}

void genreProfilesAreStable()
{
    expect(fmma::getNumGenreProfiles() >= 30, "genre profile table should keep the full standalone profile set");

    const auto names = fmma::getGenreNames();
    expect(names.contains("Streaming / General"), "genre names should include Streaming / General");
    expect(names.contains("Podcast / Spoken Word"), "genre names should include spoken-word delivery targets");

    const auto& streaming = profileNamed("Streaming / General");
    expect(streaming.lufsRange.contains(-14.0f), "streaming target range should contain -14 LUFS");
    expect(streaming.correlationMin >= 0.25f, "streaming correlation floor should remain conservative");
}

void warmupBlocksFinalJudgement()
{
    const auto result = fmma::assessMix({}, profileNamed("Streaming / General"));

    expect(! result.measurementReady, "empty input should not be measurement-ready");
    expect(result.verdictKey == "measurement-limited", "empty input should stay measurement-limited");
    expect(! result.releaseReady, "empty input should not pass the release gate");
    expect(result.releaseGateTitle == "Measure First", "empty input should ask for measurement before release");
    expectBlockerContains(result, "No reliable measurement", "warmup release gate should explain missing measurement");
    expect(result.priorityActionCount == 1, "warmup should produce exactly one guidance action");
    expectContains(result.priorityActions[0], "Start Pass", "warmup action should ask for Start Pass");
}

void healthyFullPassIsReady()
{
    const auto result = fmma::assessMix(makeHealthyStreamingInput(), profileNamed("Streaming / General"));

    expect(result.measurementReady, "healthy full pass should be measurement-ready");
    expect(result.verdictKey == "ready", "healthy full pass should be ready");
    expect(result.verdictTitle == "Full pass OK", "healthy full pass should get the final-pass verdict");
    expect(result.overallScore >= 85, "healthy full pass should score in a professional-ready range");
    expect(result.confidenceScore >= 90, "healthy full pass should have high confidence");
    expect(result.releaseReady, "healthy full pass should pass the release gate");
    expect(result.releaseGateTitle == "Release Ready", "healthy full pass should get release-ready gate title");
    expect(result.releaseGateScore >= 80, "healthy full pass release gate should be comfortably above threshold");
    expect(result.releaseBlockerCount == 0, "healthy full pass should not have release blockers");
    expect(result.loudnessConfidenceScore >= 90, "healthy full pass should have high loudness confidence");
    expect(result.dynamicsConfidenceScore >= 90, "healthy full pass should have high dynamics confidence");
    expect(result.stereoConfidenceScore >= 90, "healthy full pass should have high stereo confidence");
    expect(result.toneConfidenceScore >= 90, "healthy full pass should have high tone confidence");
    expect(result.deliveryConfidenceScore >= 90, "healthy full pass should have high delivery confidence");
    expectContains(result.confidenceBreakdownText, "Delivery", "confidence breakdown should name domain scores");
    expect(result.truePeakOk, "healthy full pass should pass true-peak target");
    expect(result.clippingOk, "healthy full pass should pass clipping target");
    expect(result.lowEndPhaseOk, "healthy full pass should pass low-end phase target");
    expectContains(result.priorityActions[0], "inside the selected profile", "healthy pass should not invent repair advice");
}

void domainConfidenceTracksMissingToneEvidence()
{
    auto input = makeHealthyStreamingInput();
    input.analysisSeconds = 12.0f;
    input.fullPassSeconds = 0.0f;
    input.fullPassCompleted = false;
    input.spectralCentroidHz = 0.0f;
    input.spectralRolloffHz = 0.0f;
    input.resonanceFreqHz = 0.0f;
    input.resonanceGainDb = 0.0f;

    const auto result = fmma::assessMix(input, profileNamed("Streaming / General"));

    expect(result.measurementReady, "short but valid input should be measurement-ready");
    expect(result.confidenceScore < 75, "short section without spectral evidence should not get final confidence");
    expect(! result.releaseReady, "short section should not pass the release gate");
    expect(result.releaseGateTitle == "Needs Full Pass", "short section should ask for a full pass");
    expectBlockerContains(result, "Finish a full pass", "short section release gate should require full pass");
    expect(result.toneConfidenceScore < result.loudnessConfidenceScore,
           "tone confidence should drop when spectral evidence is missing");
    expectContains(result.confidenceBreakdownText, "Tone", "confidence breakdown should expose the weak tone domain");
}

void clippingDominatesActions()
{
    auto input = makeHealthyStreamingInput();
    input.truePeakDb = -0.2f;
    input.worstTruePeakDb = -0.1f;
    input.clippedPercent = 0.4f;
    input.worstClippedPercent = 0.4f;

    const auto result = fmma::assessMix(input, profileNamed("Streaming / General"));

    expect(result.verdictKey == "needs-attention", "clipped pass should need attention");
    expect(! result.releaseReady, "clipped pass should not pass the release gate");
    expectBlockerContains(result, "Peak or clipping", "clipped pass release gate should block on safety");
    expect(! result.truePeakOk, "clipped pass should fail true peak");
    expect(! result.clippingOk, "clipped pass should fail sample clipping");
    expect(result.priorityActionCount >= 2, "clipped pass should produce peak and clipping actions");
    expectContains(result.priorityActions[0], "Clipping", "clipping should be the top priority when severe");
    expectContains(result.priorityActions[1], "True Peak", "true peak should stay visible after clipping");
}

void lowEndPhaseIsAReleaseBlocker()
{
    auto input = makeHealthyStreamingInput();
    input.lowEndCorrelation = 0.25f;
    input.lowEndSideDb = -2.0f;

    const auto result = fmma::assessMix(input, profileNamed("Streaming / General"));

    expect(result.verdictKey == "needs-attention", "unsafe low-end phase should need attention");
    expect(! result.releaseReady, "unsafe low-end phase should not pass the release gate");
    expectBlockerContains(result, "phase translation", "unsafe low-end phase release gate should block translation");
    expect(! result.lowEndPhaseOk, "unsafe low-end phase should fail the target");
    expectContains(result.priorityActions[0], "Low-end phase", "low-end phase should become a priority action");
}

void fullPassUsesWorstCaseHolds()
{
    auto input = makeHealthyStreamingInput();
    input.truePeakDb = -3.0f;
    input.clippedPercent = 0.0f;
    input.correlation = 0.80f;
    input.lowMidPercent = 12.0f;
    input.resonanceGainDb = 4.0f;

    input.worstTruePeakDb = 0.2f;
    input.worstClippedPercent = 0.2f;
    input.worstCorrelation = -0.10f;
    input.worstMonoLossDb = -5.0f;
    input.worstLowMidPercent = 31.0f;
    input.worstResonanceFreqHz = 3600.0f;
    input.worstResonanceGainDb = 13.0f;

    const auto result = fmma::assessMix(input, profileNamed("Streaming / General"));

    expect(result.verdictKey == "needs-attention", "full pass should judge by worst-case holds");
    expect(! result.truePeakOk, "worst-case true peak should fail even when current peak is safe");
    expect(! result.clippingOk, "worst-case clipping should fail even when current clipping is safe");
    expect(! result.correlationOk, "worst-case correlation should fail even when current correlation is safe");
    expectContains(result.priorityActions[0], "Clipping", "worst-case clipping should be ranked first");
}

void targetProfileMatchUsesReferenceWhenCaptured()
{
    auto input = makeHealthyStreamingInput();
    fmma::AnalyzerMetrics reference;
    reference.integratedLufs = -10.0f;
    reference.crestDb = 8.0f;
    reference.correlation = 0.62f;
    reference.widthPct = 82.0f;
    reference.bandPercents[0] = 10.0f;
    reference.bandPercents[1] = 18.0f;
    reference.bandPercents[4] = 30.0f;
    reference.analysisSeconds = 180.0f;
    reference.fullPassSeconds = 180.0f;
    reference.fullPassCompleted = true;

    const auto result = fmma::assessTargetMatch(input, profileNamed("Streaming / General"), &reference);

    expect(result.measurementReady, "target match should be ready for a healthy full pass");
    expect(result.referenceUsed, "target match should use a captured reference");
    expect(result.mode == "Reference + Genre", "target match mode should name reference matching");
    expect(result.score < 88, "different reference should create a target-match gap");
    expect(result.lufsDelta < -3.5f, "target match should compare LUFS against the reference");
    expectContains(result.action, "Target Match", "target match should produce an explicit target action");
}

void targetProfileMatchFallsBackToGenreProfile()
{
    const auto result = fmma::assessTargetMatch(makeHealthyStreamingInput(), profileNamed("Streaming / General"));

    expect(result.measurementReady, "genre-only target match should be ready for healthy input");
    expect(! result.referenceUsed, "genre-only target match should not claim a reference");
    expect(result.score >= 85, "healthy streaming input should closely match the genre profile");
    expect(result.mode == "Genre Profile", "genre-only target match should name the selected profile mode");
}
}

int main()
{
    genreProfilesAreStable();
    warmupBlocksFinalJudgement();
    healthyFullPassIsReady();
    domainConfidenceTracksMissingToneEvidence();
    clippingDominatesActions();
    lowEndPhaseIsAReleaseBlocker();
    fullPassUsesWorstCaseHolds();
    targetProfileMatchUsesReferenceWhenCaptured();
    targetProfileMatchFallsBackToGenreProfile();

    if (failures == 0)
    {
        std::cout << "AnalysisModelTests OK\n";
        return 0;
    }

    std::cerr << failures << " AnalysisModelTests failed\n";
    return 1;
}
