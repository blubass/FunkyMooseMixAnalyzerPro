#include "AnalysisModel.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr std::array<fmma::GenreProfile, 33> profiles {{
    { "Pop", "Popular", -10.0f, { -12.0f, -8.0f }, { 6.0f, 12.0f }, { 10.0f, 30.0f }, 38.0f, 0.35f, true },
    { "K-Pop", "Popular", -9.0f, { -11.0f, -7.0f }, { 5.0f, 11.0f }, { 12.0f, 32.0f }, 40.0f, 0.35f, true },
    { "Schlager / Deutschpop", "Popular", -10.0f, { -12.0f, -8.0f }, { 6.0f, 12.0f }, { 9.0f, 28.0f }, 38.0f, 0.4f, true },
    { "Indie / Alternative", "Popular", -11.0f, { -14.0f, -8.0f }, { 7.0f, 14.0f }, { 8.0f, 28.0f }, 38.0f, 0.3f, true },
    { "Singer-Songwriter", "Popular", -14.0f, { -17.0f, -11.0f }, { 9.0f, 18.0f }, { 6.0f, 24.0f }, 36.0f, 0.45f, false },
    { "Country / Folk", "Popular", -12.0f, { -15.0f, -9.0f }, { 8.0f, 15.0f }, { 8.0f, 26.0f }, 36.0f, 0.45f, false },
    { "Rock / Metal", "Band", -10.0f, { -12.0f, -7.0f }, { 5.0f, 11.0f }, { 10.0f, 32.0f }, 42.0f, 0.3f, true },
    { "Punk / Hardcore", "Band", -9.0f, { -11.0f, -7.0f }, { 5.0f, 10.0f }, { 8.0f, 28.0f }, 44.0f, 0.25f, true },
    { "Blues", "Band", -13.0f, { -16.0f, -10.0f }, { 8.0f, 16.0f }, { 7.0f, 25.0f }, 36.0f, 0.35f, false },
    { "Funk / Disco", "Band", -11.0f, { -13.0f, -9.0f }, { 7.0f, 13.0f }, { 12.0f, 32.0f }, 38.0f, 0.35f, true },
    { "Acoustic / Jazz", "Band", -16.0f, { -20.0f, -12.0f }, { 10.0f, 22.0f }, { 5.0f, 24.0f }, 34.0f, 0.25f, false },
    { "R&B / Soul", "Urban", -10.0f, { -13.0f, -8.0f }, { 6.0f, 13.0f }, { 12.0f, 34.0f }, 36.0f, 0.35f, true },
    { "Hip Hop / Rap", "Urban", -8.0f, { -10.0f, -6.0f }, { 5.0f, 11.0f }, { 18.0f, 42.0f }, 38.0f, 0.3f, true },
    { "Trap", "Urban", -8.0f, { -10.0f, -6.0f }, { 5.0f, 10.0f }, { 20.0f, 46.0f }, 38.0f, 0.25f, true },
    { "Afrobeats", "Urban", -10.0f, { -12.0f, -8.0f }, { 6.0f, 12.0f }, { 12.0f, 34.0f }, 38.0f, 0.35f, true },
    { "Reggaeton / Latin", "Urban", -9.0f, { -11.0f, -7.0f }, { 5.0f, 11.0f }, { 14.0f, 36.0f }, 40.0f, 0.35f, true },
    { "EDM / Electronic", "Electronic", -8.0f, { -10.0f, -6.0f }, { 5.0f, 10.0f }, { 16.0f, 40.0f }, 38.0f, 0.25f, true },
    { "House / Tech House", "Electronic", -8.0f, { -10.0f, -6.0f }, { 5.0f, 10.0f }, { 18.0f, 42.0f }, 36.0f, 0.25f, true },
    { "Techno", "Electronic", -8.0f, { -10.0f, -6.0f }, { 5.0f, 10.0f }, { 18.0f, 44.0f }, 36.0f, 0.2f, true },
    { "Trance", "Electronic", -8.0f, { -10.0f, -6.0f }, { 5.0f, 10.0f }, { 14.0f, 36.0f }, 40.0f, 0.25f, true },
    { "Drum & Bass", "Electronic", -7.0f, { -9.0f, -5.0f }, { 4.0f, 9.0f }, { 22.0f, 48.0f }, 40.0f, 0.2f, true },
    { "Dubstep / Bass Music", "Electronic", -7.0f, { -9.0f, -5.0f }, { 4.0f, 9.0f }, { 22.0f, 50.0f }, 40.0f, 0.2f, true },
    { "Lo-Fi / Chillhop", "Electronic", -12.0f, { -15.0f, -10.0f }, { 7.0f, 15.0f }, { 10.0f, 32.0f }, 34.0f, 0.25f, true },
    { "Ambient / Downtempo", "Electronic", -16.0f, { -22.0f, -12.0f }, { 9.0f, 24.0f }, { 5.0f, 30.0f }, 32.0f, 0.1f, true },
    { "Cinematic / Trailer", "Cinematic", -8.0f, { -11.0f, -6.0f }, { 7.0f, 18.0f }, { 12.0f, 42.0f }, 40.0f, 0.15f, true },
    { "Film Score", "Cinematic", -18.0f, { -24.0f, -14.0f }, { 12.0f, 28.0f }, { 5.0f, 30.0f }, 34.0f, 0.1f, true },
    { "Classical / Orchestral", "Cinematic", -20.0f, { -26.0f, -15.0f }, { 14.0f, 30.0f }, { 4.0f, 28.0f }, 32.0f, 0.05f, true },
    { "Meditation / Wellness", "Spoken & Media", -18.0f, { -24.0f, -14.0f }, { 10.0f, 24.0f }, { 4.0f, 24.0f }, 30.0f, 0.15f, true },
    { "Podcast / Spoken Word", "Spoken & Media", -16.0f, { -20.0f, -14.0f }, { 8.0f, 18.0f }, { 3.0f, 18.0f }, 42.0f, 0.55f, false },
    { "Audiobook", "Spoken & Media", -18.0f, { -21.0f, -16.0f }, { 8.0f, 18.0f }, { 2.0f, 16.0f }, 40.0f, 0.65f, false },
    { "Broadcast / TV", "Spoken & Media", -23.0f, { -24.0f, -22.0f }, { 8.0f, 20.0f }, { 3.0f, 20.0f }, 40.0f, 0.45f, false },
    { "YouTube / Streaming", "Spoken & Media", -14.0f, { -16.0f, -12.0f }, { 7.0f, 16.0f }, { 6.0f, 28.0f }, 38.0f, 0.35f, true },
    { "Streaming / General", "General", -14.0f, { -16.0f, -12.0f }, { 7.0f, 16.0f }, { 6.0f, 30.0f }, 38.0f, 0.3f, true },
}};

