#include "PluginEditor.h"

#include <cmath>

namespace
{
const juce::Colour background { 0xff101215 };
const juce::Colour panel { 0xff1b1e24 };
const juce::Colour border { 0x22ffffff };
const juce::Colour text { 0xfff4f7fb };
const juce::Colour muted { 0xffa1a8b3 };
const juce::Colour teal { 0xff19d3c5 };
const juce::Colour violet { 0xff8b5cf6 };
const juce::Colour amber { 0xfff59e0b };
const juce::Colour danger { 0xffef4444 };
const juce::Colour success { 0xff22c55e };

constexpr std::array<const char*, fmma::bandCount> bandNames {{
    "Sub", "Bass", "Low-Mids", "Mids", "Presence", "Air"
}};

float dbToUnit(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, (value + 60.0f) / 60.0f);
}

juce::Colour scoreColour(int score) noexcept
{
    return score >= 80 ? success : score >= 60 ? amber : danger;
}

juce::Colour okColour(bool ok) noexcept
{
    return ok ? success : amber;
}

float smoothValue(float current, float target, float amount, float deadband = 0.0f) noexcept
{
    if (! std::isfinite(target))
        return current;
    if (! std::isfinite(current) || current <= -119.0f)
        return target;
    if (std::abs(target - current) <= deadband)
        return current;
    return current + ((target - current) * amount);
}

juce::String formatHz(float value)
{
    if (! std::isfinite(value) || value <= 0.0f)
        return "N/A";
    return juce::String(value, 0) + " Hz";
}

float lowEndOf(const fmma::AnalyzerMetrics& source) noexcept
{
    return source.bandPercents[0] + source.bandPercents[1];
}

float presenceOf(const fmma::AnalyzerMetrics& source) noexcept
{
    return source.bandPercents[4];
}

float lowEndCorrelationOf(const fmma::AnalyzerMetrics& source) noexcept
{
    return juce::jmin(source.bandCorrelations[0], source.bandCorrelations[1]);
}

float lowEndSideDbOf(const fmma::AnalyzerMetrics& source) noexcept
{
    return juce::jmax(source.bandSideRatiosDb[0], source.bandSideRatiosDb[1]);
}

float frequencyToUnit(float frequencyHz) noexcept
{
    if (! std::isfinite(frequencyHz) || frequencyHz <= 0.0f)
        return 0.0f;

    const auto logMin = std::log10(20.0f);
    const auto logMax = std::log10(20000.0f);
    return juce::jlimit(0.0f, 1.0f, (std::log10(frequencyHz) - logMin) / (logMax - logMin));
}
}

