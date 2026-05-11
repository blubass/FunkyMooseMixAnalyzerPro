#include "ToneShapeComponent.h"
#include "Theme.h"
#include "../AnalysisModel.h"



ToneShapeComponent::ToneShapeComponent()
{
}

void ToneShapeComponent::update(const Data& newData)
{
    data = newData;
    repaint();
}

float ToneShapeComponent::frequencyToUnit(float hz) const noexcept
{
    if (! std::isfinite(hz) || hz <= 0.0f)
        return 0.0f;

    const auto logMin = std::log10(20.0f);
    const auto logMax = std::log10(20000.0f);
    return juce::jlimit(0.0f, 1.0f, (std::log10(hz) - logMin) / (logMax - logMin));
}

void ToneShapeComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    Theme::drawCard(g, area, "Tone Shape");

    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    auto graph = inner.removeFromTop(82.0f);
    auto labelStrip = graph.removeFromBottom(15.0f);
    auto plot = graph.reduced(0.0f, 2.0f);

    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(plot, 6.0f);
    g.setColour(Theme::border);
    g.drawRoundedRectangle(plot, 6.0f, 1.0f);

    for (auto i = 1; i < 4; ++i)
    {
        const auto y = plot.getY() + (plot.getHeight() * static_cast<float>(i) / 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawHorizontalLine(static_cast<int>(std::round(y)), plot.getX(), plot.getRight());
    }

    juce::Path fillPath;
    juce::Path linePath;
    const auto bandCount = data.bandPercents.size();
    const auto step = plot.getWidth() / static_cast<float>(bandCount - 1);

    for (size_t band = 0; band < bandCount; ++band)
    {
        const auto value = juce::jlimit(0.0f, 1.0f, data.bandPercents[band] / 45.0f);
        const auto x = plot.getX() + (static_cast<float>(band) * step);
        const auto y = plot.getBottom() - (value * plot.getHeight());

        if (band == 0)
        {
            linePath.startNewSubPath(x, y);
            fillPath.startNewSubPath(x, plot.getBottom());
            fillPath.lineTo(x, y);
        }
        else
        {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }

        g.setColour((band < 2 ? Theme::teal : band < 4 ? Theme::violet : Theme::amber).withAlpha(0.88f));
        g.fillEllipse(juce::Rectangle<float> { x - 3.0f, y - 3.0f, 6.0f, 6.0f });
    }

    fillPath.lineTo(plot.getRight(), plot.getBottom());
    fillPath.closeSubPath();
    g.setColour(Theme::teal.withAlpha(0.14f));
    g.fillPath(fillPath);
    g.setColour(Theme::teal);
    g.strokePath(linePath, juce::PathStrokeType(2.0f));

    const auto drawFrequencyMarker = [&] (float frequencyHz, juce::Colour colour, const juce::String& label)
    {
        if (! std::isfinite(frequencyHz) || frequencyHz <= 0.0f)
            return;

        const auto x = plot.getX() + (frequencyToUnit(frequencyHz) * plot.getWidth());
        g.setColour(colour.withAlpha(0.75f));
        g.drawVerticalLine(static_cast<int>(std::round(x)), plot.getY(), plot.getBottom());
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawFittedText(label, juce::Rectangle<int> { static_cast<int>(x) + 3,
                                                       static_cast<int>(plot.getY()) + 2,
                                                       54,
                                                       12 },
                         juce::Justification::centredLeft,
                         1);
    };

    drawFrequencyMarker(data.spectralCentroidHz, Theme::teal, "C");
    drawFrequencyMarker(data.spectralRolloffHz, Theme::amber, "R");
    if (data.resonanceFreqHz > 0.0f && data.resonanceGainDb >= 6.0f)
        drawFrequencyMarker(data.resonanceFreqHz, Theme::danger, "Res");

    const auto labelWidth = labelStrip.getWidth() / static_cast<float>(bandCount);
    for (size_t band = 0; band < bandCount; ++band)
    {
        auto labelArea = labelStrip.removeFromLeft(labelWidth);
        if (band == 0) labelArea.removeFromLeft(4.0f);
        if (band == bandCount - 1) labelArea.removeFromRight(4.0f);

        g.setColour(Theme::muted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(fmma::bandNames[band], labelArea,
                   band == 0 ? juce::Justification::centredLeft
                             : band == bandCount - 1 ? juce::Justification::centredRight
                                                     : juce::Justification::centred);
    }

    area.removeFromTop(8.0f);
    Theme::drawMetric(g, area.removeFromTop(18.0f), "Centroid", data.spectralCentroidString);
    Theme::drawMetric(g, area.removeFromTop(18.0f), "Rolloff", data.spectralRolloffString);
    Theme::drawMetric(g, area.removeFromTop(18.0f), "Resonance", data.resonanceString);
}