bool hasName(const fmma::GenreProfile& profile, const char* name)
{
    return juce::String(profile.name) == name;
}

bool hasGroup(const fmma::GenreProfile& profile, const char* group)
{
    return juce::String(profile.group) == group;
}

bool allowsBrightAir(const fmma::GenreProfile& profile)
{
    return hasName(profile, "Classical / Orchestral")
        || hasName(profile, "Film Score")
        || hasName(profile, "Cinematic / Trailer");
}

fmma::Range lraRangeForProfile(const fmma::GenreProfile& profile)
{
    if (hasName(profile, "Classical / Orchestral"))
        return { 8.0f, 28.0f };
    if (hasName(profile, "Film Score") || hasName(profile, "Cinematic / Trailer"))
        return { 6.0f, 24.0f };
    if (hasName(profile, "Ambient / Downtempo") || hasName(profile, "Meditation / Wellness"))
        return { 5.0f, 20.0f };
    if (hasGroup(profile, "Spoken & Media"))
        return { 2.0f, 12.0f };
    if (hasGroup(profile, "Electronic") || hasGroup(profile, "Urban"))
        return { 2.0f, 10.0f };
    if (hasName(profile, "Acoustic / Jazz") || hasName(profile, "Singer-Songwriter"))
        return { 5.0f, 18.0f };

    return { 3.0f, 13.0f };
}

int scoreRange(float value, fmma::Range range, float penaltyPerUnit, int fallback) noexcept
{
    if (! std::isfinite(value))
        return fallback;

    if (range.contains(value))
        return 100;

    const auto excess = juce::jmin(std::abs(value - range.low), std::abs(value - range.high));
    return juce::jlimit(0, 100, 100 - static_cast<int>(std::round(excess * penaltyPerUnit)));
}

int scorePresence(float value, float minimum, float maximum) noexcept
{
    if (! std::isfinite(value))
        return 70;

    if (value >= minimum && value <= maximum)
        return 100;

    const auto excess = value < minimum ? minimum - value : value - maximum;
    return juce::jlimit(0, 100, 100 - static_cast<int>(std::round(excess * 4.0f)));
}

int scoreLufs(float delta) noexcept
{
    if (! std::isfinite(delta))
        return 50;

    return juce::jlimit(0, 100, 100 - static_cast<int>(std::round(std::abs(delta) * 7.0f)));
}

int scoreCorrelation(float correlation, float minimum) noexcept
{
    if (! std::isfinite(correlation))
        return 50;

    if (correlation >= minimum)
        return juce::jlimit(0, 100, static_cast<int>(std::round(correlation * 100.0f)));

    return juce::jlimit(0, 100, static_cast<int>(std::round(correlation * 100.0f)) - 20);
}

