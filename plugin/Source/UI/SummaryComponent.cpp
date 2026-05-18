#include "SummaryComponent.h"
#include "Theme.h"

SummaryComponent::SummaryComponent()
{
}

void SummaryComponent::update(const Data& newData)
{
    data = newData;
    repaint();
}

juce::String SummaryComponent::formatDuration(float seconds) const
{
    if (! std::isfinite(seconds) || seconds <= 0.0f)
        return "0:00";

    const auto totalSeconds = static_cast<int>(std::round(seconds));
    const auto minutes = totalSeconds / 60;
    const auto remainingSeconds = totalSeconds % 60;
    return juce::String(minutes) + ":" + juce::String(remainingSeconds).paddedLeft('0', 2);
}

void SummaryComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    Theme::drawCard(g, area, "Live Summary");

    auto inner = area.reduced(18.0f);
    inner.removeFromTop(26.0f);
    auto scoreArea = inner.removeFromLeft(128.0f);
    auto details = inner;
    const auto colour = Theme::scoreColour(data.overallScore);

    g.setColour(colour.withAlpha(0.12f));
    g.fillRoundedRectangle(scoreArea.reduced(0.0f, 3.0f), 8.0f);
    g.setColour(colour);
    g.setFont(juce::FontOptions(42.0f, juce::Font::bold));
    g.drawText(juce::String(data.overallScore), scoreArea.removeFromTop(48.0f), juce::Justification::centred);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("MIX SCORE", scoreArea, juce::Justification::centred);

    details.removeFromLeft(18.0f);
    auto topLine = details.removeFromTop(24.0f);
    g.setColour(colour);
    g.setFont(juce::FontOptions(19.0f, juce::Font::bold));
    g.drawText(data.verdictTitle, topLine.removeFromLeft(170.0f), juce::Justification::centredLeft);
    g.setColour(Theme::muted);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(data.scopeLabel + " / " + data.genreGroup,
               topLine,
               juce::Justification::centredLeft);

    g.setColour(Theme::text);
    g.setFont(juce::FontOptions(14.0f));
    g.drawText(data.statusLine, details.removeFromTop(20.0f), juce::Justification::centredLeft);

    auto chips = details.removeFromTop(22.0f);
    Theme::drawMetric(g, chips.removeFromLeft(166.0f), "Confidence",
                      data.confidenceLabel + " " + juce::String(data.confidenceScore));
    Theme::drawMetric(g, chips.removeFromLeft(124.0f), "Time", formatDuration(data.durationSeconds));
    Theme::drawMetric(g, chips.removeFromLeft(138.0f), "LUFS d", juce::String(data.lufsDelta, 1));
    Theme::drawMetric(g, chips.removeFromLeft(138.0f), "Low", juce::String(data.lowEndPercent, 1) + "%");
    Theme::drawMetric(g, chips.removeFromLeft(138.0f), "Presence", juce::String(data.presencePercent, 1) + "%");

    if (data.confidenceCompactText.isNotEmpty() && chips.getWidth() > 150.0f)
        Theme::drawMetric(g, chips.removeFromLeft(juce::jmin(248.0f, chips.getWidth())),
                          "Rel L/D/S/T/P",
                          data.confidenceCompactText);
}