FunkyMooseMixAnalyzerAudioProcessorEditor::FunkyMooseMixAnalyzerAudioProcessorEditor(
    FunkyMooseMixAnalyzerAudioProcessor& ownerProcessor)
    : AudioProcessorEditor(&ownerProcessor),
      audioProcessor(ownerProcessor)
{
    genreLabel.setText("Genre Profile", juce::dontSendNotification);
    genreLabel.setColour(juce::Label::textColourId, muted);
    genreLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(genreLabel);

    const auto genres = fmma::getGenreNames();
    for (auto i = 0; i < genres.size(); ++i)
        genreBox.addItem(genres[i], i + 1);

    genreBox.setColour(juce::ComboBox::backgroundColourId, panel);
    genreBox.setColour(juce::ComboBox::textColourId, text);
    genreBox.setColour(juce::ComboBox::outlineColourId, border);
    addAndMakeVisible(genreBox);

    instrumentalToggle.setButtonText("Instrumental");
    instrumentalToggle.setColour(juce::ToggleButton::textColourId, muted);
    addAndMakeVisible(instrumentalToggle);

    passButton.setButtonText("Start Pass");
    passButton.onClick = [this]
    {
        if (referenceCaptureInProgress)
            return;

        if (metrics.fullPassActive)
            audioProcessor.requestFullPassFinish();
        else
        {
            audioProcessor.requestFullPassStart();
            hasDisplayMetrics = false;
        }

        assessmentCountdown = 0;
    };
    addAndMakeVisible(passButton);

    referenceButton.setButtonText("Capture Ref");
    referenceButton.onClick = [this]
    {
        if (referenceCaptureInProgress && metrics.fullPassActive)
        {
            pendingReferenceSnapshot = true;
            audioProcessor.requestFullPassFinish();
        }
        else if (! metrics.fullPassActive)
        {
            referenceCaptureInProgress = true;
            pendingReferenceSnapshot = false;
            audioProcessor.requestFullPassStart();
            hasDisplayMetrics = false;
        }

        assessmentCountdown = 0;
    };
    addAndMakeVisible(referenceButton);

    clearReferenceButton.setButtonText("Clr Ref");
    clearReferenceButton.onClick = [this]
    {
        if (referenceCaptureInProgress)
            return;

        referenceMetrics = {};
        hasReferenceMetrics = false;
        pendingReferenceSnapshot = false;
        audioProcessor.clearStoredReferenceMetrics();
    };
    addAndMakeVisible(clearReferenceButton);

    snapshotAButton.setButtonText("Snap A");
    snapshotAButton.onClick = [this]
    {
        if (referenceCaptureInProgress)
            return;

        snapshotA = audioProcessor.getMetrics();
        hasSnapshotA = true;
        audioProcessor.storeSnapshotA(snapshotA);
    };
    addAndMakeVisible(snapshotAButton);

    snapshotBButton.setButtonText("Snap B");
    snapshotBButton.onClick = [this]
    {
        if (referenceCaptureInProgress)
            return;

        snapshotB = audioProcessor.getMetrics();
        hasSnapshotB = true;
        audioProcessor.storeSnapshotB(snapshotB);
    };
    addAndMakeVisible(snapshotBButton);

    clearSnapshotsButton.setButtonText("Clr A/B");
    clearSnapshotsButton.onClick = [this]
    {
        if (referenceCaptureInProgress)
            return;

        snapshotA = {};
        snapshotB = {};
        hasSnapshotA = false;
        hasSnapshotB = false;
        audioProcessor.clearStoredSnapshots();
    };
    addAndMakeVisible(clearSnapshotsButton);

    resetButton.setButtonText("Reset");
    resetButton.onClick = [this]
    {
        audioProcessor.requestAnalyzerReset();
        hasDisplayMetrics = false;
        assessmentCountdown = 0;
    };
    addAndMakeVisible(resetButton);

    copyButton.setButtonText("Copy");
    copyButton.onClick = [this] { copyReportToClipboard(); };
    addAndMakeVisible(copyButton);

    genreAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "genre", genreBox);
    instrumentalAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, "instrumental", instrumentalToggle);
    syncStoredSnapshotsFromProcessor();

    setSize(1440, 940);
    startTimerHz(6);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::timerCallback()
{
    smoothDisplayMetrics(audioProcessor.getMetrics());
    if (! referenceCaptureInProgress)
        syncStoredSnapshotsFromProcessor();

    passButton.setButtonText(metrics.fullPassActive ? "Finish Pass" : "Start Pass");
    passButton.setEnabled(! referenceCaptureInProgress);
    referenceButton.setEnabled(referenceCaptureInProgress || ! metrics.fullPassActive);
    clearReferenceButton.setEnabled(! referenceCaptureInProgress && hasReferenceMetrics);
    snapshotAButton.setEnabled(! referenceCaptureInProgress);
    snapshotBButton.setEnabled(! referenceCaptureInProgress);
    clearSnapshotsButton.setEnabled(! referenceCaptureInProgress && (hasSnapshotA || hasSnapshotB));
    referenceButton.setButtonText(referenceCaptureInProgress && metrics.fullPassActive ? "Finish Ref"
                                  : referenceCaptureInProgress ? "Saving Ref"
                                  : hasReferenceMetrics ? "Recapture Ref"
                                                        : "Capture Ref");

    if (pendingReferenceSnapshot && ! metrics.fullPassActive && metrics.analysisFrozen && metrics.fullPassCompleted)
    {
        referenceMetrics = audioProcessor.getMetrics();
        hasReferenceMetrics = true;
        audioProcessor.storeReferenceMetrics(referenceMetrics);
        referenceCaptureInProgress = false;
        pendingReferenceSnapshot = false;
        audioProcessor.requestAnalyzerReset();
        hasDisplayMetrics = false;
        assessmentCountdown = 0;
    }

    const auto genreIndex = genreBox.getSelectedItemIndex();
    const auto instrumentalState = instrumentalToggle.getToggleState();
    if (--assessmentCountdown <= 0 || genreIndex != lastGenreIndex || instrumentalState != lastInstrumentalState)
    {
        assessment = fmma::assessMix(makeAssessmentInput(), fmma::getGenreProfile(genreIndex));
        assessmentCountdown = 6;
        lastGenreIndex = genreIndex;
        lastInstrumentalState = instrumentalState;
    }

    repaint();
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::smoothDisplayMetrics(const fmma::AnalyzerMetrics& raw)
{
    if (! hasDisplayMetrics || raw.analysisFrozen)
    {
        metrics = raw;
        hasDisplayMetrics = true;
        return;
    }

    constexpr auto slow = 0.12f;
    constexpr auto medium = 0.18f;
    metrics.momentaryLufs = smoothValue(metrics.momentaryLufs, raw.momentaryLufs, medium, 0.05f);
    metrics.shortTermLufs = smoothValue(metrics.shortTermLufs, raw.shortTermLufs, slow, 0.04f);
    metrics.integratedLufs = smoothValue(metrics.integratedLufs, raw.integratedLufs, 0.08f, 0.03f);
    metrics.truePeakDb = smoothValue(metrics.truePeakDb, raw.truePeakDb, medium, 0.04f);
    metrics.truePeakHoldDb = raw.truePeakHoldDb;
    metrics.worstTruePeakDb = raw.worstTruePeakDb;
    metrics.lraLu = smoothValue(metrics.lraLu, raw.lraLu, 0.08f, 0.05f);
    metrics.rmsDb = smoothValue(metrics.rmsDb, raw.rmsDb, slow, 0.04f);
    metrics.peakDb = smoothValue(metrics.peakDb, raw.peakDb, medium, 0.05f);
    metrics.crestDb = smoothValue(metrics.crestDb, raw.crestDb, slow, 0.05f);
    metrics.leftPeakDb = smoothValue(metrics.leftPeakDb, raw.leftPeakDb, medium, 0.05f);
    metrics.rightPeakDb = smoothValue(metrics.rightPeakDb, raw.rightPeakDb, medium, 0.05f);
    metrics.monoPeakDb = smoothValue(metrics.monoPeakDb, raw.monoPeakDb, medium, 0.05f);
    metrics.monoRmsDb = smoothValue(metrics.monoRmsDb, raw.monoRmsDb, slow, 0.04f);
    metrics.monoLossDb = smoothValue(metrics.monoLossDb, raw.monoLossDb, 0.08f, 0.03f);
    metrics.correlation = smoothValue(metrics.correlation, raw.correlation, 0.07f, 0.005f);
    metrics.worstCorrelation = raw.worstCorrelation;
    metrics.worstMonoLossDb = raw.worstMonoLossDb;
    metrics.widthPct = smoothValue(metrics.widthPct, raw.widthPct, 0.07f, 0.15f);
    metrics.msRatioDb = smoothValue(metrics.msRatioDb, raw.msRatioDb, 0.07f, 0.05f);
    metrics.stereoBalanceDb = smoothValue(metrics.stereoBalanceDb, raw.stereoBalanceDb, 0.08f, 0.03f);
    metrics.dcOffset = smoothValue(metrics.dcOffset, raw.dcOffset, 0.05f, 0.00002f);
    metrics.clippedPercent = smoothValue(metrics.clippedPercent, raw.clippedPercent, 0.05f, 0.001f);
    metrics.worstClippedPercent = raw.worstClippedPercent;
    metrics.silencePercent = smoothValue(metrics.silencePercent, raw.silencePercent, 0.05f, 0.05f);
    metrics.transientDensity = smoothValue(metrics.transientDensity, raw.transientDensity, 0.09f, 0.05f);
    metrics.attackTimeMs = smoothValue(metrics.attackTimeMs, raw.attackTimeMs, 0.07f, 0.3f);
    metrics.percussionEnergyPct = smoothValue(metrics.percussionEnergyPct, raw.percussionEnergyPct, 0.08f, 0.1f);
    metrics.spectralCentroidHz = smoothValue(metrics.spectralCentroidHz, raw.spectralCentroidHz, 0.07f, 10.0f);
    metrics.spectralRolloffHz = smoothValue(metrics.spectralRolloffHz, raw.spectralRolloffHz, 0.07f, 10.0f);
    metrics.resonanceFreqHz = smoothValue(metrics.resonanceFreqHz, raw.resonanceFreqHz, 0.06f, 25.0f);
    metrics.resonanceGainDb = smoothValue(metrics.resonanceGainDb, raw.resonanceGainDb, 0.07f, 0.05f);
    metrics.worstResonanceFreqHz = raw.worstResonanceFreqHz;
    metrics.worstResonanceGainDb = raw.worstResonanceGainDb;
    metrics.worstLowMidPercent = raw.worstLowMidPercent;
    metrics.analysisSeconds = raw.analysisSeconds;
    metrics.fullPassSeconds = raw.fullPassSeconds;
    metrics.fullPassActive = raw.fullPassActive;
    metrics.fullPassCompleted = raw.fullPassCompleted;
    metrics.analysisFrozen = raw.analysisFrozen;
    metrics.hostTransportPlaying = raw.hostTransportPlaying;
    metrics.hostAutoPassActive = raw.hostAutoPassActive;

    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        metrics.bandPercents[index] = smoothValue(metrics.bandPercents[index], raw.bandPercents[index], 0.07f, 0.08f);
        metrics.bandCorrelations[index] = smoothValue(metrics.bandCorrelations[index], raw.bandCorrelations[index], 0.08f, 0.004f);
        metrics.bandSideRatiosDb[index] = smoothValue(metrics.bandSideRatiosDb[index], raw.bandSideRatiosDb[index], 0.08f, 0.05f);
    }
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::syncStoredSnapshotsFromProcessor()
{
    fmma::AnalyzerMetrics stored;
    hasReferenceMetrics = audioProcessor.getStoredReferenceMetrics(stored);
    if (hasReferenceMetrics)
        referenceMetrics = stored;

    hasSnapshotA = audioProcessor.getStoredSnapshotA(stored);
    if (hasSnapshotA)
        snapshotA = stored;

    hasSnapshotB = audioProcessor.getStoredSnapshotB(stored);
    if (hasSnapshotB)
        snapshotB = stored;
}

fmma::MixAssessmentInput FunkyMooseMixAnalyzerAudioProcessorEditor::makeAssessmentInput() const
{
    fmma::MixAssessmentInput input;
    input.integratedLufs = metrics.integratedLufs;
    input.crestDb = metrics.crestDb;
    input.correlation = metrics.correlation;
    input.msRatioDb = metrics.msRatioDb;
    input.monoLossDb = metrics.monoLossDb;
    input.truePeakDb = metrics.truePeakDb;
    input.clippedPercent = metrics.clippedPercent;
    input.worstTruePeakDb = metrics.worstTruePeakDb;
    input.worstCorrelation = metrics.worstCorrelation;
    input.worstMonoLossDb = metrics.worstMonoLossDb;
    input.worstClippedPercent = metrics.worstClippedPercent;
    input.worstLowMidPercent = metrics.worstLowMidPercent;
    input.worstResonanceFreqHz = metrics.worstResonanceFreqHz;
    input.worstResonanceGainDb = metrics.worstResonanceGainDb;
    input.subPercent = metrics.bandPercents[0];
    input.bassPercent = metrics.bandPercents[1];
    input.lowMidPercent = metrics.bandPercents[2];
    input.midPercent = metrics.bandPercents[3];
    input.lowEndPercent = metrics.bandPercents[0] + metrics.bandPercents[1];
    input.presencePercent = metrics.bandPercents[4];
    input.airPercent = metrics.bandPercents[5];
    input.lowEndCorrelation = lowEndCorrelationOf(metrics);
    input.lowEndSideDb = lowEndSideDbOf(metrics);
    input.widthPct = metrics.widthPct;
    input.lraLu = metrics.lraLu;
    input.transientDensity = metrics.transientDensity;
    input.attackTimeMs = metrics.attackTimeMs;
    input.spectralCentroidHz = metrics.spectralCentroidHz;
    input.spectralRolloffHz = metrics.spectralRolloffHz;
    input.resonanceFreqHz = metrics.resonanceFreqHz;
    input.resonanceGainDb = metrics.resonanceGainDb;
    input.analysisSeconds = metrics.analysisSeconds;
    input.fullPassSeconds = metrics.fullPassSeconds;
    input.fullPassActive = metrics.fullPassActive;
    input.fullPassCompleted = metrics.fullPassCompleted;
    input.analysisFrozen = metrics.analysisFrozen;
    input.instrumental = instrumentalToggle.getToggleState();
    return input;
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatDuration(float seconds) const
{
    if (! std::isfinite(seconds) || seconds <= 0.0f)
        return "0:00";

    const auto totalSeconds = static_cast<int>(std::round(seconds));
    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;
    return juce::String(minutes) + ":" + juce::String(remainingSeconds).paddedLeft('0', 2);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatSigned(float value,
                                                                     const juce::String& suffix,
                                                                     int decimals) const
{
    if (! std::isfinite(value))
        return "N/A";

    return (value >= 0.0f ? "+" : "") + juce::String(value, decimals) + suffix;
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatDeliveryPreview(float targetLufs) const
{
    const auto truePeakForDelivery = metrics.fullPassCompleted ? juce::jmax(metrics.truePeakDb, metrics.worstTruePeakDb)
                                                               : metrics.truePeakDb;
    if (metrics.integratedLufs <= -119.0f || truePeakForDelivery <= -119.0f
        || ! std::isfinite(metrics.integratedLufs) || ! std::isfinite(truePeakForDelivery))
    {
        return "N/A";
    }

    const auto gainDelta = targetLufs - metrics.integratedLufs;
    const auto normalisedTruePeak = truePeakForDelivery + gainDelta;
    return formatSigned(gainDelta, " dB") + " / " + juce::String(normalisedTruePeak, 1) + " dBTP";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::referenceNote() const
{
    if (! hasReferenceMetrics)
        return "No reference captured.";

    if (! assessment.measurementReady)
        return "Play the current mix to compare against the captured reference.";

    const auto lufsDelta = metrics.integratedLufs - referenceMetrics.integratedLufs;
    const auto lowDelta = lowEndOf(metrics) - lowEndOf(referenceMetrics);
    const auto presenceDelta = presenceOf(metrics) - presenceOf(referenceMetrics);
    const auto crestDelta = metrics.crestDb - referenceMetrics.crestDb;
    const auto widthDelta = metrics.widthPct - referenceMetrics.widthPct;

    if (std::isfinite(lufsDelta) && std::abs(lufsDelta) > 1.5f)
        return lufsDelta > 0.0f ? "Louder than reference; check limiter drive."
                                : "Quieter than reference; compare perceived loudness.";
    if (std::isfinite(lowDelta) && std::abs(lowDelta) > 5.0f)
        return lowDelta > 0.0f ? "More low-end than reference; check kick/bass balance."
                               : "Less low-end than reference; check bass weight.";
    if (std::isfinite(presenceDelta) && std::abs(presenceDelta) > 5.0f)
        return presenceDelta > 0.0f ? "More presence than reference; watch harshness."
                                    : "Less presence than reference; vocals may sit back.";
    if (std::isfinite(crestDelta) && std::abs(crestDelta) > 3.0f)
        return crestDelta > 0.0f ? "More dynamic than reference; control peaks if needed."
                                 : "Flatter than reference; restore transient punch.";
    if (std::isfinite(widthDelta) && std::abs(widthDelta) > 15.0f)
        return widthDelta > 0.0f ? "Wider than reference; verify mono translation."
                                 : "Narrower than reference; compare stereo depth.";

    return "Close to reference on core metrics.";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::referenceActionNote() const
{
    if (! hasReferenceMetrics || ! assessment.measurementReady)
        return {};

    const auto note = referenceNote();
    if (note.startsWithIgnoreCase("Close to reference"))
        return {};

    return "Reference: " + note;
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::snapshotNote() const
{
    if (! hasSnapshotA && ! hasSnapshotB)
        return "Capture A, then make a change and capture B.";
    if (hasSnapshotA && ! hasSnapshotB)
        return "A stored; capture B after your mix change.";
    if (! hasSnapshotA && hasSnapshotB)
        return "B stored; capture A to compare against it.";

    const auto lufsDelta = snapshotB.integratedLufs - snapshotA.integratedLufs;
    const auto truePeakDelta = snapshotB.truePeakDb - snapshotA.truePeakDb;
    const auto lowDelta = lowEndOf(snapshotB) - lowEndOf(snapshotA);
    const auto presenceDelta = presenceOf(snapshotB) - presenceOf(snapshotA);
    const auto crestDelta = snapshotB.crestDb - snapshotA.crestDb;
    const auto widthDelta = snapshotB.widthPct - snapshotA.widthPct;

    if ((std::isfinite(snapshotB.truePeakDb) && snapshotB.truePeakDb > -1.0f)
        || (std::isfinite(truePeakDelta) && truePeakDelta > 1.0f && snapshotB.truePeakDb > -3.0f))
        return "B is hotter; check limiter ceiling and clipping risk.";
    if (std::isfinite(lufsDelta) && std::abs(lufsDelta) > 1.5f)
        return "Level-match A/B before judging tone or width.";
    if (std::isfinite(crestDelta) && crestDelta < -2.0f)
        return "B is flatter; check transient punch and compression.";
    if (std::isfinite(lowDelta) && std::abs(lowDelta) > 4.0f)
        return lowDelta > 0.0f ? "B gained low-end; check bass translation."
                               : "B lost low-end; check weight on small speakers.";
    if (std::isfinite(presenceDelta) && std::abs(presenceDelta) > 4.0f)
        return presenceDelta > 0.0f ? "B is more forward; watch harshness."
                                    : "B is less forward; check vocal/instrument focus.";
    if (std::isfinite(widthDelta) && std::abs(widthDelta) > 12.0f)
        return widthDelta > 0.0f ? "B is wider; verify mono compatibility."
                                 : "B is narrower; check stereo depth.";

    return "B stayed close to A on core mix metrics.";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::mixDoctorSummary() const
{
    if (! assessment.measurementReady)
        return "Measurement is still building; run a pass before final judgement.";

    juce::String summary = assessment.verdictTitle + ": ";
    summary << (assessment.priorityActionCount > 0
                    ? assessment.priorityActions[0]
                    : juce::String("Core metrics are in range for the selected profile."));

    const auto refAction = referenceActionNote();
    if (refAction.isNotEmpty())
        summary << " " << refAction;

    return summary;
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatDb(float value, int decimals)
{
    if (value <= -119.0f || ! std::isfinite(value))
        return "N/A";
    return juce::String(value, decimals) + " dB";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatDbTp(float value, int decimals)
{
    if (value <= -119.0f || ! std::isfinite(value))
        return "N/A";
    return juce::String(value, decimals) + " dBTP";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatLufs(float value, int decimals)
{
    if (value <= -119.0f || ! std::isfinite(value))
        return "N/A";
    return juce::String(value, decimals) + " LUFS";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatNumber(float value, int decimals)
{
    if (! std::isfinite(value))
        return "N/A";
    return juce::String(value, decimals);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);

    auto bounds = getLocalBounds().toFloat().reduced(24.0f);
    auto header = bounds.removeFromTop(72.0f);

    g.setColour(teal);
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawText("Funky Moose Mix Analyzer", header.removeFromTop(34.0f), juce::Justification::centredLeft);

    g.setColour(muted);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText("Live mix score, genre targets, loudness, stereo and DAW notes for VST3 / AU",
               header,
               juce::Justification::centredLeft);

    bounds.removeFromTop(14.0f);
    drawSummary(g, bounds.removeFromTop(124.0f));

    bounds.removeFromTop(18.0f);
    auto rowOne = bounds.removeFromTop(190.0f);
    const auto third = (rowOne.getWidth() - 48.0f) / 3.0f;
    auto loudness = rowOne.removeFromLeft(third);
    rowOne.removeFromLeft(24.0f);
    auto dynamics = rowOne.removeFromLeft(third);
    rowOne.removeFromLeft(24.0f);
    auto stereo = rowOne;

    drawCard(g, loudness, "Loudness");
    auto loudnessInner = loudness.reduced(18.0f);
    loudnessInner.removeFromTop(34.0f);
    auto loudnessBar = loudnessInner.removeFromBottom(18.0f);
    drawMetric(g, loudnessInner.removeFromTop(20.0f), "Momentary", formatLufs(metrics.momentaryLufs));
    drawMetric(g, loudnessInner.removeFromTop(20.0f), "Short-Term", formatLufs(metrics.shortTermLufs));
    drawMetric(g, loudnessInner.removeFromTop(20.0f), "Integrated", formatLufs(metrics.integratedLufs));
    drawMetric(g, loudnessInner.removeFromTop(20.0f), "LRA", juce::String(metrics.lraLu, 1) + " LU");
    drawMetric(g, loudnessInner.removeFromTop(20.0f), "True Peak", formatDbTp(metrics.truePeakDb));
    drawBar(g, loudnessBar, dbToUnit(metrics.truePeakDb),
            metrics.truePeakDb > -1.0f ? danger : teal);

    drawCard(g, dynamics, "Dynamics");
    auto dynInner = dynamics.reduced(18.0f);
    dynInner.removeFromTop(34.0f);
    auto dynBar = dynInner.removeFromBottom(18.0f);
    drawMetric(g, dynInner.removeFromTop(20.0f), "RMS", formatDb(metrics.rmsDb));
    drawMetric(g, dynInner.removeFromTop(20.0f), "Sample Peak", formatDb(metrics.peakDb));
    drawMetric(g, dynInner.removeFromTop(20.0f), "Crest", formatDb(metrics.crestDb));
    drawMetric(g, dynInner.removeFromTop(20.0f), "Trans / Attack",
               juce::String(metrics.transientDensity, 1) + "/s / " + juce::String(metrics.attackTimeMs, 0) + " ms");
    drawMetric(g, dynInner.removeFromTop(20.0f), "Percussion", juce::String(metrics.percussionEnergyPct, 1) + "%");
    drawBar(g, dynBar, dbToUnit(metrics.peakDb),
            metrics.peakDb > -1.0f ? danger : teal);

    drawCard(g, stereo, "Stereo");
    auto stereoInner = stereo.reduced(18.0f);
    stereoInner.removeFromTop(34.0f);
    auto stereoBar = stereoInner.removeFromBottom(18.0f);
    drawMetric(g, stereoInner.removeFromTop(20.0f), "Correlation", formatNumber(metrics.correlation, 2));
    drawMetric(g, stereoInner.removeFromTop(20.0f), "Width", formatNumber(metrics.widthPct, 1) + "%");
    drawMetric(g, stereoInner.removeFromTop(20.0f), "M/S Ratio", formatDb(metrics.msRatioDb, 2));
    drawMetric(g, stereoInner.removeFromTop(20.0f), "Mono Loss", formatSigned(metrics.monoLossDb, " dB"));
    drawMetric(g, stereoInner.removeFromTop(20.0f), "Low Phase",
               "C " + formatNumber(lowEndCorrelationOf(metrics), 2)
                   + " / S " + formatDb(lowEndSideDbOf(metrics), 1));
    drawMetric(g, stereoInner.removeFromTop(20.0f), "Balance L/R", formatDb(metrics.stereoBalanceDb, 2));
    drawBar(g, stereoBar, (metrics.correlation + 1.0f) * 0.5f,
            metrics.monoLossDb < -4.0f || metrics.correlation < 0.3f || lowEndCorrelationOf(metrics) < 0.65f ? danger
                : metrics.monoLossDb < -2.5f || metrics.correlation < 0.55f || lowEndSideDbOf(metrics) > -6.0f ? amber
                                                                                                                : teal);

    bounds.removeFromTop(18.0f);
    auto rowTwo = bounds.removeFromTop(190.0f);
    auto targets = rowTwo.removeFromLeft(third);
    rowTwo.removeFromLeft(24.0f);
    auto quality = rowTwo.removeFromLeft(third);
    rowTwo.removeFromLeft(24.0f);
    auto bands = rowTwo;

    drawTargetChecklist(g, targets);

    drawCard(g, quality, "Safety / Delivery");
    auto qualityInner = quality.reduced(18.0f);
    qualityInner.removeFromTop(34.0f);
    auto qualityBar = qualityInner.removeFromBottom(18.0f);
    const auto truePeakForSafety = metrics.fullPassCompleted ? juce::jmax(metrics.truePeakDb, metrics.worstTruePeakDb)
                                                             : metrics.truePeakDb;
    const auto hasTruePeak = truePeakForSafety > -119.0f && std::isfinite(truePeakForSafety);
    const auto truePeakMargin = hasTruePeak ? -1.0f - truePeakForSafety : 0.0f;
    constexpr auto safetyRowHeight = 14.5f;
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "Clipping", juce::String(metrics.clippedPercent, 3) + "%");
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "Worst Clip", juce::String(metrics.worstClippedPercent, 3) + "%");
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "TP Margin", hasTruePeak ? formatSigned(truePeakMargin, " dB") : "N/A");
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "Worst TP", formatDbTp(metrics.worstTruePeakDb));
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "Stream -14", formatDeliveryPreview(-14.0f));
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "Apple -16", formatDeliveryPreview(-16.0f));
    drawMetric(g, qualityInner.removeFromTop(safetyRowHeight), "Broadcast", formatDeliveryPreview(-23.0f));
    drawBar(g, qualityBar,
            hasTruePeak ? juce::jlimit(0.0f, 1.0f, (truePeakMargin + 1.0f) / 5.0f) : 0.0f,
            hasTruePeak ? (truePeakMargin < 0.0f ? danger : truePeakMargin < 1.0f ? amber : success) : muted);

    drawCard(g, bands, "Tone Shape");
    auto toneInner = bands.reduced(18.0f);
    toneInner.removeFromTop(34.0f);
    drawToneShape(g, toneInner);

    bounds.removeFromTop(18.0f);
    auto bottom = bounds;
    const auto bottomGap = 18.0f;
    const auto availableBottomWidth = bounds.getWidth() - (bottomGap * 3.0f);
    const auto scoreWidth = availableBottomWidth * 0.29f;
    const auto referenceWidth = availableBottomWidth * 0.23f;
    const auto snapshotWidth = availableBottomWidth * 0.23f;
    auto scoreBox = bottom.removeFromLeft(scoreWidth);
    bottom.removeFromLeft(bottomGap);
    auto referenceBox = bottom.removeFromLeft(referenceWidth);
    bottom.removeFromLeft(bottomGap);
    auto snapshotBox = bottom.removeFromLeft(snapshotWidth);
    bottom.removeFromLeft(bottomGap);
    drawScoreComponents(g, scoreBox);
    drawReferenceCompare(g, referenceBox);
    drawSnapshotCompare(g, snapshotBox);
    drawPriorityActions(g, bottom);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::resized()
{
    auto header = getLocalBounds().reduced(24).removeFromTop(34);
    auto controls = header.removeFromRight(1030);
    genreLabel.setBounds(controls.removeFromLeft(86));
    genreBox.setBounds(controls.removeFromLeft(180).reduced(0, 1));
    controls.removeFromLeft(7);
    instrumentalToggle.setBounds(controls.removeFromLeft(104));
    controls.removeFromLeft(7);
    passButton.setBounds(controls.removeFromLeft(94).reduced(0, 1));
    controls.removeFromLeft(7);
    referenceButton.setBounds(controls.removeFromLeft(98).reduced(0, 1));
    controls.removeFromLeft(7);
    clearReferenceButton.setBounds(controls.removeFromLeft(72).reduced(0, 1));
    controls.removeFromLeft(7);
    snapshotAButton.setBounds(controls.removeFromLeft(62).reduced(0, 1));
    controls.removeFromLeft(7);
    snapshotBButton.setBounds(controls.removeFromLeft(62).reduced(0, 1));
    controls.removeFromLeft(7);
    clearSnapshotsButton.setBounds(controls.removeFromLeft(74).reduced(0, 1));
    controls.removeFromLeft(7);
    resetButton.setBounds(controls.removeFromLeft(58).reduced(0, 1));
    controls.removeFromLeft(7);
    copyButton.setBounds(controls.removeFromLeft(58).reduced(0, 1));
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawSummary(juce::Graphics& g,
                                                            juce::Rectangle<float> area)
{
    drawCard(g, area, "Live Summary");

    auto inner = area.reduced(18.0f);
    inner.removeFromTop(26.0f);
    auto scoreArea = inner.removeFromLeft(128.0f);
    auto details = inner;
    const auto colour = scoreColour(assessment.overallScore);

    g.setColour(colour.withAlpha(0.12f));
    g.fillRoundedRectangle(scoreArea.reduced(0.0f, 3.0f), 8.0f);
    g.setColour(colour);
    g.setFont(juce::FontOptions(42.0f, juce::Font::bold));
    g.drawText(juce::String(assessment.overallScore), scoreArea.removeFromTop(48.0f), juce::Justification::centred);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("MIX SCORE", scoreArea, juce::Justification::centred);

    details.removeFromLeft(18.0f);
    auto topLine = details.removeFromTop(24.0f);
    g.setColour(colour);
    g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    g.drawText(assessment.verdictTitle, topLine.removeFromLeft(170.0f), juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::FontOptions(13.0f));
    const auto scopeLabel = metrics.hostAutoPassActive ? juce::String("Host Recording") : assessment.analysisScope;
    g.drawText(scopeLabel + " / " + fmma::getGenreProfile(genreBox.getSelectedItemIndex()).group,
               topLine,
               juce::Justification::centredLeft);

    g.setColour(text);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(assessment.statusLine, details.removeFromTop(20.0f), juce::Justification::centredLeft);

    auto chips = details.removeFromTop(22.0f);
    const auto input = makeAssessmentInput();
    const auto duration = metrics.fullPassCompleted ? metrics.fullPassSeconds : metrics.analysisSeconds;
    drawMetric(g, chips.removeFromLeft(180.0f), "Confidence",
               assessment.confidenceLabel + " " + juce::String(assessment.confidenceScore));
    drawMetric(g, chips.removeFromLeft(150.0f), "Time", formatDuration(duration));
    drawMetric(g, chips.removeFromLeft(170.0f), "LUFS delta", juce::String(assessment.lufsDelta, 1));
    drawMetric(g, chips.removeFromLeft(170.0f), "Low-end", juce::String(input.lowEndPercent, 1) + "%");
    drawMetric(g, chips.removeFromLeft(170.0f), "Presence", juce::String(input.presencePercent, 1) + "%");
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawCard(juce::Graphics& g,
                                                         juce::Rectangle<float> area,
                                                         const juce::String& title)
{
    g.setColour(panel);
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(border);
    g.drawRoundedRectangle(area, 8.0f, 1.0f);
    g.setColour(text);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText(title, area.reduced(18.0f).removeFromTop(24.0f), juce::Justification::centredLeft);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawMetric(juce::Graphics& g,
                                                           juce::Rectangle<float> area,
                                                           const juce::String& label,
                                                           const juce::String& value)
{
    g.setFont(juce::FontOptions(13.0f));
    g.setColour(muted);
    g.drawText(label, area.removeFromLeft(area.getWidth() * 0.56f), juce::Justification::centredLeft);
    g.setColour(text);
    g.drawText(value, area, juce::Justification::centredRight);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawBar(juce::Graphics& g,
                                                        juce::Rectangle<float> area,
                                                        float normalised,
                                                        juce::Colour colour)
{
    area = area.withHeight(10.0f).withCentre(area.getCentre());
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(area, 5.0f);
    g.setColour(colour);
    g.fillRoundedRectangle(area.withWidth(area.getWidth() * juce::jlimit(0.0f, 1.0f, normalised)), 5.0f);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawToneShape(juce::Graphics& g,
                                                              juce::Rectangle<float> area)
{
    auto graph = area.removeFromTop(82.0f);
    auto labelStrip = graph.removeFromBottom(15.0f);
    auto plot = graph.reduced(0.0f, 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(plot, 6.0f);
    g.setColour(border);
    g.drawRoundedRectangle(plot, 6.0f, 1.0f);

    for (auto i = 1; i < 4; ++i)
    {
        const auto y = plot.getY() + (plot.getHeight() * static_cast<float>(i) / 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine(static_cast<int>(std::round(y)), plot.getX(), plot.getRight());
    }

    juce::Path fillPath;
    juce::Path linePath;
    const auto step = plot.getWidth() / static_cast<float>(fmma::bandCount - 1);

    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        const auto value = juce::jlimit(0.0f, 1.0f, metrics.bandPercents[static_cast<size_t>(band)] / 45.0f);
        const auto x = plot.getX() + (static_cast<float>(band) * step);
        const auto y = plot.getBottom() - (value * plot.getHeight());

        if (band == 0)
        {
            linePath.startNewSubPath(x, y);
            fillPath.startNewSubPath(x, plot.getBottom());
            fillPath.lineTo(x, y);
        }
        else
        {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }

        g.setColour((band < 2 ? teal : band < 4 ? violet : amber).withAlpha(0.88f));
        g.fillEllipse(juce::Rectangle<float> { x - 3.0f, y - 3.0f, 6.0f, 6.0f });
    }

    fillPath.lineTo(plot.getRight(), plot.getBottom());
    fillPath.closeSubPath();
    g.setColour(teal.withAlpha(0.14f));
    g.fillPath(fillPath);
    g.setColour(teal);
    g.strokePath(linePath, juce::PathStrokeType(2.0f));

    const auto drawFrequencyMarker = [&] (float frequencyHz, juce::Colour colour, const juce::String& label)
    {
        if (! std::isfinite(frequencyHz) || frequencyHz <= 0.0f)
            return;

        const auto x = plot.getX() + (frequencyToUnit(frequencyHz) * plot.getWidth());
        g.setColour(colour.withAlpha(0.75f));
        g.drawVerticalLine(static_cast<int>(std::round(x)), plot.getY(), plot.getBottom());
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawFittedText(label, juce::Rectangle<int> { static_cast<int>(x) + 3,
                                                       static_cast<int>(plot.getY()) + 2,
                                                       54,
                                                       12 },
                         juce::Justification::centredLeft,
                         1);
    };

    drawFrequencyMarker(metrics.spectralCentroidHz, teal, "C");
    drawFrequencyMarker(metrics.spectralRolloffHz, amber, "R");
    if (metrics.resonanceFreqHz > 0.0f && metrics.resonanceGainDb >= 6.0f)
        drawFrequencyMarker(metrics.resonanceFreqHz, danger, "Res");

    const auto labelWidth = labelStrip.getWidth() / static_cast<float>(fmma::bandCount);
    for (auto band = 0; band < fmma::bandCount; ++band)
    {
        auto labelArea = labelStrip.removeFromLeft(labelWidth);
        g.setColour(muted);
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(bandNames[static_cast<size_t>(band)], labelArea, juce::Justification::centred);
    }

    area.removeFromTop(8.0f);
    drawMetric(g, area.removeFromTop(18.0f), "Centroid", formatHz(metrics.spectralCentroidHz));
    drawMetric(g, area.removeFromTop(18.0f), "Rolloff", formatHz(metrics.spectralRolloffHz));
    drawMetric(g, area.removeFromTop(18.0f), "Resonance",
               metrics.resonanceFreqHz > 0.0f && metrics.resonanceGainDb >= 6.0f
                    ? formatHz(metrics.resonanceFreqHz) + " / +" + juce::String(metrics.resonanceGainDb, 1)
                    : "N/A");
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawTargetChecklist(juce::Graphics& g,
                                                                    juce::Rectangle<float> area)
{
    drawCard(g, area, "Targets");
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    const std::array<std::pair<juce::String, bool>, 7> rows {{
        { assessment.lufsTargetText, assessment.lufsOk },
        { assessment.lowEndTargetText, assessment.lowEndOk },
        { "Low phase: corr >= 0.65, side <= -6 dB", assessment.lowEndPhaseOk },
        { assessment.crestTargetText, assessment.crestOk },
        { assessment.lraTargetText, assessment.lraOk },
        { assessment.correlationTargetText, assessment.correlationOk },
        { "True Peak <= -1.0 dBTP", assessment.truePeakOk && assessment.clippingOk },
    }};

    for (const auto& row : rows)
    {
        auto line = inner.removeFromTop(16.7f);
        g.setColour(okColour(row.second));
        g.fillEllipse(line.removeFromLeft(10.0f).reduced(1.0f));
        line.removeFromLeft(8.0f);
        g.setColour(row.second ? text : amber);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(row.first, line, juce::Justification::centredLeft);
    }
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawScoreComponents(juce::Graphics& g,
                                                                    juce::Rectangle<float> area)
{
    drawCard(g, area, "Score Components");
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    const std::array<std::pair<const char*, int>, 10> rows {{
        { "LUFS", assessment.lufsScore },
        { "Correlation", assessment.correlationScore },
        { "Low-End", assessment.lowEndScore },
        { "Crest", assessment.crestScore },
        { "LRA", assessment.lraScore },
        { "Clipping", assessment.clippingScore },
        { "Tone", assessment.toneScore },
        { "Headphones", assessment.headphoneScore },
        { "Speakers", assessment.speakerScore },
        { "Transients", assessment.transientScore },
    }};

    auto leftColumn = inner.removeFromLeft((inner.getWidth() - 14.0f) * 0.5f);
    inner.removeFromLeft(14.0f);
    auto rightColumn = inner;
    const auto rowsPerColumn = (rows.size() + size_t { 1 }) / size_t { 2 };

    for (auto i = size_t { 0 }; i < rows.size(); ++i)
    {
        auto& column = i < rowsPerColumn ? leftColumn : rightColumn;
        const auto& row = rows[i];
        auto line = column.removeFromTop(22.0f);
        auto label = line.removeFromLeft(72.0f);
        auto value = line.removeFromRight(34.0f);
        g.setColour(muted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(row.first, label, juce::Justification::centredLeft);
        g.setColour(scoreColour(row.second));
        g.drawText(juce::String(row.second), value, juce::Justification::centredRight);
        drawBar(g, line.reduced(8.0f, 0.0f), static_cast<float>(row.second) / 100.0f, scoreColour(row.second));
    }
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawReferenceCompare(juce::Graphics& g,
                                                                     juce::Rectangle<float> area)
{
    drawCard(g, area, "Reference");
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    if (! hasReferenceMetrics)
    {
        g.setColour(muted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("No reference captured.", inner.removeFromTop(24.0f), juce::Justification::centredLeft);
        drawMetric(g, inner.removeFromTop(22.0f), "State",
                   referenceCaptureInProgress ? "Recording" : "Empty");
        drawMetric(g, inner.removeFromTop(22.0f), "Compare", "Unavailable");
        return;
    }

    const auto lufsDelta = metrics.integratedLufs - referenceMetrics.integratedLufs;
    const auto lowDelta = lowEndOf(metrics) - lowEndOf(referenceMetrics);
    const auto presenceDelta = presenceOf(metrics) - presenceOf(referenceMetrics);
    const auto crestDelta = metrics.crestDb - referenceMetrics.crestDb;
    const auto widthDelta = metrics.widthPct - referenceMetrics.widthPct;

    drawMetric(g, inner.removeFromTop(20.0f), "Ref Time",
               formatDuration(referenceMetrics.fullPassSeconds > 0.0f ? referenceMetrics.fullPassSeconds
                                                                       : referenceMetrics.analysisSeconds));
    drawMetric(g, inner.removeFromTop(20.0f), "LUFS Δ", formatSigned(lufsDelta, " LU"));
    drawMetric(g, inner.removeFromTop(20.0f), "Low-End Δ", formatSigned(lowDelta, "%"));
    drawMetric(g, inner.removeFromTop(20.0f), "Presence Δ", formatSigned(presenceDelta, "%"));
    drawMetric(g, inner.removeFromTop(20.0f), "Crest Δ", formatSigned(crestDelta, " dB"));
    drawMetric(g, inner.removeFromTop(20.0f), "Width Δ", formatSigned(widthDelta, "%"));

    inner.removeFromTop(4.0f);
    g.setColour(teal);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("Ref Note", inner.removeFromTop(18.0f), juce::Justification::centredLeft);
    g.setColour(text);
    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText(referenceNote(), inner.toNearestInt(), juce::Justification::centredLeft, 2);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawSnapshotCompare(juce::Graphics& g,
                                                                    juce::Rectangle<float> area)
{
    drawCard(g, area, "A/B Snapshots");
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    if (! hasSnapshotA || ! hasSnapshotB)
    {
        drawMetric(g, inner.removeFromTop(22.0f), "Snapshot A", hasSnapshotA ? "Stored" : "Empty");
        drawMetric(g, inner.removeFromTop(22.0f), "Snapshot B", hasSnapshotB ? "Stored" : "Empty");
        drawMetric(g, inner.removeFromTop(22.0f), "Compare", hasSnapshotA || hasSnapshotB ? "Waiting" : "Unavailable");

        inner.removeFromTop(8.0f);
        g.setColour(teal);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText("A/B Note", inner.removeFromTop(18.0f), juce::Justification::centredLeft);
        g.setColour(text);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText(snapshotNote(), inner.toNearestInt(), juce::Justification::centredLeft, 2);
        return;
    }

    drawMetric(g, inner.removeFromTop(20.0f), "A Time",
               formatDuration(snapshotA.fullPassSeconds > 0.0f ? snapshotA.fullPassSeconds
                                                                : snapshotA.analysisSeconds));
    drawMetric(g, inner.removeFromTop(20.0f), "B Time",
               formatDuration(snapshotB.fullPassSeconds > 0.0f ? snapshotB.fullPassSeconds
                                                                : snapshotB.analysisSeconds));
    drawMetric(g, inner.removeFromTop(20.0f), "LUFS B-A",
               formatSigned(snapshotB.integratedLufs - snapshotA.integratedLufs, " LU"));
    drawMetric(g, inner.removeFromTop(20.0f), "TP B-A",
               formatSigned(snapshotB.truePeakDb - snapshotA.truePeakDb, " dB"));
    drawMetric(g, inner.removeFromTop(20.0f), "Low B-A",
               formatSigned(lowEndOf(snapshotB) - lowEndOf(snapshotA), "%"));
    drawMetric(g, inner.removeFromTop(20.0f), "Crest B-A",
               formatSigned(snapshotB.crestDb - snapshotA.crestDb, " dB"));
    drawMetric(g, inner.removeFromTop(20.0f), "Width B-A",
               formatSigned(snapshotB.widthPct - snapshotA.widthPct, "%"));

    inner.removeFromTop(4.0f);
    g.setColour(teal);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("A/B Note", inner.removeFromTop(18.0f), juce::Justification::centredLeft);
    g.setColour(text);
    g.setFont(juce::FontOptions(12.0f));
    g.drawFittedText(snapshotNote(), inner.toNearestInt(), juce::Justification::centredLeft, 2);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::drawPriorityActions(juce::Graphics& g,
                                                                    juce::Rectangle<float> area)
{
    drawCard(g, area, "Priority Actions");
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    const auto refAction = referenceActionNote();
    const auto rowCount = juce::jmax(1, assessment.priorityActionCount + (refAction.isNotEmpty() ? 1 : 0));
    const auto rowHeight = juce::jmin(36.0f, inner.getHeight() / static_cast<float>(rowCount));
    for (auto i = 0; i < rowCount; ++i)
    {
        const auto action = i < assessment.priorityActionCount
            ? assessment.priorityActions[static_cast<size_t>(i)]
            : refAction.isNotEmpty()
                ? refAction
            : juce::String("Play audio to generate live recommendations.");
        auto line = inner.removeFromTop(rowHeight);
        g.setColour(i == 0 ? teal : muted);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1) + ".", line.removeFromLeft(24.0f), juce::Justification::centredLeft);
        g.setColour(text);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText(action, line.toNearestInt(), juce::Justification::centredLeft, 2);
    }
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::copyReportToClipboard()
{
    const auto& profile = fmma::getGenreProfile(genreBox.getSelectedItemIndex());
    const auto input = makeAssessmentInput();
    const auto truePeakForReport = metrics.fullPassCompleted ? juce::jmax(metrics.truePeakDb, metrics.worstTruePeakDb)
                                                             : metrics.truePeakDb;
    juce::String report;
    report << "FUNKY MOOSE MIX ANALYZER - LIVE PLUGIN REPORT\n";
    report << "Genre Profile: " << profile.name << "\n";
    report << "Mode: " << (input.instrumental ? "Instrumental" : "Vocal / Full Mix") << "\n";
    report << "Analysis Scope: " << (metrics.hostAutoPassActive ? "Host Recording" : assessment.analysisScope) << "\n";
    report << "Analysis Time: " << formatDuration(metrics.fullPassCompleted ? metrics.fullPassSeconds : metrics.analysisSeconds) << "\n";
    report << "Confidence: " << assessment.confidenceLabel << " (" << assessment.confidenceScore << "/100)\n";
    report << "Confidence Note: " << assessment.confidenceText << "\n";
    report << "Verdict: " << assessment.verdictTitle << "\n";
    report << "Mix Score: " << assessment.overallScore << "/100\n";
    report << "Mix Doctor Summary: " << mixDoctorSummary() << "\n";
    report << "Tone Score: " << assessment.toneScore << "/100\n";
    report << "LRA Score: " << assessment.lraScore << "/100\n";
    report << "Transient Score: " << assessment.transientScore << "/100\n";
    report << "Headphone Score: " << assessment.headphoneScore << "/100\n";
    report << "Speaker Score: " << assessment.speakerScore << "/100\n";
    report << "------------------------------------------\n";
    report << "Integrated LUFS: " << juce::String(metrics.integratedLufs, 1)
           << " (target " << juce::String(profile.targetLufs, 0) << ")\n";
    report << "True Peak: " << juce::String(metrics.truePeakDb, 1) << " dBTP\n";
    report << "Worst True Peak: " << juce::String(metrics.worstTruePeakDb, 1) << " dBTP\n";
    report << "Crest: " << juce::String(metrics.crestDb, 1) << " dB\n";
    report << "Correlation: " << juce::String(metrics.correlation, 2) << "\n";
    report << "Worst Correlation: " << juce::String(metrics.worstCorrelation, 2) << "\n";
    report << "Width: " << juce::String(metrics.widthPct, 1) << "%\n";
    report << "Mono RMS: " << formatDb(metrics.monoRmsDb) << "\n";
    report << "Mono Loss: " << formatSigned(metrics.monoLossDb, " dB") << "\n";
    report << "Worst Mono Loss: " << formatSigned(metrics.worstMonoLossDb, " dB") << "\n";
    report << "Low-End Phase: corr " << juce::String(input.lowEndCorrelation, 2)
           << ", side " << formatDb(input.lowEndSideDb) << "\n";
    report << "Low-End: " << juce::String(input.lowEndPercent, 1) << "%\n";
    report << "Presence: " << juce::String(input.presencePercent, 1) << "%\n";
    report << "LRA: " << juce::String(metrics.lraLu, 1) << " LU\n";
    report << "Transient Density: " << juce::String(metrics.transientDensity, 1) << "/s\n";
    report << "Attack: " << juce::String(metrics.attackTimeMs, 0) << " ms\n";
    report << "Percussive Energy: " << juce::String(metrics.percussionEnergyPct, 1) << "%\n";
    report << "Spectral Centroid: " << formatHz(metrics.spectralCentroidHz) << "\n";
    report << "Spectral Rolloff: " << formatHz(metrics.spectralRolloffHz) << "\n";
    report << "Dominant Resonance Area: ";
    if (metrics.resonanceFreqHz > 0.0f && metrics.resonanceGainDb >= 6.0f)
        report << formatHz(metrics.resonanceFreqHz) << " (+" << juce::String(metrics.resonanceGainDb, 1) << " dB)\n";
    else
        report << "None detected\n";
    report << "Worst Resonance Area: ";
    if (metrics.worstResonanceFreqHz > 0.0f && metrics.worstResonanceGainDb >= 6.0f)
        report << formatHz(metrics.worstResonanceFreqHz) << " (+" << juce::String(metrics.worstResonanceGainDb, 1) << " dB)\n";
    else
        report << "None detected\n";
    report << "Clipping: " << juce::String(metrics.clippedPercent, 3) << "%\n";
    report << "Worst Clipping: " << juce::String(metrics.worstClippedPercent, 3) << "%\n";
    report << "Worst Low-Mids: " << juce::String(metrics.worstLowMidPercent, 1) << "%\n";
    report << "Band Balance: ";
    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        if (i > 0)
            report << ", ";
        report << bandNames[static_cast<size_t>(i)] << " "
               << juce::String(metrics.bandPercents[static_cast<size_t>(i)], 1) << "%";
    }
    report << "\n";
    report << "Band Phase: ";
    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        if (i > 0)
            report << ", ";
        report << bandNames[static_cast<size_t>(i)] << " corr "
               << juce::String(metrics.bandCorrelations[static_cast<size_t>(i)], 2)
               << " / side " << juce::String(metrics.bandSideRatiosDb[static_cast<size_t>(i)], 1) << " dB";
    }
    report << "\n";

    const auto hasTruePeak = truePeakForReport > -119.0f && std::isfinite(truePeakForReport);
    report << "\nDELIVERY PREVIEW\n";
    report << "True Peak Margin to -1.0 dBTP: "
           << (hasTruePeak ? formatSigned(-1.0f - truePeakForReport, " dB") : juce::String("N/A")) << "\n";
    report << "Streaming -14 LUFS: gain / estimated TP " << formatDeliveryPreview(-14.0f) << "\n";
    report << "Apple -16 LUFS: gain / estimated TP " << formatDeliveryPreview(-16.0f) << "\n";
    report << "Broadcast -23 LUFS: gain / estimated TP " << formatDeliveryPreview(-23.0f) << "\n";

    if (hasReferenceMetrics)
    {
        report << "\nREFERENCE COMPARE\n";
        report << "Reference Time: "
               << formatDuration(referenceMetrics.fullPassSeconds > 0.0f ? referenceMetrics.fullPassSeconds
                                                                          : referenceMetrics.analysisSeconds)
               << "\n";
        report << "Reference LUFS: " << juce::String(referenceMetrics.integratedLufs, 1) << "\n";
        report << "LUFS Delta: " << formatSigned(metrics.integratedLufs - referenceMetrics.integratedLufs, " LU") << "\n";
        report << "Low-End Delta: " << formatSigned(lowEndOf(metrics) - lowEndOf(referenceMetrics), "%") << "\n";
        report << "Presence Delta: " << formatSigned(presenceOf(metrics) - presenceOf(referenceMetrics), "%") << "\n";
        report << "Crest Delta: " << formatSigned(metrics.crestDb - referenceMetrics.crestDb, " dB") << "\n";
        report << "Width Delta: " << formatSigned(metrics.widthPct - referenceMetrics.widthPct, "%") << "\n";
        report << "Reference Note: " << referenceNote() << "\n";
        if (referenceActionNote().isNotEmpty())
            report << "Reference Action: " << referenceActionNote() << "\n";
    }

    if (hasSnapshotA || hasSnapshotB)
    {
        report << "\nA/B SNAPSHOT COMPARE\n";
        report << "Snapshot A: " << (hasSnapshotA ? "stored" : "empty") << "\n";
        report << "Snapshot B: " << (hasSnapshotB ? "stored" : "empty") << "\n";

        if (hasSnapshotA && hasSnapshotB)
        {
            report << "A Time: "
                   << formatDuration(snapshotA.fullPassSeconds > 0.0f ? snapshotA.fullPassSeconds
                                                                      : snapshotA.analysisSeconds)
                   << "\n";
            report << "B Time: "
                   << formatDuration(snapshotB.fullPassSeconds > 0.0f ? snapshotB.fullPassSeconds
                                                                      : snapshotB.analysisSeconds)
                   << "\n";
            report << "LUFS Delta B-A: "
                   << formatSigned(snapshotB.integratedLufs - snapshotA.integratedLufs, " LU") << "\n";
            report << "True Peak Delta B-A: "
                   << formatSigned(snapshotB.truePeakDb - snapshotA.truePeakDb, " dB") << "\n";
            report << "Low-End Delta B-A: "
                   << formatSigned(lowEndOf(snapshotB) - lowEndOf(snapshotA), "%") << "\n";
            report << "Presence Delta B-A: "
                   << formatSigned(presenceOf(snapshotB) - presenceOf(snapshotA), "%") << "\n";
            report << "Crest Delta B-A: "
                   << formatSigned(snapshotB.crestDb - snapshotA.crestDb, " dB") << "\n";
            report << "Width Delta B-A: "
                   << formatSigned(snapshotB.widthPct - snapshotA.widthPct, "%") << "\n";
        }

        report << "A/B Note: " << snapshotNote() << "\n";
    }

    report << "\n";
    report << "Top Actions:\n";

    for (auto i = 0; i < assessment.priorityActionCount; ++i)
        report << juce::String(i + 1) << ". " << assessment.priorityActions[static_cast<size_t>(i)] << "\n";
    if (referenceActionNote().isNotEmpty())
        report << "R. " << referenceActionNote() << "\n";

    juce::SystemClipboard::copyTextToClipboard(report);
}