int scoreWidth(float widthPct, bool wideExpected) noexcept
{
    const auto target = wideExpected ? 65.0f : 35.0f;
    return juce::jlimit(0, 100, 100 - static_cast<int>(std::round(std::abs(widthPct - target) * 1.4f)));
}

int scoreTransientShape(float density, float attackMs, fmma::Range crestRange, float crestDb) noexcept
{
    auto score = scoreRange(crestDb, crestRange, 5.0f, 70);

    if (density > 0.0f && density < 1.0f && attackMs > 30.0f)
        score -= 25;
    if (density > 8.0f && attackMs < 4.0f)
        score -= 18;

    return juce::jlimit(0, 100, score);
}

int scoreLowEndShape(const fmma::MixAssessmentInput& input, const fmma::GenreProfile& profile) noexcept
{
    auto score = scoreRange(input.lowEndPercent, profile.lowEndRange, 4.0f, 70);

    if (input.subPercent > 16.0f && input.subPercent > input.bassPercent * 1.35f)
        score -= static_cast<int>(std::round((input.subPercent - juce::jmax(16.0f, input.bassPercent)) * 4.0f));
    if (input.bassPercent > 22.0f && input.bassPercent > input.subPercent * 1.8f)
        score -= static_cast<int>(std::round((input.bassPercent - 22.0f) * 3.0f));
    if (input.lowMidPercent > 24.0f)
        score -= static_cast<int>(std::round((input.lowMidPercent - 24.0f) * 4.0f));
    if (input.lowEndPercent > profile.lowEndRange.low && input.bassPercent < 4.0f && input.subPercent > 8.0f)
        score -= 18;

    return juce::jlimit(0, 100, score);
}

int scoreLowEndPhase(const fmma::MixAssessmentInput& input) noexcept
{
    auto score = 100;

    if (input.lowEndCorrelation < 0.75f)
        score -= static_cast<int>(std::round((0.75f - input.lowEndCorrelation) * 120.0f));
    if (input.lowEndSideDb > -10.0f)
        score -= static_cast<int>(std::round((input.lowEndSideDb + 10.0f) * 5.0f));

    return juce::jlimit(0, 100, score);
}

int scoreAirShape(const fmma::MixAssessmentInput& input, const fmma::GenreProfile& profile)
{
    auto score = 100;
    const auto effectivePresenceMax = input.instrumental ? profile.presenceMax : profile.presenceMax - 4.0f;

    if (input.airPercent > 18.0f && ! allowsBrightAir(profile))
        score -= static_cast<int>(std::round((input.airPercent - 18.0f) * 4.0f));
    if (input.airPercent < 2.5f && input.spectralRolloffHz > 0.0f && input.spectralRolloffHz < 7500.0f)
        score -= static_cast<int>(std::round((2.5f - input.airPercent) * 8.0f));
    if (input.spectralCentroidHz > 5000.0f && input.presencePercent > effectivePresenceMax - 2.0f)
        score -= 18;
    if (input.spectralCentroidHz > 0.0f && input.spectralCentroidHz < 1200.0f && input.presencePercent < 16.0f)
        score -= 16;

    return juce::jlimit(0, 100, score);
}

int scoreLraShape(const fmma::MixAssessmentInput& input, const fmma::Range& targetRange) noexcept
{
    const auto duration = input.fullPassCompleted ? input.fullPassSeconds : input.analysisSeconds;
    if (duration < 20.0f || input.lraLu <= 0.0f)
        return 70;

    return scoreRange(input.lraLu, targetRange, 6.0f, 70);
}

float estimatedNormalisedTruePeak(const fmma::MixAssessmentInput& input, float targetLufs) noexcept
{
    if (input.integratedLufs <= -119.0f || input.truePeakDb <= -119.0f
        || ! std::isfinite(input.integratedLufs) || ! std::isfinite(input.truePeakDb))
        return -120.0f;

    return input.truePeakDb + (targetLufs - input.integratedLufs);
}

int scoreConfidence(const fmma::MixAssessmentInput& input) noexcept
{
    const auto duration = input.fullPassCompleted ? input.fullPassSeconds : input.analysisSeconds;
    auto score = 0;

    if (duration >= 8.0f)
        score += 25;
    if (duration >= 30.0f)
        score += 20;
    if (duration >= 60.0f)
        score += 15;
    if (duration >= 120.0f)
        score += 10;

    if (input.integratedLufs > -119.0f)
        score += 10;
    if (input.spectralCentroidHz > 0.0f && input.spectralRolloffHz > 0.0f)
        score += 5;
    if (input.lraLu > 0.0f || duration >= 20.0f)
        score += 5;

    if (input.fullPassActive)
        score += 5;
    if (input.fullPassCompleted)
        score += duration >= 60.0f ? 20 : 10;

    return juce::jlimit(0, 100, score);
}

