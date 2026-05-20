#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/SummaryComponent.h"
#include "UI/MetricsCardComponent.h"
#include "UI/ToneShapeComponent.h"
#include "UI/ActionChecklistComponent.h"
#include "UI/ScoreComponent.h"
#include "UI/CompareComponent.h"

class FunkyMooseMixAnalyzerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                        private juce::Timer
{
public:
    explicit FunkyMooseMixAnalyzerAudioProcessorEditor(FunkyMooseMixAnalyzerAudioProcessor&);
    ~FunkyMooseMixAnalyzerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void copyReportToClipboard();
    void copyJsonReportToClipboard();
    juce::String buildTextReport() const;
    juce::String buildTextReport(const fmma::AnalyzerMetrics& sourceMetrics,
                                 const fmma::MixAssessmentInput& input,
                                 const fmma::MixAssessment& sourceAssessment) const;
    juce::String buildJsonReport() const;
    juce::String buildJsonReport(const fmma::AnalyzerMetrics& sourceMetrics,
                                 const fmma::MixAssessmentInput& input,
                                 const fmma::MixAssessment& sourceAssessment) const;
    juce::String formatDuration(float seconds) const;
    juce::String formatSigned(float value, const juce::String& suffix, int decimals = 1) const;
    juce::String formatDeliveryPreview(float targetLufs) const;
    juce::String formatDeliveryPreview(float targetLufs, const fmma::AnalyzerMetrics& sourceMetrics) const;
    juce::String referenceNote() const;
    juce::String referenceNote(const fmma::AnalyzerMetrics& sourceMetrics,
                               const fmma::MixAssessment& sourceAssessment) const;
    juce::String referenceActionNote() const;
    juce::String referenceActionNote(const fmma::AnalyzerMetrics& sourceMetrics,
                                     const fmma::MixAssessment& sourceAssessment) const;
    juce::String snapshotNote() const;
    juce::String mixDoctorSummary() const;
    juce::String mixDoctorSummary(const fmma::AnalyzerMetrics& sourceMetrics,
                                  const fmma::MixAssessment& sourceAssessment) const;
    fmma::MixAssessmentInput makeAssessmentInput() const;
    void smoothDisplayMetrics(const fmma::AnalyzerMetrics& raw);
    void syncStoredSnapshotsFromProcessor();

    static juce::String formatDb(float value, int decimals = 1);
    static juce::String formatDbTp(float value, int decimals = 1);
    static juce::String formatLufs(float value, int decimals = 1);
    static juce::String formatNumber(float value, int decimals = 1);

    FunkyMooseMixAnalyzerAudioProcessor& audioProcessor;
    fmma::AnalyzerMetrics metrics;
    fmma::MixAssessment assessment;
    bool hasDisplayMetrics = false;
    int assessmentCountdown = 0;
    int lastGenreIndex = -1;
    bool lastInstrumentalState = false;
    fmma::AnalyzerMetrics referenceMetrics;
    bool hasReferenceMetrics = false;
    bool referenceCaptureInProgress = false;
    bool pendingReferenceSnapshot = false;
    fmma::AnalyzerMetrics snapshotA;
    fmma::AnalyzerMetrics snapshotB;
    bool hasSnapshotA = false;
    bool hasSnapshotB = false;

    juce::Label genreLabel;
    juce::ComboBox genreBox;
    juce::ToggleButton instrumentalToggle;
    juce::ToggleButton autoMasterToggle;
    juce::ToggleButton autoMasterAuditionToggle;
    juce::Slider autoMasterStrengthSlider;
    juce::TextButton passButton;
    juce::TextButton referenceButton;
    juce::TextButton clearReferenceButton;
    juce::TextButton snapshotAButton;
    juce::TextButton snapshotBButton;
    juce::TextButton clearSnapshotsButton;
    juce::TextButton resetButton;
    juce::TextButton copyButton;
    juce::TextButton copyJsonButton;
    std::unique_ptr<ComboAttachment> genreAttachment;
    std::unique_ptr<ButtonAttachment> instrumentalAttachment;
    std::unique_ptr<ButtonAttachment> autoMasterAttachment;
    std::unique_ptr<ButtonAttachment> autoMasterAuditionAttachment;
    std::unique_ptr<SliderAttachment> autoMasterStrengthAttachment;

    SummaryComponent summaryComponent;
    MetricsCardComponent loudnessCard;
    MetricsCardComponent dynamicsCard;
    MetricsCardComponent stereoCard;
    MetricsCardComponent qualityCard;
    ToneShapeComponent toneShapeComponent;
    ActionChecklistComponent targetsComponent;
    ActionChecklistComponent actionsComponent;
    ScoreComponent scoreComponent;
    CompareComponent referenceComponent;
    CompareComponent snapshotComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseMixAnalyzerAudioProcessorEditor)
};
