#pragma once

#include <JuceHeader.h>

namespace Theme
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

    inline juce::Colour scoreColour(int score) noexcept
    {
        return score >= 80 ? success : score >= 60 ? amber : danger;
    }

    inline juce::Colour okColour(bool ok) noexcept
    {
        return ok ? success : amber;
    }

    inline void drawCard(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title)
    {
        g.setColour(panel);
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(border);
        g.drawRoundedRectangle(area, 8.0f, 1.0f);
        g.setColour(text);
        g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
        g.drawText(title, area.reduced(18.0f).removeFromTop(24.0f), juce::Justification::centredLeft);
    }

    inline void drawMetric(juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label, const juce::String& value)
    {
        g.setFont(juce::FontOptions(13.0f));
        g.setColour(muted);
        g.drawText(label, area.removeFromLeft(area.getWidth() * 0.56f), juce::Justification::centredLeft);
        g.setColour(text);
        g.drawText(value, area, juce::Justification::centredRight);
    }

    inline void drawBar(juce::Graphics& g, juce::Rectangle<float> area, float normalised, juce::Colour colour)
    {
        area = area.withHeight(10.0f).withCentre(area.getCentre());
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(colour);
        g.fillRoundedRectangle(area.withWidth(area.getWidth() * juce::jlimit(0.0f, 1.0f, normalised)), 5.0f);
    }
}