juce::String confidenceLabelFor(int score, const fmma::MixAssessmentInput& input)
{
    if (input.fullPassCompleted && score >= 75)
        return "Full pass";
    if (score >= 75)
        return "High";
    if (score >= 50)
        return "Section";
    if (score >= 25)
        return "Low";
    return "Unstable";
}

juce::String confidenceTextFor(int score, const fmma::MixAssessmentInput& input)
{
    const auto duration = input.fullPassCompleted ? input.fullPassSeconds : input.analysisSeconds;

    if (input.fullPassActive)
        return "Recording analysis pass; play the song section or full song once.";

    if (input.fullPassCompleted)
    {
        if (score >= 75)
            return "Completed pass; values are frozen for final judgement.";
        return "Pass completed, but it is short; treat judgement as section-based.";
    }

    if (duration < 8.0f)
        return "Too little audio for reliable judgement.";

    if (score >= 75)
        return "Stable live estimate; use a full pass for final calls.";

    return "Section estimate; play more audio or run Start Pass.";
}

void addAction(fmma::MixAssessment& result, const juce::String& action)
{
    if (result.priorityActionCount >= static_cast<int>(result.priorityActions.size()))
        return;

    result.priorityActions[static_cast<size_t>(result.priorityActionCount)] = action;
    ++result.priorityActionCount;
}

struct ActionCandidate
{
    juce::String action;
    float severity = 0.0f;
    int order = 0;
};

void addActionCandidate(std::array<ActionCandidate, 32>& candidates,
                        int& count,
                        int& order,
                        float severity,
                        const juce::String& action)
{
    if (count >= static_cast<int>(candidates.size()))
        return;

    auto& candidate = candidates[static_cast<size_t>(count)];
    candidate.action = action;
    candidate.severity = severity;
    candidate.order = order++;
    ++count;
}

void publishPriorityActions(fmma::MixAssessment& result,
                            std::array<ActionCandidate, 32>& candidates,
                            int count)
{
    std::sort(candidates.begin(), candidates.begin() + count,
              [] (const ActionCandidate& a, const ActionCandidate& b)
              {
                  if (std::abs(a.severity - b.severity) < 0.0001f)
                      return a.order < b.order;

                  return a.severity > b.severity;
              });

    for (auto i = 0; i < count && result.priorityActionCount < static_cast<int>(result.priorityActions.size()); ++i)
        addAction(result, candidates[static_cast<size_t>(i)].action);
}
}

