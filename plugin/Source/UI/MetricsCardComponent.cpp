#include "MetricsCardComponent.h"
#include "Theme.h"

MetricsCardComponent::MetricsCardComponent()
{
}

void MetricsCardComponent::update(const Data& newData)
{
    data = newData;
    repaint();
}

void MetricsCardComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    Theme::drawCard(g, area, data.title);

    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);
    auto barArea = inner.removeFromBottom(18.0f);

    for (const auto& metric : data.metrics)
    {
        Theme::drawMetric(g, inner.removeFromTop(data.rowHeight), metric.first, metric.second);
    }

    Theme::drawBar(g, barArea, data.barValue, data.barColour);
}
