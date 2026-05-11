#pragma once

#include <JuceHeader.h>
#include <vector>

class ActionChecklistComponent : public juce::Component
{
public:
    ActionChecklistComponent();
    ~ActionChecklistComponent() override = default;

    void paint(juce::Graphics& g) override;

    struct Item
    {
        juce::String text;
        juce::Colour dotColour;
        float dotSize = 8.0f;
    };

    struct Data
    {
        juce::String title;
        std::vector<Item> items;
        juce::String emptyMessage;
        juce::Colour emptyMessageColour;
        float fontHeight = 13.0f;
    };

    void update(const Data& newData);

private:
    Data data;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActionChecklistComponent)
};