namespace fmma
{
juce::StringArray getGenreNames()
{
    juce::StringArray names;
    for (const auto& profile : profiles)
        names.add(profile.name);
    return names;
}

const GenreProfile& getGenreProfile(int index) noexcept
{
    return profiles[static_cast<size_t>(juce::jlimit(0, static_cast<int>(profiles.size()) - 1, index))];
}

int getNumGenreProfiles() noexcept
{
    return static_cast<int>(profiles.size());
}

juce::String formatRange(const Range& range, const juce::String& suffix, int decimals)
{
    return juce::String(range.low, decimals) + " to " + juce::String(range.high, decimals) + suffix;
}

MixAssessment assessMix(const MixAssessmentInput& input, const GenreProfile& profile)
{
    MixAssessment result;
    const auto durationForAssessment = input.fullPassCompleted ? input.fullPassSeconds : input.analysisSeconds;
    result.confidenceScore = scoreConfidence(input);
    result.confidenceLabel = confidenceLabelFor(result.confidenceScore, input);
    result.confidenceText = confidenceTextFor(result.confidenceScore, input);
    result.analysisScope = input.fullPassCompleted ? "Full pass"
                         : input.fullPassActive ? "Recording"
                         : durationForAssessment >= 30.0f ? "Section"
                         : "Live";
    result.measurementReady = durationForAssessment >= 8.0f && input.integratedLufs > -119.0f;
    result.lufsDelta = input.integratedLufs - profile.targetLufs;
    result.effectivePresenceMax = input.instrumental ? profile.presenceMax : profile.presenceMax - 4.0f;
    const auto presenceMin = input.instrumental ? 0.0f : 18.0f;
    const auto lraTargetRange = lraRangeForProfile(profile);
    const auto hasReliableLra = durationForAssessment >= 20.0f && input.lraLu > 0.0f;
    const auto assessedTruePeakDb = input.fullPassCompleted ? juce::jmax(input.truePeakDb, input.worstTruePeakDb)
                                                            : input.truePeakDb;
    const auto assessedClippedPercent = input.fullPassCompleted ? juce::jmax(input.clippedPercent, input.worstClippedPercent)
                                                                : input.clippedPercent;
    const auto assessedCorrelation = input.fullPassCompleted ? juce::jmin(input.correlation, input.worstCorrelation)
                                                             : input.correlation;
    const auto assessedMonoLossDb = input.fullPassCompleted ? juce::jmin(input.monoLossDb, input.worstMonoLossDb)
                                                            : input.monoLossDb;
    const auto assessedLowMidPercent = input.fullPassCompleted ? juce::jmax(input.lowMidPercent, input.worstLowMidPercent)
                                                               : input.lowMidPercent;
    const auto assessedResonanceGainDb = input.fullPassCompleted ? juce::jmax(input.resonanceGainDb, input.worstResonanceGainDb)
                                                                 : input.resonanceGainDb;
    const auto assessedResonanceFreqHz = input.fullPassCompleted && input.worstResonanceGainDb > input.resonanceGainDb
        ? input.worstResonanceFreqHz
        : input.resonanceFreqHz;
    auto lowEndShapeInput = input;
    lowEndShapeInput.lowMidPercent = assessedLowMidPercent;
    auto deliveryInput = input;
    deliveryInput.truePeakDb = assessedTruePeakDb;

    result.lufsOk = profile.lufsRange.contains(input.integratedLufs);
    result.crestOk = profile.crestRange.contains(input.crestDb);
    result.lraOk = ! hasReliableLra || lraTargetRange.contains(input.lraLu);
    result.lowEndOk = profile.lowEndRange.contains(input.lowEndPercent);
    result.lowEndPhaseOk = input.lowEndCorrelation >= 0.65f && input.lowEndSideDb <= -6.0f;
    result.presenceOk = input.presencePercent <= result.effectivePresenceMax && input.presencePercent >= presenceMin;
    result.correlationOk = assessedCorrelation >= profile.correlationMin;
    result.truePeakOk = assessedTruePeakDb <= -1.0f;
    result.clippingOk = assessedClippedPercent <= 0.05f;

    result.lufsScore = scoreLufs(result.lufsDelta);
    result.correlationScore = scoreCorrelation(assessedCorrelation, profile.correlationMin);
    result.lowEndScore = scoreLowEndShape(lowEndShapeInput, profile);
    result.crestScore = scoreRange(input.crestDb, profile.crestRange, 5.0f, 70);
    const auto sampleClipScore = assessedClippedPercent <= 0.01f
        ? 100
        : juce::jlimit(0, 100, 100 - static_cast<int>(std::round(assessedClippedPercent * 200.0f)));
    const auto truePeakScore = assessedTruePeakDb <= -1.0f
        ? 100
        : juce::jlimit(0, 100, 100 - static_cast<int>(std::round((assessedTruePeakDb + 1.0f) * 35.0f)));
    result.clippingScore = juce::jmin(sampleClipScore, truePeakScore);
    result.lraScore = scoreLraShape(input, lraTargetRange);
    result.transientScore = scoreTransientShape(input.transientDensity, input.attackTimeMs, profile.crestRange, input.crestDb);
    const auto presenceScore = scorePresence(input.presencePercent, presenceMin, result.effectivePresenceMax);
    const auto resonanceScore = assessedResonanceGainDb <= 10.0f
        ? 100
        : juce::jlimit(0, 100, 100 - static_cast<int>(std::round((assessedResonanceGainDb - 10.0f) * 6.0f)));
    const auto airScore = scoreAirShape(input, profile);
    const auto lowEndPhaseScore = scoreLowEndPhase(input);
    result.toneScore = juce::jlimit(0, 100, static_cast<int>(std::round(
        result.lowEndScore * 0.45f
        + presenceScore * 0.25f
        + resonanceScore * 0.15f
        + airScore * 0.15f)));
    const auto monoCompat = juce::jlimit(0, 100, static_cast<int>(std::round(((assessedCorrelation + 1.0f) * 0.5f) * 100.0f)));
    const auto widthScore = scoreWidth(input.widthPct, profile.wideExpected);
    result.headphoneScore = juce::jlimit(0, 100, static_cast<int>(std::round(
        monoCompat * 0.40f + widthScore * 0.35f + result.toneScore * 0.25f)));
    result.speakerScore = juce::jlimit(0, 100, static_cast<int>(std::round(
        monoCompat * 0.48f + result.lowEndScore * 0.22f + result.clippingScore * 0.15f + lowEndPhaseScore * 0.15f)));

    result.overallScore = juce::jlimit(0, 100, static_cast<int>(
        std::round(result.lufsScore * 0.21f
                 + result.correlationScore * 0.18f
                 + result.lowEndScore * 0.16f
                 + result.crestScore * 0.12f
                 + result.clippingScore * 0.11f
                 + result.toneScore * 0.10f
                 + result.lraScore * 0.07f
                 + result.transientScore * 0.05f)));

    result.lufsTargetText = "Target " + juce::String(profile.targetLufs, 0) + " LUFS, range "
                          + formatRange(profile.lufsRange, " LUFS", 0);
    result.lowEndTargetText = "Low-end target " + formatRange(profile.lowEndRange, "%", 0);
    result.crestTargetText = "Crest target " + formatRange(profile.crestRange, " dB", 0);
    result.lraTargetText = "LRA target " + formatRange(lraTargetRange, " LU", 0);
    result.correlationTargetText = "Correlation min " + juce::String(profile.correlationMin, 2);

    if (! result.measurementReady)
    {
        result.verdictKey = "measurement-limited";
        result.verdictTitle = "Warming up";
        result.statusLine = result.confidenceText;
        addAction(result, "Use Start Pass, then play at least the loudest chorus/drop before judging.");
        return result;
    }

    std::array<ActionCandidate, 32> actionCandidates {};
    auto actionCandidateCount = 0;
    auto actionOrder = 0;

    if (! result.truePeakOk)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           120.0f + ((assessedTruePeakDb + 1.0f) * 35.0f),
                           "True Peak is " + juce::String(assessedTruePeakDb, 1)
                               + " dBTP; lower limiter ceiling/output to <= -1.0 dBTP.");

