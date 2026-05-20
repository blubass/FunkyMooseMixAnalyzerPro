#include "PluginEditor.h"

#include <cmath>

namespace
{
float dbToUnit(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, (value + 60.0f) / 60.0f);
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

juce::var jsonNumber(float value)
{
    return std::isfinite(value) ? juce::var(static_cast<double>(value)) : juce::var();
}

juce::var jsonAudioNumber(float value)
{
    return (std::isfinite(value) && value > -119.0f) ? juce::var(static_cast<double>(value)) : juce::var();
}

void setJsonProperty(juce::var& object, const char* name, const juce::var& value)
{
    if (auto* dynamicObject = object.getDynamicObject())
        dynamicObject->setProperty(juce::Identifier(name), value);
}
}

using namespace Theme;

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

    autoMasterToggle.setButtonText("Auto Master");
    autoMasterToggle.setColour(juce::ToggleButton::textColourId, muted);
    autoMasterToggle.setTooltip("Enable conservative automatic mastering output processing.");
    addAndMakeVisible(autoMasterToggle);

    autoMasterAuditionToggle.setButtonText("Match A/B");
    autoMasterAuditionToggle.setColour(juce::ToggleButton::textColourId, muted);
    autoMasterAuditionToggle.setTooltip("Audition Auto Master at matched loudness for honest A/B decisions.");
    addAndMakeVisible(autoMasterAuditionToggle);

    autoMasterStrengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    autoMasterStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 20);
    autoMasterStrengthSlider.setColour(juce::Slider::textBoxTextColourId, text);
    autoMasterStrengthSlider.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    autoMasterStrengthSlider.setColour(juce::Slider::textBoxOutlineColourId, border);
    autoMasterStrengthSlider.setColour(juce::Slider::trackColourId, teal);
    autoMasterStrengthSlider.setTooltip("Auto Master strength.");
    addAndMakeVisible(autoMasterStrengthSlider);

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

    copyButton.setButtonText("Copy TXT");
    copyButton.onClick = [this] { copyReportToClipboard(); };
    addAndMakeVisible(copyButton);

    copyJsonButton.setButtonText("Copy JSON");
    copyJsonButton.onClick = [this] { copyJsonReportToClipboard(); };
    addAndMakeVisible(copyJsonButton);

    genreAttachment = std::make_unique<ComboAttachment>(audioProcessor.parameters, "genre", genreBox);
    instrumentalAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, "instrumental", instrumentalToggle);
    autoMasterAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, "autoMasterEnabled", autoMasterToggle);
    autoMasterAuditionAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, "autoMasterAuditionMatch", autoMasterAuditionToggle);
    autoMasterStrengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, "autoMasterStrength", autoMasterStrengthSlider);
    addAndMakeVisible(summaryComponent);
    addAndMakeVisible(loudnessCard);
    addAndMakeVisible(dynamicsCard);
    addAndMakeVisible(stereoCard);
    addAndMakeVisible(qualityCard);
    addAndMakeVisible(toneShapeComponent);
    addAndMakeVisible(targetsComponent);
    addAndMakeVisible(actionsComponent);
    addAndMakeVisible(scoreComponent);
    addAndMakeVisible(referenceComponent);
    addAndMakeVisible(snapshotComponent);

    setSize(1640, 940);
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
        assessment = fmma::assessMix(audioProcessor.makeAssessmentInput(), fmma::getGenreProfile(genreIndex));
        assessmentCountdown = 1;
        lastGenreIndex = genreIndex;
        lastInstrumentalState = instrumentalState;
    }

    SummaryComponent::Data summaryData;
    summaryData.overallScore = assessment.overallScore;
    summaryData.verdictTitle = assessment.verdictTitle;
    summaryData.scopeLabel = metrics.hostAutoPassActive ? "Host Recording" : assessment.analysisScope;
    summaryData.genreGroup = fmma::getGenreProfile(genreIndex).group;
    summaryData.statusLine = assessment.statusLine;
    summaryData.confidenceLabel = assessment.confidenceLabel;
    summaryData.confidenceScore = assessment.confidenceScore;
    summaryData.confidenceCompactText = assessment.confidenceCompactText;
    summaryData.durationSeconds = metrics.fullPassCompleted ? metrics.fullPassSeconds : metrics.analysisSeconds;
    summaryData.lufsDelta = assessment.lufsDelta;
    
    const auto input = audioProcessor.makeAssessmentInput();
    summaryData.lowEndPercent = input.lowEndPercent;
    summaryData.presencePercent = input.presencePercent;
    
    summaryComponent.update(summaryData);

    MetricsCardComponent::Data loudnessData;
    loudnessData.title = "Loudness";
    loudnessData.metrics = {
        {"Momentary", formatLufs(metrics.momentaryLufs)},
        {"Short-Term", formatLufs(metrics.shortTermLufs)},
        {"Integrated", formatLufs(metrics.integratedLufs)},
        {"LRA", juce::String(metrics.lraLu, 1) + " LU"},
        {"True Peak", formatDbTp(metrics.truePeakDb)}
    };
    loudnessData.barValue = dbToUnit(metrics.truePeakDb);
    loudnessData.barColour = metrics.truePeakDb > -1.0f ? Theme::danger : Theme::teal;
    loudnessCard.update(loudnessData);

    MetricsCardComponent::Data dynamicsData;
    dynamicsData.title = "Dynamics";
    dynamicsData.metrics = {
        {"RMS", formatDb(metrics.rmsDb)},
        {"Sample Peak", formatDb(metrics.peakDb)},
        {"Crest", formatDb(metrics.crestDb)},
        {"Trans / Attack", juce::String(metrics.transientDensity, 1) + "/s / " + juce::String(metrics.attackTimeMs, 0) + " ms"},
        {"Percussion", juce::String(metrics.percussionEnergyPct, 1) + "%"}
    };
    dynamicsData.barValue = dbToUnit(metrics.peakDb);
    dynamicsData.barColour = metrics.peakDb > -1.0f ? Theme::danger : Theme::teal;
    dynamicsCard.update(dynamicsData);

    MetricsCardComponent::Data stereoData;
    stereoData.title = "Stereo";
    stereoData.metrics = {
        {"Correlation", formatNumber(metrics.correlation, 2)},
        {"Width", formatNumber(metrics.widthPct, 1) + "%"},
        {"M/S Ratio", formatDb(metrics.msRatioDb, 2)},
        {"Mono Loss", formatSigned(metrics.monoLossDb, " dB")},
        {"Low Phase", "C " + formatNumber(lowEndCorrelationOf(metrics), 2) + " / S " + formatDb(lowEndSideDbOf(metrics), 1)},
        {"Balance L/R", formatDb(metrics.stereoBalanceDb, 2)}
    };
    stereoData.barValue = (metrics.correlation + 1.0f) * 0.5f;
    stereoData.barColour = (metrics.monoLossDb < -4.0f || metrics.correlation < 0.3f || lowEndCorrelationOf(metrics) < 0.65f) ? Theme::danger
                         : (metrics.monoLossDb < -2.5f || metrics.correlation < 0.55f || lowEndSideDbOf(metrics) > -6.0f) ? Theme::amber
                         : Theme::teal;
    stereoCard.update(stereoData);

    const auto truePeakForSafety = metrics.fullPassCompleted ? juce::jmax(metrics.truePeakDb, metrics.worstTruePeakDb)
                                                             : metrics.truePeakDb;
    const auto hasTruePeak = truePeakForSafety > -119.0f && std::isfinite(truePeakForSafety);
    const auto truePeakMargin = hasTruePeak ? -1.0f - truePeakForSafety : 0.0f;

    MetricsCardComponent::Data qualityData;
    qualityData.title = "Safety / Delivery";
    qualityData.metrics = {
        {"Auto Master", metrics.autoMasterEnabled ? "On " + juce::String(metrics.autoMasterStrength, 0) + "%" : "Off"},
        {"AM Target", metrics.autoMasterEnabled ? juce::String(metrics.autoMasterTargetLufs, 0) + " LUFS / "
                                                  + juce::String(metrics.autoMasterCeilingDbTp, 1) + " dBTP"
                                                : "N/A"},
        {"AM Gain/Match", metrics.autoMasterEnabled ? formatSigned(metrics.autoMasterGainDb, " dB")
                                                       + " / " + formatSigned(metrics.autoMasterLoudnessMatchGainDb, " dB")
                                                     : "N/A"},
        {"AM Tone", metrics.autoMasterEnabled ? "L " + formatSigned(metrics.autoMasterLowShelfDb, " dB", 1)
                                                + " / P " + formatSigned(metrics.autoMasterPresenceDb, " dB", 1)
                                                + " / A " + formatSigned(metrics.autoMasterAirShelfDb, " dB", 1)
                                              : "N/A"},
        {"AM Dyn", metrics.autoMasterEnabled ? "Glue " + juce::String(metrics.autoMasterGlueReductionDb, 1)
                                               + " / Lim " + juce::String(metrics.autoMasterLimiterReductionDb, 1) + " dB"
                                             : "N/A"},
        {"AM Score/LU/TP", metrics.autoMasterEnabled ? juce::String(metrics.autoMasterReleaseScore, 0) + " / "
                                                       + formatSigned(metrics.autoMasterLufsDeltaDb, "", 1)
                                                       + " / " + formatSigned(metrics.autoMasterTruePeakMarginDb, "", 1)
                                                     : "N/A"},
        {"AM A/B", metrics.autoMasterEnabled ? juce::String(metrics.autoMasterAbScore, 0) + " / LU "
                                                + formatSigned(metrics.autoMasterAbLoudnessDeltaDb, "", 1)
                                                + " / TP " + formatSigned(metrics.autoMasterAbTruePeakDeltaDb, "", 1)
                                              : "N/A"},
        {"AM Listen", metrics.autoMasterEnabled ? (metrics.autoMasterAuditionMatch ? "Matched "
                                                    + formatSigned(metrics.autoMasterAuditionGainDb, " dB")
                                                  : "Print")
                                                : "N/A"},
        {"Clip/Worst", juce::String(metrics.clippedPercent, 3) + " / " + juce::String(metrics.worstClippedPercent, 3) + "%"},
        {"TP Margin", hasTruePeak ? formatSigned(truePeakMargin, " dB") : "N/A"},
        {"Worst TP", formatDbTp(metrics.worstTruePeakDb)},
        {"Stream -14", formatDeliveryPreview(-14.0f)}
    };
    qualityData.rowHeight = 11.8f;
    qualityData.barValue = hasTruePeak ? juce::jlimit(0.0f, 1.0f, (truePeakMargin + 1.0f) / 5.0f) : 0.0f;
    qualityData.barColour = hasTruePeak ? (truePeakMargin < 0.0f ? Theme::danger : truePeakMargin < 1.0f ? Theme::amber : Theme::success) : Theme::muted;
    qualityCard.update(qualityData);

    ToneShapeComponent::Data toneShapeData;
    for (size_t i = 0; i < 6; ++i)
        toneShapeData.bandPercents[i] = metrics.bandPercents[i];
    toneShapeData.spectralCentroidHz = metrics.spectralCentroidHz;
    toneShapeData.spectralRolloffHz = metrics.spectralRolloffHz;
    toneShapeData.resonanceFreqHz = metrics.resonanceFreqHz;
    toneShapeData.resonanceGainDb = metrics.resonanceGainDb;
    toneShapeData.spectralCentroidString = formatHz(metrics.spectralCentroidHz);
    toneShapeData.spectralRolloffString = formatHz(metrics.spectralRolloffHz);
    toneShapeData.resonanceString = (metrics.resonanceFreqHz > 0.0f && metrics.resonanceGainDb >= 6.0f)
                                  ? formatHz(metrics.resonanceFreqHz) + " / +" + juce::String(metrics.resonanceGainDb, 1)
                                  : "N/A";
    toneShapeComponent.update(toneShapeData);

    ActionChecklistComponent::Data targetsData;
    targetsData.title = "Targets";
    targetsData.fontHeight = 13.0f;
    targetsData.items = {
        { "Release Gate: " + assessment.releaseGateTitle + " (" + juce::String(assessment.releaseGateScore) + "/100)",
          Theme::okColour(assessment.releaseReady), 10.0f },
        { assessment.lufsTargetText, Theme::okColour(assessment.lufsOk), 10.0f },
        { assessment.lowEndTargetText, Theme::okColour(assessment.lowEndOk), 10.0f },
        { "Low phase: corr >= 0.65, side <= -6 dB", Theme::okColour(assessment.lowEndPhaseOk), 10.0f },
        { assessment.crestTargetText, Theme::okColour(assessment.crestOk), 10.0f },
        { assessment.lraTargetText, Theme::okColour(assessment.lraOk), 10.0f },
        { assessment.correlationTargetText, Theme::okColour(assessment.correlationOk), 10.0f },
        { "True Peak <= -1.0 dBTP", Theme::okColour(assessment.truePeakOk && assessment.clippingOk), 10.0f }
    };
    targetsComponent.update(targetsData);

    ActionChecklistComponent::Data actionsData;
    actionsData.title = "Priority Actions";
    actionsData.fontHeight = 14.0f;
    actionsData.emptyMessage = "Mix meets all target criteria!";
    actionsData.emptyMessageColour = Theme::success;
    for (int i = 0; i < assessment.priorityActionCount; ++i)
    {
        actionsData.items.push_back({ assessment.priorityActions[static_cast<size_t>(i)], Theme::danger, 8.0f });
    }
    actionsComponent.update(actionsData);

    // Score Component
    ScoreComponent::Data scoreData;
    scoreData.lufsScore       = assessment.lufsScore;
    scoreData.correlationScore= assessment.correlationScore;
    scoreData.lowEndScore     = assessment.lowEndScore;
    scoreData.crestScore      = assessment.crestScore;
    scoreData.lraScore        = assessment.lraScore;
    scoreData.clippingScore   = assessment.clippingScore;
    scoreData.toneScore       = assessment.toneScore;
    scoreData.headphoneScore  = assessment.headphoneScore;
    scoreData.speakerScore    = assessment.speakerScore;
    scoreData.transientScore  = assessment.transientScore;
    scoreComponent.update(scoreData);

    // Reference Component
    {
        CompareComponent::Data refData;
        refData.title = "Reference";
        if (! hasReferenceMetrics)
        {
            refData.emptyMessage = "No reference captured.";
            refData.metrics = {
                { "State",   referenceCaptureInProgress ? "Recording" : "Empty" },
                { "Compare", "Unavailable" }
            };
        }
        else
        {
            refData.rowHeight = 20.0f;
            refData.metrics = {
                { "Ref Time",   formatDuration(referenceMetrics.fullPassSeconds > 0.0f ? referenceMetrics.fullPassSeconds : referenceMetrics.analysisSeconds) },
                { "LUFS Delta",      formatSigned(metrics.integratedLufs - referenceMetrics.integratedLufs, " LU") },
                { "Low-End Delta",   formatSigned(lowEndOf(metrics) - lowEndOf(referenceMetrics), "%") },
                { "Presence Delta",  formatSigned(presenceOf(metrics) - presenceOf(referenceMetrics), "%") },
                { "Crest Delta",     formatSigned(metrics.crestDb - referenceMetrics.crestDb, " dB") },
                { "Width Delta",     formatSigned(metrics.widthPct - referenceMetrics.widthPct, "%") }
            };
            refData.noteTitle = "Ref Note";
            refData.noteText  = referenceNote();
        }
        referenceComponent.update(refData);
    }

    // Snapshot Component
    {
        CompareComponent::Data snapData;
        snapData.title = "A/B Snapshots";
        if (! hasSnapshotA || ! hasSnapshotB)
        {
            snapData.rowHeight = 22.0f;
            snapData.metrics = {
                { "Snapshot A", hasSnapshotA ? "Stored" : "Empty" },
                { "Snapshot B", hasSnapshotB ? "Stored" : "Empty" },
                { "Compare",   hasSnapshotA || hasSnapshotB ? "Waiting" : "Unavailable" }
            };
            snapData.noteTitle = "A/B Note";
            snapData.noteText  = snapshotNote();
        }
        else
        {
            snapData.rowHeight = 20.0f;
            snapData.metrics = {
                { "A Time",   formatDuration(snapshotA.fullPassSeconds > 0.0f ? snapshotA.fullPassSeconds : snapshotA.analysisSeconds) },
                { "B Time",   formatDuration(snapshotB.fullPassSeconds > 0.0f ? snapshotB.fullPassSeconds : snapshotB.analysisSeconds) },
                { "LUFS B-A", formatSigned(snapshotB.integratedLufs - snapshotA.integratedLufs, " LU") },
                { "TP B-A",   formatSigned(snapshotB.truePeakDb - snapshotA.truePeakDb, " dB") },
                { "Low B-A",  formatSigned(lowEndOf(snapshotB) - lowEndOf(snapshotA), "%") },
                { "Crest B-A",formatSigned(snapshotB.crestDb - snapshotA.crestDb, " dB") },
                { "Width B-A",formatSigned(snapshotB.widthPct - snapshotA.widthPct, "%") }
            };
            snapData.noteTitle = "A/B Note";
            snapData.noteText  = snapshotNote();
        }
        snapshotComponent.update(snapData);
    }

    repaint();
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::smoothDisplayMetrics(const fmma::AnalyzerMetrics& raw)
{
    if (! hasDisplayMetrics)
    {
        metrics = raw;
        hasDisplayMetrics = true;
        return;
    }

    const float amount = 0.15f;
    metrics.momentaryLufs = smoothValue(metrics.momentaryLufs, raw.momentaryLufs, amount);
    metrics.shortTermLufs = smoothValue(metrics.shortTermLufs, raw.shortTermLufs, amount);
    metrics.integratedLufs = smoothValue(metrics.integratedLufs, raw.integratedLufs, amount);
    metrics.truePeakDb = smoothValue(metrics.truePeakDb, raw.truePeakDb, amount);
    metrics.truePeakHoldDb = smoothValue(metrics.truePeakHoldDb, raw.truePeakHoldDb, 0.05f);
    metrics.worstTruePeakDb = raw.worstTruePeakDb;
    metrics.lraLu = smoothValue(metrics.lraLu, raw.lraLu, 0.05f);
    metrics.rmsDb = smoothValue(metrics.rmsDb, raw.rmsDb, amount);
    metrics.peakDb = smoothValue(metrics.peakDb, raw.peakDb, amount);
    metrics.crestDb = smoothValue(metrics.crestDb, raw.crestDb, amount);
    metrics.leftPeakDb = smoothValue(metrics.leftPeakDb, raw.leftPeakDb, amount);
    metrics.rightPeakDb = smoothValue(metrics.rightPeakDb, raw.rightPeakDb, amount);
    metrics.monoPeakDb = smoothValue(metrics.monoPeakDb, raw.monoPeakDb, amount);
    metrics.monoRmsDb = smoothValue(metrics.monoRmsDb, raw.monoRmsDb, amount);
    metrics.monoLossDb = smoothValue(metrics.monoLossDb, raw.monoLossDb, amount);
    metrics.correlation = smoothValue(metrics.correlation, raw.correlation, amount);
    metrics.worstCorrelation = raw.worstCorrelation;
    metrics.worstMonoLossDb = raw.worstMonoLossDb;
    metrics.widthPct = smoothValue(metrics.widthPct, raw.widthPct, amount);
    metrics.msRatioDb = smoothValue(metrics.msRatioDb, raw.msRatioDb, amount);
    metrics.stereoBalanceDb = smoothValue(metrics.stereoBalanceDb, raw.stereoBalanceDb, amount);
    metrics.dcOffset = smoothValue(metrics.dcOffset, raw.dcOffset, amount);
    metrics.clippedPercent = smoothValue(metrics.clippedPercent, raw.clippedPercent, amount);
    metrics.worstClippedPercent = raw.worstClippedPercent;
    metrics.silencePercent = smoothValue(metrics.silencePercent, raw.silencePercent, amount);
    metrics.transientDensity = smoothValue(metrics.transientDensity, raw.transientDensity, amount);
    metrics.attackTimeMs = smoothValue(metrics.attackTimeMs, raw.attackTimeMs, amount);
    metrics.percussionEnergyPct = smoothValue(metrics.percussionEnergyPct, raw.percussionEnergyPct, amount);
    metrics.spectralCentroidHz = smoothValue(metrics.spectralCentroidHz, raw.spectralCentroidHz, amount);
    metrics.spectralRolloffHz = smoothValue(metrics.spectralRolloffHz, raw.spectralRolloffHz, amount);
    metrics.resonanceFreqHz = smoothValue(metrics.resonanceFreqHz, raw.resonanceFreqHz, amount);
    metrics.resonanceGainDb = smoothValue(metrics.resonanceGainDb, raw.resonanceGainDb, amount);
    metrics.worstResonanceFreqHz = raw.worstResonanceFreqHz;
    metrics.worstResonanceGainDb = raw.worstResonanceGainDb;
    metrics.analysisSeconds = raw.analysisSeconds;
    metrics.fullPassSeconds = raw.fullPassSeconds;
    metrics.fullPassActive = raw.fullPassActive;
    metrics.fullPassCompleted = raw.fullPassCompleted;
    metrics.analysisFrozen = raw.analysisFrozen;
    metrics.hostTransportPlaying = raw.hostTransportPlaying;
    metrics.hostAutoPassActive = raw.hostAutoPassActive;
    metrics.autoMasterEnabled = raw.autoMasterEnabled;
    metrics.autoMasterAuditionMatch = raw.autoMasterAuditionMatch;
    metrics.autoMasterStrength = raw.autoMasterStrength;
    metrics.autoMasterTargetLufs = raw.autoMasterTargetLufs;
    metrics.autoMasterCeilingDbTp = raw.autoMasterCeilingDbTp;
    metrics.autoMasterGainDb = smoothValue(metrics.autoMasterGainDb, raw.autoMasterGainDb, amount);
    metrics.autoMasterLowShelfDb = smoothValue(metrics.autoMasterLowShelfDb, raw.autoMasterLowShelfDb, amount);
    metrics.autoMasterPresenceDb = smoothValue(metrics.autoMasterPresenceDb, raw.autoMasterPresenceDb, amount);
    metrics.autoMasterAirShelfDb = smoothValue(metrics.autoMasterAirShelfDb, raw.autoMasterAirShelfDb, amount);
    metrics.autoMasterWidthPercent = smoothValue(metrics.autoMasterWidthPercent, raw.autoMasterWidthPercent, amount);
    metrics.autoMasterGlueReductionDb = smoothValue(metrics.autoMasterGlueReductionDb, raw.autoMasterGlueReductionDb, amount);
    metrics.autoMasterLimiterReductionDb = smoothValue(metrics.autoMasterLimiterReductionDb, raw.autoMasterLimiterReductionDb, amount);
    const auto smoothAudioDbValue = [amount] (float current, float next)
    {
        return (current <= -119.0f || next <= -119.0f) ? next : smoothValue(current, next, amount);
    };
    metrics.autoMasterProjectedLufs = smoothAudioDbValue(metrics.autoMasterProjectedLufs, raw.autoMasterProjectedLufs);
    metrics.autoMasterProjectedTruePeakDbTp = smoothAudioDbValue(metrics.autoMasterProjectedTruePeakDbTp, raw.autoMasterProjectedTruePeakDbTp);
    metrics.autoMasterLoudnessMatchGainDb = smoothValue(metrics.autoMasterLoudnessMatchGainDb, raw.autoMasterLoudnessMatchGainDb, amount);
    metrics.autoMasterLufsDeltaDb = smoothValue(metrics.autoMasterLufsDeltaDb, raw.autoMasterLufsDeltaDb, amount);
    metrics.autoMasterTruePeakMarginDb = smoothValue(metrics.autoMasterTruePeakMarginDb, raw.autoMasterTruePeakMarginDb, amount);
    metrics.autoMasterReleaseScore = smoothValue(metrics.autoMasterReleaseScore, raw.autoMasterReleaseScore, amount);
    metrics.autoMasterAbLoudnessDeltaDb = smoothValue(metrics.autoMasterAbLoudnessDeltaDb, raw.autoMasterAbLoudnessDeltaDb, amount);
    metrics.autoMasterAbTruePeakDbTp = smoothAudioDbValue(metrics.autoMasterAbTruePeakDbTp, raw.autoMasterAbTruePeakDbTp);
    metrics.autoMasterAbTruePeakDeltaDb = smoothValue(metrics.autoMasterAbTruePeakDeltaDb, raw.autoMasterAbTruePeakDeltaDb, amount);
    metrics.autoMasterAbDynamicsDeltaDb = smoothValue(metrics.autoMasterAbDynamicsDeltaDb, raw.autoMasterAbDynamicsDeltaDb, amount);
    metrics.autoMasterAbScore = smoothValue(metrics.autoMasterAbScore, raw.autoMasterAbScore, amount);
    metrics.autoMasterAuditionGainDb = smoothValue(metrics.autoMasterAuditionGainDb, raw.autoMasterAuditionGainDb, amount);
    metrics.autoMasterAuditionLoudnessDeltaDb = smoothValue(metrics.autoMasterAuditionLoudnessDeltaDb, raw.autoMasterAuditionLoudnessDeltaDb, amount);
    metrics.autoMasterAuditionTruePeakDbTp = smoothAudioDbValue(metrics.autoMasterAuditionTruePeakDbTp, raw.autoMasterAuditionTruePeakDbTp);

    for (size_t i = 0; i < fmma::bandCount; ++i)
    {
        metrics.bandPercents[i] = smoothValue(metrics.bandPercents[i], raw.bandPercents[i], amount);
        metrics.bandCorrelations[i] = smoothValue(metrics.bandCorrelations[i], raw.bandCorrelations[i], amount);
        metrics.bandSideRatiosDb[i] = smoothValue(metrics.bandSideRatiosDb[i], raw.bandSideRatiosDb[i], amount);
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
    return formatDeliveryPreview(targetLufs, metrics);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::formatDeliveryPreview(
    float targetLufs,
    const fmma::AnalyzerMetrics& sourceMetrics) const
{
    const auto truePeakForDelivery = sourceMetrics.fullPassCompleted
        ? juce::jmax(sourceMetrics.truePeakDb, sourceMetrics.worstTruePeakDb)
        : sourceMetrics.truePeakDb;
    if (sourceMetrics.integratedLufs <= -119.0f || truePeakForDelivery <= -119.0f
        || ! std::isfinite(sourceMetrics.integratedLufs) || ! std::isfinite(truePeakForDelivery))
    {
        return "N/A";
    }

    const auto gainDelta = targetLufs - sourceMetrics.integratedLufs;
    const auto normalisedTruePeak = truePeakForDelivery + gainDelta;
    return formatSigned(gainDelta, " dB") + " / " + juce::String(normalisedTruePeak, 1) + " dBTP";
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::referenceNote() const
{
    return referenceNote(metrics, assessment);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::referenceNote(
    const fmma::AnalyzerMetrics& sourceMetrics,
    const fmma::MixAssessment& sourceAssessment) const
{
    if (! hasReferenceMetrics)
        return "No reference captured.";

    if (! sourceAssessment.measurementReady)
        return "Play the current mix to compare against the captured reference.";

    const auto lufsDelta = sourceMetrics.integratedLufs - referenceMetrics.integratedLufs;
    const auto lowDelta = lowEndOf(sourceMetrics) - lowEndOf(referenceMetrics);
    const auto presenceDelta = presenceOf(sourceMetrics) - presenceOf(referenceMetrics);
    const auto crestDelta = sourceMetrics.crestDb - referenceMetrics.crestDb;
    const auto widthDelta = sourceMetrics.widthPct - referenceMetrics.widthPct;

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
    return referenceActionNote(metrics, assessment);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::referenceActionNote(
    const fmma::AnalyzerMetrics& sourceMetrics,
    const fmma::MixAssessment& sourceAssessment) const
{
    if (! hasReferenceMetrics || ! sourceAssessment.measurementReady)
        return {};

    const auto note = referenceNote(sourceMetrics, sourceAssessment);
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
    return mixDoctorSummary(metrics, assessment);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::mixDoctorSummary(
    const fmma::AnalyzerMetrics& sourceMetrics,
    const fmma::MixAssessment& sourceAssessment) const
{
    if (! sourceAssessment.measurementReady)
        return "Measurement is still building; run a pass before final judgement.";

    juce::String summary = sourceAssessment.verdictTitle + ": ";
    summary << (sourceAssessment.priorityActionCount > 0
                    ? sourceAssessment.priorityActions[0]
                    : juce::String("Core metrics are in range for the selected profile."));

    const auto refAction = referenceActionNote(sourceMetrics, sourceAssessment);
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
    bounds.removeFromTop(124.0f); // Reserve space for summaryComponent

    bounds.removeFromTop(18.0f);
    bounds.removeFromTop(190.0f); // Space for metrics cards

    bounds.removeFromTop(18.0f);
    bounds.removeFromTop(190.0f);

    bounds.removeFromTop(18.0f);
    bounds.removeFromTop(190.0f);
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    auto header = bounds.removeFromTop(34);
    auto controls = header.removeFromRight(1370);
    genreLabel.setBounds(controls.removeFromLeft(80));
    genreBox.setBounds(controls.removeFromLeft(170).reduced(0, 1));
    controls.removeFromLeft(7);
    instrumentalToggle.setBounds(controls.removeFromLeft(96));
    controls.removeFromLeft(7);
    autoMasterToggle.setBounds(controls.removeFromLeft(102));
    controls.removeFromLeft(7);
    autoMasterStrengthSlider.setBounds(controls.removeFromLeft(112).reduced(0, 1));
    controls.removeFromLeft(7);
    autoMasterAuditionToggle.setBounds(controls.removeFromLeft(86));
    controls.removeFromLeft(7);
    passButton.setBounds(controls.removeFromLeft(88).reduced(0, 1));
    controls.removeFromLeft(7);
    referenceButton.setBounds(controls.removeFromLeft(92).reduced(0, 1));
    controls.removeFromLeft(7);
    clearReferenceButton.setBounds(controls.removeFromLeft(62).reduced(0, 1));
    controls.removeFromLeft(7);
    snapshotAButton.setBounds(controls.removeFromLeft(56).reduced(0, 1));
    controls.removeFromLeft(7);
    snapshotBButton.setBounds(controls.removeFromLeft(56).reduced(0, 1));
    controls.removeFromLeft(7);
    clearSnapshotsButton.setBounds(controls.removeFromLeft(66).reduced(0, 1));
    controls.removeFromLeft(7);
    resetButton.setBounds(controls.removeFromLeft(54).reduced(0, 1));
    controls.removeFromLeft(7);
    copyButton.setBounds(controls.removeFromLeft(72).reduced(0, 1));
    controls.removeFromLeft(7);
    copyJsonButton.setBounds(controls.removeFromLeft(82).reduced(0, 1));

    bounds.removeFromTop(14); // Space between header and summary
    summaryComponent.setBounds(bounds.removeFromTop(124));

    bounds.removeFromTop(18);
    auto rowOne = bounds.removeFromTop(190);
    const auto third = (rowOne.getWidth() - 48) / 3;
    loudnessCard.setBounds(rowOne.removeFromLeft(third));
    rowOne.removeFromLeft(24);
    dynamicsCard.setBounds(rowOne.removeFromLeft(third));
    rowOne.removeFromLeft(24);
    stereoCard.setBounds(rowOne);

    auto rowTwo = bounds.removeFromTop(190);
    const auto thirdRowTwo = (rowTwo.getWidth() - 48) / 3;
    targetsComponent.setBounds(rowTwo.removeFromLeft(thirdRowTwo));
    rowTwo.removeFromLeft(24);
    qualityCard.setBounds(rowTwo.removeFromLeft(thirdRowTwo));
    rowTwo.removeFromLeft(24);
    toneShapeComponent.setBounds(rowTwo);

    bounds.removeFromTop(18);
    auto bottom = bounds;
    const auto bottomGap = 18;
    const auto availableBottomWidth = bottom.getWidth() - (bottomGap * 3);
    const auto scoreWidth = static_cast<int>(availableBottomWidth * 0.29f);
    const auto referenceWidth = static_cast<int>(availableBottomWidth * 0.23f);
    const auto snapshotWidth = static_cast<int>(availableBottomWidth * 0.23f);
    scoreComponent.setBounds(bottom.removeFromLeft(scoreWidth));
    bottom.removeFromLeft(bottomGap);
    referenceComponent.setBounds(bottom.removeFromLeft(referenceWidth));
    bottom.removeFromLeft(bottomGap);
    snapshotComponent.setBounds(bottom.removeFromLeft(snapshotWidth));
    bottom.removeFromLeft(bottomGap);
    actionsComponent.setBounds(bottom);
}


void FunkyMooseMixAnalyzerAudioProcessorEditor::copyReportToClipboard()
{
    juce::SystemClipboard::copyTextToClipboard(buildTextReport());
}

void FunkyMooseMixAnalyzerAudioProcessorEditor::copyJsonReportToClipboard()
{
    juce::SystemClipboard::copyTextToClipboard(buildJsonReport());
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::buildTextReport() const
{
    const auto& profile = fmma::getGenreProfile(genreBox.getSelectedItemIndex());
    const auto input = audioProcessor.makeAssessmentInput();
    const auto sourceAssessment = fmma::assessMix(input, profile);

    return buildTextReport(audioProcessor.getMetrics(), input, sourceAssessment);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::buildTextReport(
    const fmma::AnalyzerMetrics& sourceMetrics,
    const fmma::MixAssessmentInput& input,
    const fmma::MixAssessment& sourceAssessment) const
{
    const auto& profile = fmma::getGenreProfile(genreBox.getSelectedItemIndex());
    const auto truePeakForReport = sourceMetrics.fullPassCompleted
        ? juce::jmax(sourceMetrics.truePeakDb, sourceMetrics.worstTruePeakDb)
        : sourceMetrics.truePeakDb;
    juce::String report;
    report << "FUNKY MOOSE MIX ANALYZER - LIVE PLUGIN REPORT\n";
    report << "Genre Profile: " << profile.name << "\n";
    report << "Mode: " << (input.instrumental ? "Instrumental" : "Vocal / Full Mix") << "\n";
    report << "Analysis Scope: " << (sourceMetrics.hostAutoPassActive ? "Host Recording" : sourceAssessment.analysisScope) << "\n";
    report << "Measurement Standard: ITU-R BS.1770-5 / EBU Mode\n";
    report << "Analysis Time: " << formatDuration(sourceMetrics.fullPassCompleted ? sourceMetrics.fullPassSeconds : sourceMetrics.analysisSeconds) << "\n";
    report << "Confidence: " << sourceAssessment.confidenceLabel << " (" << sourceAssessment.confidenceScore << "/100)\n";
    report << "Confidence Domains: " << sourceAssessment.confidenceBreakdownText << "\n";
    report << "Confidence Note: " << sourceAssessment.confidenceText << "\n";
    report << "Release Gate: " << sourceAssessment.releaseGateTitle << " ("
           << sourceAssessment.releaseGateScore << "/100, "
           << (sourceAssessment.releaseReady ? "ready" : "blocked") << ")\n";
    report << "Release Gate Note: " << sourceAssessment.releaseGateText << "\n";
    if (sourceAssessment.releaseBlockerCount > 0)
    {
        report << "Release Blockers:\n";
        for (auto i = 0; i < sourceAssessment.releaseBlockerCount; ++i)
            report << "- " << sourceAssessment.releaseBlockers[static_cast<size_t>(i)] << "\n";
    }
    report << "Auto Master: " << (sourceMetrics.autoMasterEnabled ? "enabled" : "off") << "\n";
    if (sourceMetrics.autoMasterEnabled)
    {
        report << "Auto Master Target: " << juce::String(sourceMetrics.autoMasterTargetLufs, 1)
               << " LUFS / " << juce::String(sourceMetrics.autoMasterCeilingDbTp, 1) << " dBTP ceiling\n";
        report << "Auto Master Strength: " << juce::String(sourceMetrics.autoMasterStrength, 0) << "%\n";
        report << "Auto Master Moves: gain " << formatSigned(sourceMetrics.autoMasterGainDb, " dB")
               << ", low shelf " << formatSigned(sourceMetrics.autoMasterLowShelfDb, " dB")
               << ", presence " << formatSigned(sourceMetrics.autoMasterPresenceDb, " dB")
               << ", air " << formatSigned(sourceMetrics.autoMasterAirShelfDb, " dB")
               << ", width " << juce::String(sourceMetrics.autoMasterWidthPercent, 0) << "%\n";
        report << "Auto Master Glue GR: " << juce::String(sourceMetrics.autoMasterGlueReductionDb, 1) << " dB\n";
        report << "Auto Master Limiter GR: " << juce::String(sourceMetrics.autoMasterLimiterReductionDb, 1) << " dB\n";
        report << "Auto Master Projected LUFS: "
               << (sourceMetrics.autoMasterProjectedLufs > -119.0f ? juce::String(sourceMetrics.autoMasterProjectedLufs, 1) : "N/A") << "\n";
        report << "Auto Master Projected True Peak: "
               << (sourceMetrics.autoMasterProjectedTruePeakDbTp > -119.0f
                       ? juce::String(sourceMetrics.autoMasterProjectedTruePeakDbTp, 1) + " dBTP"
                       : "N/A")
               << "\n";
        report << "Auto Master Audition: " << (sourceMetrics.autoMasterAuditionMatch ? "loudness-matched" : "print level") << "\n";
        if (sourceMetrics.autoMasterAuditionMatch)
        {
            report << "Auto Master Audition Gain: " << formatSigned(sourceMetrics.autoMasterAuditionGainDb, " dB") << "\n";
            report << "Auto Master Audition Loudness Delta: " << formatSigned(sourceMetrics.autoMasterAuditionLoudnessDeltaDb, " dB") << "\n";
            report << "Auto Master Audition True Peak: "
                   << (sourceMetrics.autoMasterAuditionTruePeakDbTp > -119.0f
                           ? juce::String(sourceMetrics.autoMasterAuditionTruePeakDbTp, 1) + " dBTP"
                           : "N/A")
                   << "\n";
        }
        report << "Auto Master Loudness-Match Gain: " << formatSigned(sourceMetrics.autoMasterLoudnessMatchGainDb, " dB") << "\n";
        report << "Auto Master Target Delta: " << formatSigned(sourceMetrics.autoMasterLufsDeltaDb, " dB") << "\n";
        report << "Auto Master True Peak Margin: " << formatSigned(sourceMetrics.autoMasterTruePeakMarginDb, " dB") << "\n";
        report << "Auto Master Release Score: " << juce::String(sourceMetrics.autoMasterReleaseScore, 0) << "/100\n";
        report << "Auto Master A/B Score: " << juce::String(sourceMetrics.autoMasterAbScore, 0) << "/100\n";
        report << "Auto Master A/B Loudness Delta: " << formatSigned(sourceMetrics.autoMasterAbLoudnessDeltaDb, " dB") << "\n";
        report << "Auto Master A/B True Peak: "
               << (sourceMetrics.autoMasterAbTruePeakDbTp > -119.0f
                       ? juce::String(sourceMetrics.autoMasterAbTruePeakDbTp, 1) + " dBTP ("
                         + formatSigned(sourceMetrics.autoMasterAbTruePeakDeltaDb, " dB") + ")"
                       : "N/A")
               << "\n";
        report << "Auto Master A/B Dynamics Delta: " << formatSigned(sourceMetrics.autoMasterAbDynamicsDeltaDb, " dB") << "\n";
    }
    report << "Verdict: " << sourceAssessment.verdictTitle << "\n";
    report << "Mix Score: " << sourceAssessment.overallScore << "/100\n";
    report << "Mix Doctor Summary: " << mixDoctorSummary(sourceMetrics, sourceAssessment) << "\n";
    report << "Tone Score: " << sourceAssessment.toneScore << "/100\n";
    report << "LRA Score: " << sourceAssessment.lraScore << "/100\n";
    report << "Transient Score: " << sourceAssessment.transientScore << "/100\n";
    report << "Headphone Score: " << sourceAssessment.headphoneScore << "/100\n";
    report << "Speaker Score: " << sourceAssessment.speakerScore << "/100\n";
    report << "------------------------------------------\n";
    report << "Integrated LUFS: " << juce::String(sourceMetrics.integratedLufs, 1)
           << " (target " << juce::String(profile.targetLufs, 0) << ")\n";
    report << "True Peak: " << juce::String(sourceMetrics.truePeakDb, 1) << " dBTP\n";
    report << "Worst True Peak: " << juce::String(sourceMetrics.worstTruePeakDb, 1) << " dBTP\n";
    report << "Crest: " << juce::String(sourceMetrics.crestDb, 1) << " dB\n";
    report << "Correlation: " << juce::String(sourceMetrics.correlation, 2) << "\n";
    report << "Worst Correlation: " << juce::String(sourceMetrics.worstCorrelation, 2) << "\n";
    report << "Width: " << juce::String(sourceMetrics.widthPct, 1) << "%\n";
    report << "Mono RMS: " << formatDb(sourceMetrics.monoRmsDb) << "\n";
    report << "Mono Loss: " << formatSigned(sourceMetrics.monoLossDb, " dB") << "\n";
    report << "Worst Mono Loss: " << formatSigned(sourceMetrics.worstMonoLossDb, " dB") << "\n";
    report << "Low-End Phase: corr " << juce::String(input.lowEndCorrelation, 2)
           << ", side " << formatDb(input.lowEndSideDb) << "\n";
    report << "Low-End: " << juce::String(input.lowEndPercent, 1) << "%\n";
    report << "Presence: " << juce::String(input.presencePercent, 1) << "%\n";
    report << "LRA: " << juce::String(sourceMetrics.lraLu, 1) << " LU\n";
    report << "Transient Density: " << juce::String(sourceMetrics.transientDensity, 1) << "/s\n";
    report << "Attack: " << juce::String(sourceMetrics.attackTimeMs, 0) << " ms\n";
    report << "Percussive Energy: " << juce::String(sourceMetrics.percussionEnergyPct, 1) << "%\n";
    report << "Spectral Centroid: " << formatHz(sourceMetrics.spectralCentroidHz) << "\n";
    report << "Spectral Rolloff: " << formatHz(sourceMetrics.spectralRolloffHz) << "\n";
    report << "Dominant Resonance Area: ";
    if (sourceMetrics.resonanceFreqHz > 0.0f && sourceMetrics.resonanceGainDb >= 6.0f)
        report << formatHz(sourceMetrics.resonanceFreqHz) << " (+" << juce::String(sourceMetrics.resonanceGainDb, 1) << " dB)\n";
    else
        report << "None detected\n";
    report << "Worst Resonance Area: ";
    if (sourceMetrics.worstResonanceFreqHz > 0.0f && sourceMetrics.worstResonanceGainDb >= 6.0f)
        report << formatHz(sourceMetrics.worstResonanceFreqHz) << " (+" << juce::String(sourceMetrics.worstResonanceGainDb, 1) << " dB)\n";
    else
        report << "None detected\n";
    report << "Clipping: " << juce::String(sourceMetrics.clippedPercent, 3) << "%\n";
    report << "Worst Clipping: " << juce::String(sourceMetrics.worstClippedPercent, 3) << "%\n";
    report << "Worst Low-Mids: " << juce::String(sourceMetrics.worstLowMidPercent, 1) << "%\n";
    report << "Band Balance: ";
    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        if (i > 0)
            report << ", ";
        report << fmma::bandNames[static_cast<size_t>(i)] << " "
               << juce::String(sourceMetrics.bandPercents[static_cast<size_t>(i)], 1) << "%";
    }
    report << "\n";
    report << "Band Phase: ";
    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        if (i > 0)
            report << ", ";
        report << fmma::bandNames[static_cast<size_t>(i)] << " corr "
               << juce::String(sourceMetrics.bandCorrelations[static_cast<size_t>(i)], 2)
               << " / side " << juce::String(sourceMetrics.bandSideRatiosDb[static_cast<size_t>(i)], 1) << " dB";
    }
    report << "\n";

    const auto hasTruePeak = truePeakForReport > -119.0f && std::isfinite(truePeakForReport);
    report << "\nDELIVERY PREVIEW\n";
    report << "True Peak Margin to -1.0 dBTP: "
           << (hasTruePeak ? formatSigned(-1.0f - truePeakForReport, " dB") : juce::String("N/A")) << "\n";
    report << "Streaming -14 LUFS: gain / estimated TP " << formatDeliveryPreview(-14.0f, sourceMetrics) << "\n";
    report << "Apple -16 LUFS: gain / estimated TP " << formatDeliveryPreview(-16.0f, sourceMetrics) << "\n";
    report << "Broadcast -23 LUFS: gain / estimated TP " << formatDeliveryPreview(-23.0f, sourceMetrics) << "\n";

    if (hasReferenceMetrics)
    {
        report << "\nREFERENCE COMPARE\n";
        report << "Reference Time: "
               << formatDuration(referenceMetrics.fullPassSeconds > 0.0f ? referenceMetrics.fullPassSeconds
                                                                          : referenceMetrics.analysisSeconds)
               << "\n";
        report << "Reference LUFS: " << juce::String(referenceMetrics.integratedLufs, 1) << "\n";
        report << "LUFS Delta: " << formatSigned(sourceMetrics.integratedLufs - referenceMetrics.integratedLufs, " LU") << "\n";
        report << "Low-End Delta: " << formatSigned(lowEndOf(sourceMetrics) - lowEndOf(referenceMetrics), "%") << "\n";
        report << "Presence Delta: " << formatSigned(presenceOf(sourceMetrics) - presenceOf(referenceMetrics), "%") << "\n";
        report << "Crest Delta: " << formatSigned(sourceMetrics.crestDb - referenceMetrics.crestDb, " dB") << "\n";
        report << "Width Delta: " << formatSigned(sourceMetrics.widthPct - referenceMetrics.widthPct, "%") << "\n";
        report << "Reference Note: " << referenceNote(sourceMetrics, sourceAssessment) << "\n";
        if (referenceActionNote(sourceMetrics, sourceAssessment).isNotEmpty())
            report << "Reference Action: " << referenceActionNote(sourceMetrics, sourceAssessment) << "\n";
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

    for (auto i = 0; i < sourceAssessment.priorityActionCount; ++i)
        report << juce::String(i + 1) << ". " << sourceAssessment.priorityActions[static_cast<size_t>(i)] << "\n";
    if (referenceActionNote(sourceMetrics, sourceAssessment).isNotEmpty())
        report << "R. " << referenceActionNote(sourceMetrics, sourceAssessment) << "\n";

    return report;
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::buildJsonReport() const
{
    const auto& profile = fmma::getGenreProfile(genreBox.getSelectedItemIndex());
    const auto input = audioProcessor.makeAssessmentInput();
    const auto sourceAssessment = fmma::assessMix(input, profile);

    return buildJsonReport(audioProcessor.getMetrics(), input, sourceAssessment);
}

juce::String FunkyMooseMixAnalyzerAudioProcessorEditor::buildJsonReport(
    const fmma::AnalyzerMetrics& sourceMetrics,
    const fmma::MixAssessmentInput& input,
    const fmma::MixAssessment& sourceAssessment) const
{
    const auto& profile = fmma::getGenreProfile(genreBox.getSelectedItemIndex());
    const auto truePeakForReport = sourceMetrics.fullPassCompleted
        ? juce::jmax(sourceMetrics.truePeakDb, sourceMetrics.worstTruePeakDb)
        : sourceMetrics.truePeakDb;

    auto makeRange = [] (const fmma::Range& range)
    {
        juce::var object { new juce::DynamicObject() };
        setJsonProperty(object, "low", jsonNumber(range.low));
        setJsonProperty(object, "high", jsonNumber(range.high));
        return object;
    };

    auto makeMetricSummary = [] (const fmma::AnalyzerMetrics& source)
    {
        juce::var object { new juce::DynamicObject() };
        setJsonProperty(object, "analysisSeconds", jsonNumber(source.fullPassSeconds > 0.0f ? source.fullPassSeconds
                                                                                            : source.analysisSeconds));
        setJsonProperty(object, "integratedLufs", jsonAudioNumber(source.integratedLufs));
        setJsonProperty(object, "truePeakDbTp", jsonAudioNumber(source.truePeakDb));
        setJsonProperty(object, "crestDb", jsonNumber(source.crestDb));
        setJsonProperty(object, "lowEndPercent", jsonNumber(lowEndOf(source)));
        setJsonProperty(object, "presencePercent", jsonNumber(presenceOf(source)));
        setJsonProperty(object, "widthPercent", jsonNumber(source.widthPct));
        setJsonProperty(object, "correlation", jsonNumber(source.correlation));
        return object;
    };

    auto root = juce::var { new juce::DynamicObject() };
    setJsonProperty(root, "schema", "funky-moose.mix-analyzer.report");
    setJsonProperty(root, "schemaVersion", 1);
    setJsonProperty(root, "source", "Funky Moose Mix Analyzer Plugin");
    setJsonProperty(root, "measurementStandard", "ITU-R BS.1770-5 / EBU Mode");
    setJsonProperty(root, "generatedAt", juce::Time::getCurrentTime().toISO8601(true));
    setJsonProperty(root, "mode", input.instrumental ? "instrumental" : "vocal-full-mix");
    setJsonProperty(root, "analysisScope", sourceMetrics.hostAutoPassActive ? "host-recording" : sourceAssessment.analysisScope);
    setJsonProperty(root, "analysisSeconds", jsonNumber(sourceMetrics.fullPassCompleted ? sourceMetrics.fullPassSeconds
                                                                                        : sourceMetrics.analysisSeconds));

    auto profileJson = juce::var { new juce::DynamicObject() };
    setJsonProperty(profileJson, "name", profile.name);
    setJsonProperty(profileJson, "group", profile.group);
    setJsonProperty(profileJson, "targetLufs", jsonNumber(profile.targetLufs));
    setJsonProperty(profileJson, "lufsRange", makeRange(profile.lufsRange));
    setJsonProperty(profileJson, "crestRange", makeRange(profile.crestRange));
    setJsonProperty(profileJson, "lowEndRange", makeRange(profile.lowEndRange));
    setJsonProperty(profileJson, "presenceMax", jsonNumber(profile.presenceMax));
    setJsonProperty(profileJson, "correlationMin", jsonNumber(profile.correlationMin));
    setJsonProperty(profileJson, "wideExpected", profile.wideExpected);
    setJsonProperty(root, "genreProfile", profileJson);

    auto scores = juce::var { new juce::DynamicObject() };
    setJsonProperty(scores, "overall", sourceAssessment.overallScore);
    setJsonProperty(scores, "confidence", sourceAssessment.confidenceScore);
    setJsonProperty(scores, "loudnessConfidence", sourceAssessment.loudnessConfidenceScore);
    setJsonProperty(scores, "dynamicsConfidence", sourceAssessment.dynamicsConfidenceScore);
    setJsonProperty(scores, "stereoConfidence", sourceAssessment.stereoConfidenceScore);
    setJsonProperty(scores, "toneConfidence", sourceAssessment.toneConfidenceScore);
    setJsonProperty(scores, "deliveryConfidence", sourceAssessment.deliveryConfidenceScore);
    setJsonProperty(scores, "releaseGate", sourceAssessment.releaseGateScore);
    setJsonProperty(scores, "lufs", sourceAssessment.lufsScore);
    setJsonProperty(scores, "correlation", sourceAssessment.correlationScore);
    setJsonProperty(scores, "lowEnd", sourceAssessment.lowEndScore);
    setJsonProperty(scores, "crest", sourceAssessment.crestScore);
    setJsonProperty(scores, "clipping", sourceAssessment.clippingScore);
    setJsonProperty(scores, "headphone", sourceAssessment.headphoneScore);
    setJsonProperty(scores, "speaker", sourceAssessment.speakerScore);
    setJsonProperty(scores, "lra", sourceAssessment.lraScore);
    setJsonProperty(scores, "transient", sourceAssessment.transientScore);
    setJsonProperty(scores, "tone", sourceAssessment.toneScore);
    setJsonProperty(root, "scores", scores);

    auto assessmentJson = juce::var { new juce::DynamicObject() };
    setJsonProperty(assessmentJson, "verdictKey", sourceAssessment.verdictKey);
    setJsonProperty(assessmentJson, "verdictTitle", sourceAssessment.verdictTitle);
    setJsonProperty(assessmentJson, "statusLine", sourceAssessment.statusLine);
    setJsonProperty(assessmentJson, "confidenceLabel", sourceAssessment.confidenceLabel);
    setJsonProperty(assessmentJson, "confidenceText", sourceAssessment.confidenceText);
    setJsonProperty(assessmentJson, "confidenceBreakdownText", sourceAssessment.confidenceBreakdownText);
    setJsonProperty(assessmentJson, "confidenceCompactText", sourceAssessment.confidenceCompactText);
    auto confidenceJson = juce::var { new juce::DynamicObject() };
    setJsonProperty(confidenceJson, "score", sourceAssessment.confidenceScore);
    setJsonProperty(confidenceJson, "label", sourceAssessment.confidenceLabel);
    setJsonProperty(confidenceJson, "text", sourceAssessment.confidenceText);
    auto confidenceDomains = juce::var { new juce::DynamicObject() };
    setJsonProperty(confidenceDomains, "loudness", sourceAssessment.loudnessConfidenceScore);
    setJsonProperty(confidenceDomains, "dynamics", sourceAssessment.dynamicsConfidenceScore);
    setJsonProperty(confidenceDomains, "stereo", sourceAssessment.stereoConfidenceScore);
    setJsonProperty(confidenceDomains, "tone", sourceAssessment.toneConfidenceScore);
    setJsonProperty(confidenceDomains, "delivery", sourceAssessment.deliveryConfidenceScore);
    setJsonProperty(confidenceJson, "domains", confidenceDomains);
    setJsonProperty(assessmentJson, "confidence", confidenceJson);
    auto releaseGateJson = juce::var { new juce::DynamicObject() };
    setJsonProperty(releaseGateJson, "score", sourceAssessment.releaseGateScore);
    setJsonProperty(releaseGateJson, "ready", sourceAssessment.releaseReady);
    setJsonProperty(releaseGateJson, "title", sourceAssessment.releaseGateTitle);
    setJsonProperty(releaseGateJson, "text", sourceAssessment.releaseGateText);
    juce::Array<juce::var> releaseBlockers;
    for (auto i = 0; i < sourceAssessment.releaseBlockerCount; ++i)
        releaseBlockers.add(sourceAssessment.releaseBlockers[static_cast<size_t>(i)]);
    setJsonProperty(releaseGateJson, "blockers", releaseBlockers);
    setJsonProperty(assessmentJson, "releaseGate", releaseGateJson);
    setJsonProperty(assessmentJson, "measurementReady", sourceAssessment.measurementReady);
    setJsonProperty(assessmentJson, "summary", mixDoctorSummary(sourceMetrics, sourceAssessment));

    juce::Array<juce::var> actions;
    for (auto i = 0; i < sourceAssessment.priorityActionCount; ++i)
        actions.add(sourceAssessment.priorityActions[static_cast<size_t>(i)]);
    if (referenceActionNote(sourceMetrics, sourceAssessment).isNotEmpty())
        actions.add(referenceActionNote(sourceMetrics, sourceAssessment));
    setJsonProperty(assessmentJson, "priorityActions", actions);
    setJsonProperty(root, "assessment", assessmentJson);

    auto autoMasterJson = juce::var { new juce::DynamicObject() };
    setJsonProperty(autoMasterJson, "enabled", sourceMetrics.autoMasterEnabled);
    setJsonProperty(autoMasterJson, "auditionMatch", sourceMetrics.autoMasterAuditionMatch);
    setJsonProperty(autoMasterJson, "strengthPercent", jsonNumber(sourceMetrics.autoMasterStrength));
    setJsonProperty(autoMasterJson, "targetLufs", jsonNumber(sourceMetrics.autoMasterTargetLufs));
    setJsonProperty(autoMasterJson, "ceilingDbTp", jsonNumber(sourceMetrics.autoMasterCeilingDbTp));
    setJsonProperty(autoMasterJson, "gainDb", jsonNumber(sourceMetrics.autoMasterGainDb));
    setJsonProperty(autoMasterJson, "lowShelfDb", jsonNumber(sourceMetrics.autoMasterLowShelfDb));
    setJsonProperty(autoMasterJson, "presenceDb", jsonNumber(sourceMetrics.autoMasterPresenceDb));
    setJsonProperty(autoMasterJson, "airShelfDb", jsonNumber(sourceMetrics.autoMasterAirShelfDb));
    setJsonProperty(autoMasterJson, "widthPercent", jsonNumber(sourceMetrics.autoMasterWidthPercent));
    setJsonProperty(autoMasterJson, "glueReductionDb", jsonNumber(sourceMetrics.autoMasterGlueReductionDb));
    setJsonProperty(autoMasterJson, "limiterReductionDb", jsonNumber(sourceMetrics.autoMasterLimiterReductionDb));
    setJsonProperty(autoMasterJson, "projectedLufs", jsonAudioNumber(sourceMetrics.autoMasterProjectedLufs));
    setJsonProperty(autoMasterJson, "projectedTruePeakDbTp", jsonAudioNumber(sourceMetrics.autoMasterProjectedTruePeakDbTp));
    setJsonProperty(autoMasterJson, "loudnessMatchGainDb", jsonNumber(sourceMetrics.autoMasterLoudnessMatchGainDb));
    setJsonProperty(autoMasterJson, "lufsDeltaDb", jsonNumber(sourceMetrics.autoMasterLufsDeltaDb));
    setJsonProperty(autoMasterJson, "truePeakMarginDb", jsonNumber(sourceMetrics.autoMasterTruePeakMarginDb));
    setJsonProperty(autoMasterJson, "releaseScore", jsonNumber(sourceMetrics.autoMasterReleaseScore));
    setJsonProperty(autoMasterJson, "abLoudnessDeltaDb", jsonNumber(sourceMetrics.autoMasterAbLoudnessDeltaDb));
    setJsonProperty(autoMasterJson, "abTruePeakDbTp", jsonAudioNumber(sourceMetrics.autoMasterAbTruePeakDbTp));
    setJsonProperty(autoMasterJson, "abTruePeakDeltaDb", jsonNumber(sourceMetrics.autoMasterAbTruePeakDeltaDb));
    setJsonProperty(autoMasterJson, "abDynamicsDeltaDb", jsonNumber(sourceMetrics.autoMasterAbDynamicsDeltaDb));
    setJsonProperty(autoMasterJson, "abScore", jsonNumber(sourceMetrics.autoMasterAbScore));
    setJsonProperty(autoMasterJson, "auditionGainDb", jsonNumber(sourceMetrics.autoMasterAuditionGainDb));
    setJsonProperty(autoMasterJson, "auditionLoudnessDeltaDb", jsonNumber(sourceMetrics.autoMasterAuditionLoudnessDeltaDb));
    setJsonProperty(autoMasterJson, "auditionTruePeakDbTp", jsonAudioNumber(sourceMetrics.autoMasterAuditionTruePeakDbTp));
    setJsonProperty(root, "autoMaster", autoMasterJson);

    auto measurements = juce::var { new juce::DynamicObject() };
    setJsonProperty(measurements, "momentaryLufs", jsonAudioNumber(sourceMetrics.momentaryLufs));
    setJsonProperty(measurements, "shortTermLufs", jsonAudioNumber(sourceMetrics.shortTermLufs));
    setJsonProperty(measurements, "integratedLufs", jsonAudioNumber(sourceMetrics.integratedLufs));
    setJsonProperty(measurements, "truePeakDbTp", jsonAudioNumber(sourceMetrics.truePeakDb));
    setJsonProperty(measurements, "rmsDb", jsonAudioNumber(sourceMetrics.rmsDb));
    setJsonProperty(measurements, "samplePeakDb", jsonAudioNumber(sourceMetrics.peakDb));
    setJsonProperty(measurements, "crestDb", jsonNumber(sourceMetrics.crestDb));
    setJsonProperty(measurements, "lraLu", jsonNumber(sourceMetrics.lraLu));
    setJsonProperty(measurements, "correlation", jsonNumber(sourceMetrics.correlation));
    setJsonProperty(measurements, "widthPercent", jsonNumber(sourceMetrics.widthPct));
    setJsonProperty(measurements, "msRatioDb", jsonNumber(sourceMetrics.msRatioDb));
    setJsonProperty(measurements, "stereoBalanceDb", jsonNumber(sourceMetrics.stereoBalanceDb));
    setJsonProperty(measurements, "monoRmsDb", jsonAudioNumber(sourceMetrics.monoRmsDb));
    setJsonProperty(measurements, "monoLossDb", jsonNumber(sourceMetrics.monoLossDb));
    setJsonProperty(measurements, "dcOffset", jsonNumber(sourceMetrics.dcOffset));
    setJsonProperty(measurements, "clippedPercent", jsonNumber(sourceMetrics.clippedPercent));
    setJsonProperty(measurements, "silencePercent", jsonNumber(sourceMetrics.silencePercent));
    setJsonProperty(measurements, "transientDensityPerSecond", jsonNumber(sourceMetrics.transientDensity));
    setJsonProperty(measurements, "attackTimeMs", jsonNumber(sourceMetrics.attackTimeMs));
    setJsonProperty(measurements, "percussionEnergyPercent", jsonNumber(sourceMetrics.percussionEnergyPct));
    setJsonProperty(measurements, "spectralCentroidHz", jsonNumber(sourceMetrics.spectralCentroidHz));
    setJsonProperty(measurements, "spectralRolloffHz", jsonNumber(sourceMetrics.spectralRolloffHz));
    setJsonProperty(measurements, "resonanceFreqHz", jsonNumber(sourceMetrics.resonanceFreqHz));
    setJsonProperty(measurements, "resonanceGainDb", jsonNumber(sourceMetrics.resonanceGainDb));
    setJsonProperty(measurements, "lowEndPercent", jsonNumber(input.lowEndPercent));
    setJsonProperty(measurements, "presencePercent", jsonNumber(input.presencePercent));
    setJsonProperty(measurements, "lowEndCorrelation", jsonNumber(input.lowEndCorrelation));
    setJsonProperty(measurements, "lowEndSideDb", jsonAudioNumber(input.lowEndSideDb));

    juce::Array<juce::var> bands;
    for (auto i = 0; i < fmma::bandCount; ++i)
    {
        const auto index = static_cast<size_t>(i);
        juce::var band { new juce::DynamicObject() };
        setJsonProperty(band, "name", fmma::bandNames[index]);
        setJsonProperty(band, "percent", jsonNumber(sourceMetrics.bandPercents[index]));
        setJsonProperty(band, "correlation", jsonNumber(sourceMetrics.bandCorrelations[index]));
        setJsonProperty(band, "sideRatioDb", jsonAudioNumber(sourceMetrics.bandSideRatiosDb[index]));
        bands.add(band);
    }
    setJsonProperty(measurements, "bands", bands);
    setJsonProperty(root, "measurements", measurements);

    auto worstCase = juce::var { new juce::DynamicObject() };
    setJsonProperty(worstCase, "truePeakDbTp", jsonAudioNumber(sourceMetrics.worstTruePeakDb));
    setJsonProperty(worstCase, "correlation", jsonNumber(sourceMetrics.worstCorrelation));
    setJsonProperty(worstCase, "monoLossDb", jsonNumber(sourceMetrics.worstMonoLossDb));
    setJsonProperty(worstCase, "clippedPercent", jsonNumber(sourceMetrics.worstClippedPercent));
    setJsonProperty(worstCase, "lowMidPercent", jsonNumber(sourceMetrics.worstLowMidPercent));
    setJsonProperty(worstCase, "resonanceFreqHz", jsonNumber(sourceMetrics.worstResonanceFreqHz));
    setJsonProperty(worstCase, "resonanceGainDb", jsonNumber(sourceMetrics.worstResonanceGainDb));
    setJsonProperty(root, "worstCase", worstCase);

    juce::Array<juce::var> deliveryPreview;
    auto addDeliveryTarget = [&] (const char* name, float targetLufs)
    {
        juce::var target { new juce::DynamicObject() };
        setJsonProperty(target, "name", name);
        setJsonProperty(target, "targetLufs", jsonNumber(targetLufs));

        const auto hasLoudness = sourceMetrics.integratedLufs > -119.0f && std::isfinite(sourceMetrics.integratedLufs);
        const auto hasTruePeak = truePeakForReport > -119.0f && std::isfinite(truePeakForReport);
        if (hasLoudness && hasTruePeak)
        {
            const auto gainDelta = targetLufs - sourceMetrics.integratedLufs;
            setJsonProperty(target, "gainDb", jsonNumber(gainDelta));
            setJsonProperty(target, "estimatedTruePeakDbTp", jsonNumber(truePeakForReport + gainDelta));
        }
        else
        {
            setJsonProperty(target, "gainDb", juce::var());
            setJsonProperty(target, "estimatedTruePeakDbTp", juce::var());
        }

        deliveryPreview.add(target);
    };
    addDeliveryTarget("Streaming -14 LUFS", -14.0f);
    addDeliveryTarget("Apple -16 LUFS", -16.0f);
    addDeliveryTarget("Broadcast -23 LUFS", -23.0f);
    setJsonProperty(root, "deliveryPreview", deliveryPreview);

    auto reference = juce::var { new juce::DynamicObject() };
    setJsonProperty(reference, "captured", hasReferenceMetrics);
    setJsonProperty(reference, "note", referenceNote(sourceMetrics, sourceAssessment));
    if (referenceActionNote(sourceMetrics, sourceAssessment).isNotEmpty())
        setJsonProperty(reference, "action", referenceActionNote(sourceMetrics, sourceAssessment));
    if (hasReferenceMetrics)
    {
        setJsonProperty(reference, "metrics", makeMetricSummary(referenceMetrics));
        setJsonProperty(reference, "lufsDelta", jsonNumber(sourceMetrics.integratedLufs - referenceMetrics.integratedLufs));
        setJsonProperty(reference, "lowEndDeltaPercent", jsonNumber(lowEndOf(sourceMetrics) - lowEndOf(referenceMetrics)));
        setJsonProperty(reference, "presenceDeltaPercent", jsonNumber(presenceOf(sourceMetrics) - presenceOf(referenceMetrics)));
        setJsonProperty(reference, "crestDeltaDb", jsonNumber(sourceMetrics.crestDb - referenceMetrics.crestDb));
        setJsonProperty(reference, "widthDeltaPercent", jsonNumber(sourceMetrics.widthPct - referenceMetrics.widthPct));
    }
    setJsonProperty(root, "reference", reference);

    auto snapshots = juce::var { new juce::DynamicObject() };
    setJsonProperty(snapshots, "hasA", hasSnapshotA);
    setJsonProperty(snapshots, "hasB", hasSnapshotB);
    setJsonProperty(snapshots, "note", snapshotNote());
    if (hasSnapshotA)
        setJsonProperty(snapshots, "a", makeMetricSummary(snapshotA));
    if (hasSnapshotB)
        setJsonProperty(snapshots, "b", makeMetricSummary(snapshotB));
    if (hasSnapshotA && hasSnapshotB)
    {
        auto deltas = juce::var { new juce::DynamicObject() };
        setJsonProperty(deltas, "lufs", jsonNumber(snapshotB.integratedLufs - snapshotA.integratedLufs));
        setJsonProperty(deltas, "truePeakDb", jsonNumber(snapshotB.truePeakDb - snapshotA.truePeakDb));
        setJsonProperty(deltas, "lowEndPercent", jsonNumber(lowEndOf(snapshotB) - lowEndOf(snapshotA)));
        setJsonProperty(deltas, "presencePercent", jsonNumber(presenceOf(snapshotB) - presenceOf(snapshotA)));
        setJsonProperty(deltas, "crestDb", jsonNumber(snapshotB.crestDb - snapshotA.crestDb));
        setJsonProperty(deltas, "widthPercent", jsonNumber(snapshotB.widthPct - snapshotA.widthPct));
        setJsonProperty(snapshots, "deltaBMinusA", deltas);
    }
    setJsonProperty(root, "snapshots", snapshots);

    setJsonProperty(root, "plainTextReport", buildTextReport(sourceMetrics, input, sourceAssessment));

    return juce::JSON::toString(root, false, 4);
}
