#include "ResponseGraph.h"
#include "ScribaLookAndFeel.h"

#include <cmath>

static constexpr double graphMinHz = 20.0;
static constexpr double graphMaxHz = 20000.0;

juce::Rectangle<float> ResponseGraph::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (36.0f).withTrimmedBottom (20.0f).withTrimmedTop (4.0f).withTrimmedRight (8.0f);
}

float ResponseGraph::xForFreq (double hz) const
{
    const auto area = plotArea();
    const double t = std::log (hz / graphMinHz) / std::log (graphMaxHz / graphMinHz);
    return area.getX() + (float) t * area.getWidth();
}

float ResponseGraph::yForDb (float db) const
{
    const auto area = plotArea();
    const float t = (dbRange - db) / (2.0f * dbRange);
    return area.getY() + juce::jlimit (0.0f, 1.0f, t) * area.getHeight();
}

void ResponseGraph::paint (juce::Graphics& g)
{
    using namespace ScribaColours;
    const auto area = plotArea();

    if (highlightHz > graphMinHz)
    {
        const float x = xForFreq (juce::jmin (highlightHz, graphMaxHz));
        g.setColour (left.withAlpha (0.05f));
        g.fillRect (area.withRight (x));
        g.setColour (textMuted);
        g.setFont (ScribaLookAndFeel::font (10.0f));
        g.drawText (highlightLabel, area.withHeight (16.0f).translated (6.0f, 2.0f), juce::Justification::centredLeft);
    }

    // griglia
    g.setFont (ScribaLookAndFeel::mono (10.0f));
    const double freqLines[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* freqLabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

    for (int i = 0; i < 10; ++i)
    {
        const float x = xForFreq (freqLines[i]);
        g.setColour (grid);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (textMuted);
        g.drawText (freqLabels[i], juce::Rectangle<float> (x - 20.0f, area.getBottom() + 4.0f, 40.0f, 14.0f), juce::Justification::centred);
    }

    const float step = dbRange >= 15.0f ? 5.0f : (dbRange >= 8.0f ? 3.0f : 2.0f);
    for (float db = -std::floor (dbRange / step) * step; db <= dbRange; db += step)
    {
        const float y = yForDb (db);
        g.setColour (std::abs (db) < 0.01f ? controlHi.withAlpha (0.6f) : grid);
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (textMuted);
        auto label = db > 0.0f ? "+" + juce::String ((int) db) : juce::String ((int) db);
        g.drawText (label, juce::Rectangle<float> (0.0f, y - 7.0f, area.getX() - 6.0f, 14.0f), juce::Justification::centredRight);
    }

    if (curves.empty() && emptyMessage.isNotEmpty())
    {
        g.setColour (textMuted);
        g.setFont (ScribaLookAndFeel::font (13.0f));
        g.drawFittedText (emptyMessage, area.reduced (30.0f).toNearestInt(), juce::Justification::centred, 3);
        return;
    }

    // curve
    g.saveState();
    g.reduceClipRegion (area.expanded (1.0f).toNearestInt());

    for (auto& c : curves)
    {
        juce::Path p;
        bool started = false;

        for (int i = 0; i < scriba::FreqGrid::numPoints; ++i)
        {
            const double f = scriba::FreqGrid::freq (i);
            if (f < graphMinHz * 0.95 || f > graphMaxHz * 1.05)
                continue;

            const float x = xForFreq (f);
            const float y = yForDb (c.response.db[(size_t) i]);

            if (! started) { p.startNewSubPath (x, y); started = true; }
            else           p.lineTo (x, y);
        }

        juce::PathStrokeType stroke (c.thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (c.colour);

        if (c.dashed)
        {
            juce::Path dashed;
            const float pattern[] = { 6.0f, 5.0f };
            stroke.createDashedStroke (dashed, p, pattern, 2);
            g.fillPath (dashed);
        }
        else
        {
            g.strokePath (p, stroke);
        }
    }

    g.restoreState();
}
