#pragma once

#include <JuceHeader.h>

class SummaryComponent : public juce::Component
{
public:
    SummaryComponent();
    ~SummaryComponent() override = default;

    void paint(juce::Graphics& g) override;

    struct Data
    {
        int overallScore = 0;
        juce::String verdictTitle;
        juce::String scopeLabel;
        juce::String genreGroup;
        juce::String statusLine;
        
        juce::String confidenceLabel;
        int confidenceScore = 0;
        juce::String confidenceCompactText;
        
        float durationSeconds = 0.0f;
        float lufsDelta = 0.0f;
        float lowEndPercent = 0.0f;
        float presencePercent = 0.0f;
    };

    void update(const Data& newData);

private:
    Data data;

    juce::String formatDuration(float seconds) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SummaryComponent)
};
