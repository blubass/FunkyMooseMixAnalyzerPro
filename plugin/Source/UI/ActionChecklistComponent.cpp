#include "ActionChecklistComponent.h"
#include "Theme.h"

ActionChecklistComponent::ActionChecklistComponent()
{
}

void ActionChecklistComponent::update(const Data& newData)
{
    data = newData;
    repaint();
}

void ActionChecklistComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    Theme::drawCard(g, area, data.title);

    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    if (data.items.empty() && data.emptyMessage.isNotEmpty())
    {
        g.setColour(data.emptyMessageColour);
        g.setFont(juce::FontOptions(data.fontHeight + 2.0f, juce::Font::bold));
        g.drawText(data.emptyMessage, inner, juce::Justification::centred);
        return;
    }

    if (data.items.empty())
        return;

    const auto rowHeight = inner.getHeight() / static_cast<float>(data.items.size());

    for (const auto& item : data.items)
    {
        auto line = inner.removeFromTop(rowHeight);
        
        // Draw Dot
        auto dotArea = line.removeFromLeft(item.dotSize);
        auto dotRect = dotArea.withSizeKeepingCentre(item.dotSize, item.dotSize);
        g.setColour(item.dotColour);
        g.fillEllipse(dotRect.reduced(1.0f));
        
        // Draw Text
        line.removeFromLeft(8.0f);
        g.setColour(Theme::text);
        g.setFont(juce::FontOptions(data.fontHeight));
        g.drawText(item.text, line, juce::Justification::centredLeft);
    }
}
