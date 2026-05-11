#pragma once

#include <JuceHeader.h>
#include <vector>

class CompareComponent : public juce::Component
{
public:
    CompareComponent();
    ~CompareComponent() override = default;

    void paint(juce::Graphics& g) override;

    struct Metric
    {
        juce::String label;
        juce::String value;
    };

    struct Data
    {
        juce::String title;
        juce::String emptyMessage; // Rendered above metrics if not empty
        std::vector<Metric> metrics;
        juce::String noteTitle;
        juce::String noteText;
        float rowHeight = 20.0f; // Can be adjusted
    };

    void update(const Data& newData);

private:
    Data data;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompareComponent)
};
