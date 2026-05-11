#pragma once

#include <JuceHeader.h>
#include <array>

class ScoreComponent : public juce::Component
{
public:
    ScoreComponent();
    ~ScoreComponent() override = default;

    void paint(juce::Graphics& g) override;

    struct Data
    {
        int lufsScore = 0;
        int correlationScore = 0;
        int lowEndScore = 0;
        int crestScore = 0;
        int lraScore = 0;
        int clippingScore = 0;
        int toneScore = 0;
        int headphoneScore = 0;
        int speakerScore = 0;
        int transientScore = 0;
    };

    void update(const Data& newData);

private:
    Data data;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScoreComponent)
};
