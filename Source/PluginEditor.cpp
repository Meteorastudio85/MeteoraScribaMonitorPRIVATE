#include "PluginEditor.h"

using namespace ScribaColours;

namespace
{
    constexpr int headerHeight = 100;
    constexpr int footerHeight = 34;
}

ScribaMonitorEditor::ScribaMonitorEditor (ScribaMonitorProcessor& p)
    : AudioProcessorEditor (&p), processor (p), correctionPage (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (correctionPage);

    if (processor.isStandalone())
    {
        measurePage = std::make_unique<MeasurePage> (processor);
        measurePage->onProfileSaved = [this] { showPage (1); };
        addAndMakeVisible (*measurePage);

        for (auto* tab : { &measureTab, &correctionTab })
        {
            tab->setClickingTogglesState (false);
            addAndMakeVisible (tab);
        }

        measureTab.onClick    = [this] { showPage (0); };
        correctionTab.onClick = [this] { showPage (1); };

        setSize (1100, 664 + headerHeight + footerHeight);
        showPage (0);
    }
    else
    {
        setSize (900, 634 + headerHeight + footerHeight);
    }

    startTimerHz (4);
}

ScribaMonitorEditor::~ScribaMonitorEditor()
{
    setLookAndFeel (nullptr);
}

void ScribaMonitorEditor::showPage (int index)
{
    currentPage = measurePage != nullptr ? index : 1;

    if (measurePage != nullptr)
        measurePage->setVisible (currentPage == 0);

    correctionPage.setVisible (currentPage == 1);
    measureTab.setToggleState (currentPage == 0, juce::dontSendNotification);
    correctionTab.setToggleState (currentPage == 1, juce::dontSendNotification);
    repaint();
}

void ScribaMonitorEditor::drawLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Segnaposto del logo Scriba: cerchio con monogramma.
    // Da sostituire con il logo originale (PNG/SVG) quando disponibile.
    g.setColour (text.withAlpha (0.9f));
    g.drawEllipse (r.reduced (1.0f), 2.0f);

    juce::Path m;
    const auto b = r.reduced (r.getWidth() * 0.28f);
    m.startNewSubPath (b.getX(), b.getBottom());
    m.lineTo (b.getX() + b.getWidth() * 0.12f, b.getY());
    m.lineTo (b.getCentreX(), b.getBottom() - b.getHeight() * 0.25f);
    m.lineTo (b.getRight() - b.getWidth() * 0.12f, b.getY());
    m.lineTo (b.getRight(), b.getBottom());
    g.strokePath (m, juce::PathStrokeType (3.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // penna, richiamo alla "scrittura" di Scriba
    g.setColour (accent);
    g.drawLine (b.getRight() + 2.0f, b.getY() - 4.0f, b.getCentreX() + 2.0f, b.getBottom() - 4.0f, 2.0f);
}

void ScribaMonitorEditor::paint (juce::Graphics& g)
{
    ScribaLookAndFeel::fillBackground (g, getLocalBounds());

    // --- intestazione ---
    auto head = headerArea.reduced (28, 14);
    drawLogo (g, head.removeFromLeft (head.getHeight()).toFloat());
    head.removeFromLeft (22);

    auto titleRow = head.removeFromTop ((int) (head.getHeight() * 0.62f));
    const auto titleFont = ScribaLookAndFeel::font (34.0f, true).withExtraKerningFactor (0.22f);
    g.setFont (titleFont);
    g.setColour (text);
    const juce::String scriba ("SCRIBA");
    g.drawText (scriba, titleRow, juce::Justification::bottomLeft);
    titleRow.removeFromLeft (juce::GlyphArrangement::getStringWidthInt (titleFont, scriba) + 18);
    g.setColour (accent);
    g.drawText ("MONITOR", titleRow, juce::Justification::bottomLeft);

    g.setColour (textMuted);
    g.setFont (ScribaLookAndFeel::font (12.5f).withExtraKerningFactor (0.3f));
    g.drawText (juce::String::fromUTF8 ("ROOM \xc2\xb7 MEASURE \xc2\xb7 CORRECTION"),
                head.withTrimmedTop (4), juce::Justification::topLeft);

    // pastiglia di stato
    juce::String pillText;
    const auto& profile = processor.getProfile();
    if (processor.isStandalone() && currentPage == 0)
        pillText = juce::String (processor.getSession().getNumPointsDone()) + " / "
                 + juce::String (processor.getSession().getNumPoints()) + " PUNTI";
    else
        pillText = profile.isValid() ? profile.name.toUpperCase() : "NESSUN PROFILO";

    ScribaLookAndFeel::drawPill (g, pillArea.toFloat(), pillText);

    // --- piè di pagina ---
    auto foot = footerArea.reduced (28, 0);
    g.setFont (ScribaLookAndFeel::font (13.0f));
    g.setColour (textMuted);
    g.drawText (juce::String::fromUTF8 ("\xc2\xa9 2026 Meteora Plugins - Scriba Monitor - v") + JucePlugin_VersionString,
                foot, juce::Justification::centredRight);

    const juce::String hint = currentPage == 0
        ? juce::String ("Microfono al punto d'ascolto, puntato in avanti. Poi Check livello e Misura.")
        : juce::String ("Plugin sul master: mettilo in bypass prima del bounce.");
    g.setColour (textSoft.withAlpha (0.85f));
    g.drawText (hint, foot, juce::Justification::centredLeft);
}

void ScribaMonitorEditor::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (headerHeight);
    footerArea = area.removeFromBottom (footerHeight);

    pillArea = headerArea.withTrimmedRight (28).removeFromRight (200).withSizeKeepingCentre (200, 52);

    if (measurePage != nullptr)
    {
        auto tabs = juce::Rectangle<int> (pillArea.getX() - 20 - 280, pillArea.getY() + 4, 280, 44);
        measureTab.setBounds (tabs.removeFromLeft (138));
        tabs.removeFromLeft (4);
        correctionTab.setBounds (tabs);
        measurePage->setBounds (area);
    }

    correctionPage.setBounds (area);
}
