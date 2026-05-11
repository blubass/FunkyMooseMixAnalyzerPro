#pragma once

#include <JuceHeader.h>
#include <array>

class ToneShapeComponent : public juce::Component
{
public:
    ToneShapeComponent();
    ~ToneShapeComponent() override = default;

    void paint(juce::Graphics& g) override;

    struct Data
    {
        std::array<float, 6> bandPercents{};
        float spectralCentroidHz = 0.0f;
        float spectralRolloffHz = 0.0f;
        float resonanceFreqHz = 0.0f;
        float resonanceGainDb = 0.0f;
        
        juce::String spectralCentroidString;
        juce::String spectralRolloffString;
        juce::String resonanceString;
    };

    void update(const Data& newData);

private:
    Data data;
    
    float frequencyToUnit(float hz) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToneShapeComponent)
};
