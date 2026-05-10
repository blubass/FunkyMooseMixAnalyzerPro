#pragma once

#include "PluginProcessor.h"

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

    void timerCallback() override;
    void drawCard(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title);
    void drawSummary(juce::Graphics& g, juce::Rectangle<float> area);
    void drawMetric(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label, const juce::String& value);
    void drawBar(juce::Graphics& g, juce::Rectangle<float> area, float normalised, juce::Colour colour);
    void drawToneShape(juce::Graphics& g, juce::Rectangle<float> area);
    void drawTargetChecklist(juce::Graphics& g, juce::Rectangle<float> area);
    void drawScoreComponents(juce::Graphics& g, juce::Rectangle<float> area);
    void drawReferenceCompare(juce::Graphics& g, juce::Rectangle<float> area);
    void drawSnapshotCompare(juce::Graphics& g, juce::Rectangle<float> area);
    void drawPriorityActions(juce::Graphics& g, juce::Rectangle<float> area);
    void copyReportToClipboard();
    void copyJsonReportToClipboard();
    juce::String buildTextReport() const;
    juce::String buildJsonReport() const;
    juce::String formatDuration(float seconds) const;
    juce::String formatSigned(float value, const juce::String& suffix, int decimals = 1) const;
    juce::String formatDeliveryPreview(float targetLufs) const;
    juce::String referenceNote() const;
    juce::String referenceActionNote() const;
    juce::String snapshotNote() const;
    juce::String mixDoctorSummary() const;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FunkyMooseMixAnalyzerAudioProcessorEditor)
};
