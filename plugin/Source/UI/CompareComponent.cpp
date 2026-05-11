#include "CompareComponent.h"
#include "Theme.h"

CompareComponent::CompareComponent()
{
}

void CompareComponent::update(const Data& newData)
{
    data = newData;
    repaint();
}

void CompareComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    Theme::drawCard(g, area, data.title);
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    if (data.emptyMessage.isNotEmpty())
    {
        g.setColour(Theme::muted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(data.emptyMessage, inner.removeFromTop(24.0f), juce::Justification::centredLeft);
    }

    for (const auto& metric : data.metrics)
    {
        Theme::drawMetric(g, inner.removeFromTop(data.rowHeight), metric.label, metric.value);
    }

    if (data.noteTitle.isNotEmpty())
    {
        inner.removeFromTop(4.0f);
        g.setColour(Theme::teal);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(data.noteTitle, inner.removeFromTop(18.0f), juce::Justification::centredLeft);
        g.setColour(Theme::text);
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText(data.noteText, inner.toNearestInt(), juce::Justification::centredLeft, 2);
    }
}
