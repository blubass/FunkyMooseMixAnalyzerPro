#pragma once

#include <JuceHeader.h>
#include <vector>

class MetricsCardComponent : public juce::Component
{
public:
    MetricsCardComponent();
    ~MetricsCardComponent() override = default;

    void paint(juce::Graphics& g) override;

    struct Data
    {
        juce::String title;
        std::vector<std::pair<juce::String, juce::String>> metrics;
        float barValue = 0.0f;
        juce::Colour barColour;
        float rowHeight = 20.0f;
    };

    void update(const Data& newData);

private:
    Data data;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetricsCardComponent)
};
