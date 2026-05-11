#include "ScoreComponent.h"
#include "Theme.h"

ScoreComponent::ScoreComponent()
{
}

void ScoreComponent::update(const Data& newData)
{
    data = newData;
    repaint();
}

void ScoreComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    Theme::drawCard(g, area, "Score Components");
    auto inner = area.reduced(18.0f);
    inner.removeFromTop(34.0f);

    const std::array<std::pair<const char*, int>, 10> rows {{
        { "LUFS", data.lufsScore },
        { "Correlation", data.correlationScore },
        { "Low-End", data.lowEndScore },
        { "Crest", data.crestScore },
        { "LRA", data.lraScore },
        { "Clipping", data.clippingScore },
        { "Tone", data.toneScore },
        { "Headphones", data.headphoneScore },
        { "Speakers", data.speakerScore },
        { "Transients", data.transientScore },
    }};

    auto leftColumn = inner.removeFromLeft((inner.getWidth() - 14.0f) * 0.5f);
    inner.removeFromLeft(14.0f);
    auto rightColumn = inner;
    const auto rowsPerColumn = (rows.size() + size_t { 1 }) / size_t { 2 };

    for (auto i = size_t { 0 }; i < rows.size(); ++i)
    {
        auto& column = i < rowsPerColumn ? leftColumn : rightColumn;
        const auto& row = rows[i];
        auto line = column.removeFromTop(22.0f);
        auto label = line.removeFromLeft(72.0f);
        auto value = line.removeFromRight(34.0f);
        
        g.setColour(Theme::muted);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(row.first, label, juce::Justification::centredLeft);
        
        const auto colour = Theme::scoreColour(row.second);
        g.setColour(colour);
        g.drawText(juce::String(row.second), value, juce::Justification::centredRight);
        
        Theme::drawBar(g, line.reduced(8.0f, 0.0f), static_cast<float>(row.second) / 100.0f, colour);
    }
}