    if (! result.clippingOk)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           125.0f + (assessedClippedPercent * 200.0f),
                           "Clipping is " + juce::String(assessedClippedPercent, 3)
                               + "%; reduce output gain or limiter drive before judging tone.");

    if (! result.correlationOk)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           100.0f + ((profile.correlationMin - assessedCorrelation) * 100.0f),
                           "Correlation is " + juce::String(assessedCorrelation, 2)
                               + "; check wideners, chorus, Haas delays, and mono compatibility.");
    else if (assessedCorrelation > 0.95f && profile.wideExpected)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           32.0f + ((assessedCorrelation - 0.95f) * 100.0f),
                           "Stereo field is very narrow; pan supporting parts or add subtle width above the low end.");

    if (assessedMonoLossDb < -4.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           92.0f + ((-4.0f - assessedMonoLossDb) * 10.0f),
                           "Mono sum loses " + juce::String(std::abs(assessedMonoLossDb), 1)
                               + " dB; narrow phase-heavy width and keep low-end centered.");
    else if (assessedMonoLossDb < -2.5f && profile.wideExpected)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           46.0f + ((-2.5f - assessedMonoLossDb) * 8.0f),
                           "Mono sum loses " + juce::String(std::abs(assessedMonoLossDb), 1)
                               + " dB; verify hooks, vocals, and bass still translate in mono.");

    if (input.msRatioDb > -3.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           58.0f + ((input.msRatioDb + 3.0f) * 10.0f),
                           "Side energy is high (" + juce::String(input.msRatioDb, 1)
                               + " dB M/S); reduce side low-end or wide bus processing.");

    if (input.integratedLufs > profile.lufsRange.high)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           55.0f + ((input.integratedLufs - profile.lufsRange.high) * 12.0f),
                           "Integrated loudness is " + juce::String(input.integratedLufs, 1)
                               + " LUFS; reduce limiter drive toward "
                               + formatRange(profile.lufsRange, " LUFS", 0) + ".");
    else if (input.integratedLufs < profile.lufsRange.low)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           42.0f + ((profile.lufsRange.low - input.integratedLufs) * 8.0f),
                           "Integrated loudness is " + juce::String(input.integratedLufs, 1)
                               + " LUFS; add controlled loudness toward "
                               + formatRange(profile.lufsRange, " LUFS", 0) + ".");

    const auto streamTruePeak = estimatedNormalisedTruePeak(deliveryInput, -14.0f);
    if (streamTruePeak > -1.0f && input.integratedLufs < -14.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           78.0f + ((streamTruePeak + 1.0f) * 8.0f),
                           "Normalising to -14 LUFS would estimate " + juce::String(streamTruePeak, 1)
                               + " dBTP; reduce peaks or raise density before delivery.");
    else if (input.integratedLufs > -8.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           34.0f + ((input.integratedLufs + 8.0f) * 4.0f),
                           "Streaming services may turn this down by about "
                               + juce::String(input.integratedLufs + 14.0f, 1)
                               + " dB; keep the loudness only if the density is intentional.");

    if (input.lowEndPercent > profile.lowEndRange.high)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           70.0f + ((input.lowEndPercent - profile.lowEndRange.high) * 5.0f),
                           "Low-end is " + juce::String(input.lowEndPercent, 1)
                               + "%; clean non-bass lows and separate kick/bass space.");
    else if (input.lowEndPercent < profile.lowEndRange.low)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           52.0f + ((profile.lowEndRange.low - input.lowEndPercent) * 4.0f),
                           "Low-end is " + juce::String(input.lowEndPercent, 1)
                               + "%; add bass weight or harmonic saturation for translation.");

    if (input.subPercent > 16.0f && input.subPercent > input.bassPercent * 1.35f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           68.0f + ((input.subPercent - juce::jmax(input.bassPercent, 16.0f)) * 5.0f),
                           "Sub dominates bass (" + juce::String(input.subPercent, 1)
                               + "% sub vs " + juce::String(input.bassPercent, 1)
                               + "% bass); trim sub rumble or add upper bass harmonics.");

    if (input.bassPercent > 22.0f && input.bassPercent > input.subPercent * 1.8f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           50.0f + ((input.bassPercent - 22.0f) * 4.0f),
                           "Bass band is heavy (" + juce::String(input.bassPercent, 1)
                               + "%); check 60-250 Hz masking between kick, bass, and low instruments.");

    if (assessedLowMidPercent > 24.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           57.0f + ((assessedLowMidPercent - 24.0f) * 5.0f),
                           "Low-mids are crowded (" + juce::String(assessedLowMidPercent, 1)
                               + "%); reduce 250-500 Hz buildup before adding brightness.");

    if (! result.lowEndPhaseOk)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           88.0f + (juce::jmax(0.0f, 0.65f - input.lowEndCorrelation) * 80.0f)
                               + juce::jmax(0.0f, input.lowEndSideDb + 6.0f) * 4.0f,
                           "Low-end phase is unsafe (corr " + juce::String(input.lowEndCorrelation, 2)
                               + ", side " + juce::String(input.lowEndSideDb, 1)
                               + " dB); keep sub/kick mono and remove stereo width below bass fundamentals.");

    if (input.lowEndPercent > profile.lowEndRange.low && input.bassPercent < 4.0f && input.subPercent > 8.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           48.0f + ((8.0f - input.bassPercent) * 4.0f),
                           "Low-end is mostly sub with little audible bass; add harmonics around 80-180 Hz for small speakers.");

    if (input.crestDb < profile.crestRange.low)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           65.0f + ((profile.crestRange.low - input.crestDb) * 7.0f),
                           "Crest is " + juce::String(input.crestDb, 1)
                               + " dB; ease bus compression/limiting or restore transients.");
    else if (input.crestDb > profile.crestRange.high)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           45.0f + ((input.crestDb - profile.crestRange.high) * 5.0f),
                           "Crest is " + juce::String(input.crestDb, 1)
                               + " dB; control jumpy peaks with gentle compression or clip gain.");

    if (hasReliableLra && input.lraLu < lraTargetRange.low)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           56.0f + ((lraTargetRange.low - input.lraLu) * 6.0f),
                           "LRA is " + juce::String(input.lraLu, 1)
                               + " LU; add section contrast, automation, or ease bus compression.");
    else if (hasReliableLra && input.lraLu > lraTargetRange.high)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           52.0f + ((input.lraLu - lraTargetRange.high) * 5.0f),
                           "LRA is " + juce::String(input.lraLu, 1)
                               + " LU; control large level swings with automation or gentle compression.");

    if (input.presencePercent > result.effectivePresenceMax)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           60.0f + ((input.presencePercent - result.effectivePresenceMax) * 3.0f),
                           "Presence is " + juce::String(input.presencePercent, 1)
                               + "%; check vocals, synths, cymbals, de-essing, or dynamic EQ.");
    else if (! input.instrumental && input.presencePercent < presenceMin)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           48.0f + ((presenceMin - input.presencePercent) * 3.0f),
                           "Presence is " + juce::String(input.presencePercent, 1)
                               + "%; clear 2-5 kHz masking or add a gentle vocal lift.");

    if (! input.instrumental && input.presencePercent < presenceMin && assessedLowMidPercent > 20.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           62.0f + ((assessedLowMidPercent - 20.0f) * 3.0f),
                           "Vocal presence is likely masked by low-mids; reduce 250-500 Hz buildup before boosting 2-5 kHz.");

    if (input.spectralCentroidHz > 5000.0f && input.presencePercent > result.effectivePresenceMax - 2.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           58.0f + ((input.spectralCentroidHz - 5000.0f) / 250.0f),
                           "Overall tone is very forward; check harsh synths, guitars, cymbals, and vocal edge.");

    if (input.airPercent < 2.5f && input.spectralRolloffHz > 0.0f && input.spectralRolloffHz < 7500.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           44.0f + ((7500.0f - input.spectralRolloffHz) / 250.0f),
                           "Top end rolls off early; add air only after clearing mud and presence masking.");

    if (input.airPercent > 18.0f && ! allowsBrightAir(profile))
    {
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           42.0f + ((input.airPercent - 18.0f) * 2.0f),
                           "Air band is " + juce::String(input.airPercent, 1)
                               + "%; check exciters, cymbals, and broad high-shelf boosts.");
    }

    if (assessedResonanceGainDb > 8.0f && assessedResonanceFreqHz >= 2500.0f && assessedResonanceFreqHz <= 7000.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           80.0f + ((assessedResonanceGainDb - 8.0f) * 6.0f),
                           "Harsh resonance around " + juce::String(assessedResonanceFreqHz, 0)
                               + " Hz; use dynamic EQ or de-essing only if that band is audibly poking out.");
    else if (assessedResonanceGainDb > 8.0f && assessedResonanceFreqHz > 7000.0f && assessedResonanceFreqHz <= 11000.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           72.0f + ((assessedResonanceGainDb - 8.0f) * 5.0f),
                           "Sibilance/air resonance around " + juce::String(assessedResonanceFreqHz, 0)
                               + " Hz; check esses, hats, exciters, and bright reverbs.");
    else if (assessedResonanceGainDb > 10.0f && assessedResonanceFreqHz > 0.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           72.0f + ((assessedResonanceGainDb - 10.0f) * 6.0f),
                           "Dominant resonance around " + juce::String(assessedResonanceFreqHz, 0)
                               + " Hz; use narrow dynamic EQ only if it is audible.");

    if (input.transientDensity > 0.0f && input.transientDensity < 1.0f && input.attackTimeMs > 30.0f && input.crestDb < profile.crestRange.low)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           62.0f + ((30.0f - input.crestDb) * 1.0f),
                           "Transients look over-compressed; lengthen compressor attack or restore drum punch.");
    else if (input.transientDensity > 8.0f && input.attackTimeMs < 4.0f)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           40.0f + ((input.transientDensity - 8.0f) * 3.0f),
                           "Very fast transient pattern; soften spikes with parallel compression or transient control.");

    if (result.speakerScore < 60)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           54.0f + static_cast<float>(60 - result.speakerScore),
                           "Speaker translation score is " + juce::String(result.speakerScore)
                               + "; verify mono, low-end balance, and limiter headroom on small speakers.");

    if (result.headphoneScore < 60)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           50.0f + static_cast<float>(60 - result.headphoneScore),
                           "Headphone translation score is " + juce::String(result.headphoneScore)
                               + "; check width, harsh upper mids, and low-end center focus.");

    if (! input.fullPassCompleted && ! input.fullPassActive && result.confidenceScore < 75)
        addActionCandidate(actionCandidates, actionCandidateCount, actionOrder,
                           30.0f + static_cast<float>(75 - result.confidenceScore) * 0.25f,
                           "Run a full pass before final judgement; current confidence is "
                               + result.confidenceLabel + " " + juce::String(result.confidenceScore) + "/100.");

    if (actionCandidateCount == 0)
        addAction(result, "Metrics sit inside the selected profile; keep moves subtle and compare against references.");
    else
        publishPriorityActions(result, actionCandidates, actionCandidateCount);

    const auto coreChecksOk = result.lufsOk
                           && result.correlationOk
                           && result.lowEndOk
                           && result.lowEndPhaseOk
                           && result.crestOk
                           && result.lraOk
                           && result.clippingOk
                           && result.truePeakOk
                           && result.presenceOk;

    if (coreChecksOk)
    {
        result.verdictKey = "ready";
        result.verdictTitle = input.fullPassCompleted && result.confidenceScore >= 75 ? "Full pass OK"
                            : input.fullPassActive ? "Pass in range"
                            : "Section OK";
        result.statusLine = input.fullPassCompleted && result.confidenceScore >= 75
            ? "Completed pass sits inside the selected profile; confirm against a reference."
            : "This section is in range; finish a full pass before making final calls.";
    }
    else if (! result.correlationOk || ! result.lowEndOk || ! result.lowEndPhaseOk
          || ! result.clippingOk || ! result.truePeakOk || ! result.presenceOk)
    {
        result.verdictKey = "needs-attention";
        result.verdictTitle = "Review";
        result.statusLine = "One or more core translation checks need attention.";
    }
    else
    {
        result.verdictKey = "polish";
        result.verdictTitle = "Polish";
        result.statusLine = "Close to target; refine loudness, dynamics, or tone.";
    }

    return result;
}
}
