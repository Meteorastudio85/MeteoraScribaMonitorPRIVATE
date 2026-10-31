#include "CorrectionPage.h"
#include "ScribaLookAndFeel.h"
#include "../PluginProcessor.h"

using namespace ScribaColours;

static scriba::Response meanDb (const scriba::Response& a, const scriba::Response& b)
{
    scriba::Response r;
    for (size_t i = 0; i < r.db.size(); ++i)
        r.db[i] = 0.5f * (a.db[i] + b.db[i]);
    return r;
}

CorrectionPage::CorrectionPage (ScribaMonitorProcessor& p)
    : processor (p), state (p.getState())
{
    // --- profilo ---
    profileBox.setTextWhenNothingSelected ("Nessun profilo: misura con lo standalone");
    profileBox.onChange = [this]
    {
        const int index = profileBox.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, profileFiles.size()))
        {
            auto profile = scriba::Profile::loadFrom (profileFiles[index]);
            if (profile.isValid())
                processor.loadProfile (profile);
        }
    };
    addAndMakeVisible (profileBox);

    loadProfileButton.setButtonText (juce::String::fromUTF8 ("Carica\xe2\x80\xa6"));
    loadProfileButton.onClick = [this] { chooseProfileFile(); };
    addAndMakeVisible (loadProfileButton);

    bypassButton.getProperties().set ("primary", true);
    bypassButton.onClick = [this]
    {
        if (auto* param = state.getParameter (ParamIDs::bypass))
            param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        syncButtons();
    };
    addAndMakeVisible (bypassButton);

    // --- vista canali ---
    for (auto name : { "L+R", "L", "R" })
    {
        auto* b = viewButtons.add (new juce::TextButton (name));
        b->onClick = [this, b]
        {
            const int index = viewButtons.indexOf (b);
            viewChannel = index == 0 ? 2 : index - 1;
            syncButtons();
            updateGraph();
        };
        addAndMakeVisible (b);
    }

    // --- target ---
    const auto targetNames = ScribaMonitorProcessor::getTargetNames();
    for (int i = 0; i < targetNames.size(); ++i)
    {
        auto* b = targetButtons.add (new juce::TextButton (i == 2 ? targetNames[i] + juce::String::fromUTF8 ("\xe2\x80\xa6") : targetNames[i]));
        b->onClick = [this, i]
        {
            if (i == 2)
                chooseCustomTarget();
            else
                setChoiceParam (ParamIDs::target, i);
        };
        addAndMakeVisible (b);
    }

    // --- manopole ---
    const std::pair<const char*, const char*> knobDefs[] =
    {
        { ParamIDs::tilt,   "Tilt dB/oct" },
        { ParamIDs::bass,   "Bassi dB" },
        { ParamIDs::treble, "Acuti dB" },
        { ParamIDs::mix,    "Dry / Wet" },
        { ParamIDs::output, "Output dB" },
    };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.title = knobDefs[i].second;
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (k.slider);
        k.attachment = std::make_unique<SliderAttachment> (state, knobDefs[i].first, k.slider);
    }

    knobs[3].slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + "%"; };
    for (size_t i : { (size_t) 0, (size_t) 1, (size_t) 2, (size_t) 4 })
        knobs[i].slider.textFromValueFunction = [] (double v) { return (v > 0.04 ? "+" : "") + juce::String (v, 1); };

    // --- filtro ---
    for (auto name : ScribaMonitorProcessor::getPhaseNames())
    {
        auto* b = phaseButtons.add (new juce::TextButton (name));
        const int index = phaseButtons.size() - 1;
        b->onClick = [this, index] { setChoiceParam (ParamIDs::phase, index); };
        addAndMakeVisible (b);
    }

    maxBoost.setSliderStyle (juce::Slider::LinearHorizontal);
    maxBoost.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    maxBoost.setTextValueSuffix (" dB");
    maxBoost.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (maxBoost);
    maxBoostAttachment = std::make_unique<SliderAttachment> (state, ParamIDs::maxBoost, maxBoost);

    rangeBox.addItemList (ScribaMonitorProcessor::getRangeNames(), 1);
    addAndMakeVisible (rangeBox);
    rangeAttachment = std::make_unique<ComboAttachment> (state, ParamIDs::range, rangeBox);

    // --- emulazioni ---
    for (auto& e : scriba::getEmulations())
    {
        auto* b = emulationButtons.add (new juce::TextButton (e.name));
        const int index = emulationButtons.size() - 1;
        b->onClick = [this, index] { setChoiceParam (ParamIDs::emulation, index); };
        addAndMakeVisible (b);
    }

    compareButton.onClick = [this]
    {
        if (auto* param = state.getParameter (ParamIDs::emulationOn))
            param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        syncButtons();
    };
    addAndMakeVisible (compareButton);

    addAndMakeVisible (graph);
    graph.setDbRange (12.0f);

    refreshProfileList();
    syncButtons();
    updateGraph();
    startTimerHz (15);
}

CorrectionPage::~CorrectionPage() = default;

// ---------------------------------------------------------------------------

void CorrectionPage::refreshProfileList()
{
    profileFiles = scriba::Profile::getProfilesFolder().findChildFiles (juce::File::findFiles, false,
                                                                        juce::String ("*") + scriba::Profile::fileExtension);
    std::sort (profileFiles.begin(), profileFiles.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() > b.getLastModificationTime(); });

    profileBox.clear (juce::dontSendNotification);

    const auto& current = processor.getProfile();
    int selected = 0;

    for (int i = 0; i < profileFiles.size(); ++i)
    {
        auto p = scriba::Profile::loadFrom (profileFiles[i]);
        if (! p.isValid())
            continue;

        profileBox.addItem (p.getDisplayName(), i + 1);
        if (current.isValid() && p.name == current.name && p.created == current.created)
            selected = i + 1;
    }

    if (selected > 0)
        profileBox.setSelectedId (selected, juce::dontSendNotification);
    else if (current.isValid())
        profileBox.setText (current.getDisplayName(), juce::dontSendNotification);
}

void CorrectionPage::chooseProfileFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Carica profilo", scriba::Profile::getProfilesFolder(),
                                                   juce::String ("*") + scriba::Profile::fileExtension);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto profile = scriba::Profile::loadFrom (fc.getResult());
        if (profile.isValid())
        {
            processor.loadProfile (profile);
            refreshProfileList();
        }
        else if (fc.getResult().existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Profilo",
                                                    juce::String::fromUTF8 ("Il file non \xc3\xa8 un profilo di Scriba Monitor valido."));
        }
    });
}

void CorrectionPage::chooseCustomTarget()
{
    chooser = std::make_unique<juce::FileChooser> ("Curva target personalizzata (frequenza  dB)", juce::File(), "*.txt;*.csv;*.frd");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (! fc.getResult().existsAsFile())
            return;

        if (processor.loadCustomTarget (fc.getResult()))
            setChoiceParam (ParamIDs::target, 2);
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Target",
                                                    "Il file non contiene una curva leggibile (righe: frequenza  dB).");
    });
}

void CorrectionPage::setChoiceParam (const char* id, int index)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        param->endChangeGesture();
    }
    syncButtons();
}

int CorrectionPage::getChoiceParam (const char* id) const
{
    return juce::roundToInt (state.getRawParameterValue (id)->load());
}

void CorrectionPage::syncButtons()
{
    const bool bypassed = state.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    bypassButton.setButtonText (bypassed ? "BYPASS" : "ATTIVO");
    bypassButton.getProperties().set ("primary", ! bypassed);
    bypassButton.repaint();

    for (int i = 0; i < viewButtons.size(); ++i)
        viewButtons[i]->setToggleState (i == (viewChannel == 2 ? 0 : viewChannel + 1), juce::dontSendNotification);

    const int target = getChoiceParam (ParamIDs::target);
    for (int i = 0; i < targetButtons.size(); ++i)
        targetButtons[i]->setToggleState (i == target, juce::dontSendNotification);

    const int phase = getChoiceParam (ParamIDs::phase);
    for (int i = 0; i < phaseButtons.size(); ++i)
        phaseButtons[i]->setToggleState (i == phase, juce::dontSendNotification);

    const int emulation = getChoiceParam (ParamIDs::emulation);
    for (int i = 0; i < emulationButtons.size(); ++i)
        emulationButtons[i]->setToggleState (i == emulation, juce::dontSendNotification);

    const bool emulationOn = state.getRawParameterValue (ParamIDs::emulationOn)->load() > 0.5f;
    compareButton.setToggleState (! emulationOn && emulation > 0, juce::dontSendNotification);
    compareButton.setEnabled (emulation > 0);
}

void CorrectionPage::timerCallback()
{
    syncButtons();

    if (processor.getDesignVersion() != lastDesignVersion)
    {
        lastDesignVersion = processor.getDesignVersion();
        updateGraph();
        repaint (filterInfoArea);
    }
}

void CorrectionPage::updateGraph()
{
    if (! processor.getProfile().isValid())
    {
        graph.setCurves ({});
        graph.setEmptyMessage ("Nessun profilo caricato.\nMisura la stanza con Scriba Monitor standalone, poi scegli il profilo qui sopra.");
        graph.setHighlightBelow (0.0, {});
        return;
    }

    const auto& l = processor.getDesign (0);
    const auto& r = processor.getDesign (1);

    auto pick = [&] (const scriba::Response& a, const scriba::Response& b)
    {
        return viewChannel == 0 ? a : viewChannel == 1 ? b : meanDb (a, b);
    };

    graph.setCurves ({
        { pick (l.target, r.target),       textMuted, 1.5f, true },
        { pick (l.measured, r.measured),   textDim,   1.5f, false },
        { pick (l.filter, r.filter),       left,      1.5f, false },
        { pick (l.predicted, r.predicted), accent,    2.5f, false },
    });

    const int range = getChoiceParam (ParamIDs::range);
    if (range == 0)       graph.setHighlightBelow (300.0, "correzione fino a 300 Hz");
    else if (range == 1)  graph.setHighlightBelow (1000.0, "correzione fino a 1 kHz");
    else                  graph.setHighlightBelow (0.0, {});
}

// ---------------------------------------------------------------------------

void CorrectionPage::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    topBar = area.removeFromTop (36);
    {
        auto bar = topBar;
        bypassButton.setBounds (bar.removeFromRight (96));
        bar.removeFromRight (16);
        loadProfileButton.setBounds (bar.removeFromRight (84));
        bar.removeFromRight (8);
        bar.removeFromLeft (80);                            // etichetta "Profilo"
        profileBox.setBounds (bar.removeFromLeft (juce::jmin (340, bar.getWidth())));
    }
    area.removeFromTop (12);

    emulationPanel = area.removeFromBottom (122);
    area.removeFromBottom (12);

    auto controls = area.removeFromBottom (172);
    area.removeFromBottom (12);
    graphPanel = area;

    targetPanel = controls.removeFromLeft (180);
    controls.removeFromLeft (12);
    filterPanel = controls.removeFromRight (230);
    controls.removeFromRight (12);
    knobsPanel = controls;

    // grafico
    {
        auto inner = graphPanel.reduced (14, 10);
        auto header = inner.removeFromTop (28);
        auto views = header.removeFromLeft (150);
        for (auto* b : viewButtons)
            b->setBounds (views.removeFromLeft (50).reduced (2, 0));
        legendArea = header;
        inner.removeFromTop (4);
        graph.setBounds (inner);
    }

    // target
    {
        auto inner = targetPanel.reduced (14, 12);
        inner.removeFromTop (22);
        for (auto* b : targetButtons)
        {
            b->setBounds (inner.removeFromTop (34));
            inner.removeFromTop (6);
        }
    }

    // manopole
    {
        auto inner = knobsPanel.reduced (14, 12);
        inner.removeFromTop (22);
        const int w = inner.getWidth() / (int) knobs.size();
        for (auto& k : knobs)
        {
            auto cell = inner.removeFromLeft (w);
            cell.removeFromBottom (20);                     // titolo
            k.slider.setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 84), juce::jmin (cell.getHeight(), 84)));
        }
    }

    // filtro
    {
        auto inner = filterPanel.reduced (14, 12);
        inner.removeFromTop (22);
        auto phases = inner.removeFromTop (30);
        const int pw = phases.getWidth() / 2;
        phaseButtons[0]->setBounds (phases.removeFromLeft (pw).withTrimmedRight (3));
        phaseButtons[1]->setBounds (phases.withTrimmedLeft (3));
        inner.removeFromTop (8);
        rangeBox.setBounds (inner.removeFromTop (30));
        inner.removeFromTop (6);
        auto boostRow = inner.removeFromTop (24);
        boostRow.removeFromLeft (72);                       // etichetta "Boost max"
        maxBoost.setBounds (boostRow);
        inner.removeFromTop (4);
        filterInfoArea = inner;
    }

    // emulazioni
    {
        auto inner = emulationPanel.reduced (14, 10);
        auto header = inner.removeFromTop (24);
        compareButton.setBounds (header.removeFromRight (230));
        inner.removeFromTop (8);

        // due righe: 5 + 4 pulsanti
        const int perRow = (emulationButtons.size() + 1) / 2;
        const int gap = 6;
        const int w = (inner.getWidth() - gap * (perRow - 1)) / perRow;
        for (int i = 0; i < emulationButtons.size(); ++i)
        {
            const int row = i / perRow, col = i % perRow;
            emulationButtons[i]->setBounds (inner.getX() + col * (w + gap), inner.getY() + row * 38, w, 32);
        }
    }
}

void CorrectionPage::paint (juce::Graphics& g)
{

    g.setColour (textMuted);
    g.setFont (ScribaLookAndFeel::caps (12.0f));
    g.drawText ("PROFILO", topBar.withWidth (80), juce::Justification::centredLeft);

    ScribaLookAndFeel::drawPanel (g, graphPanel, {});
    ScribaLookAndFeel::drawPanel (g, targetPanel, "TARGET");
    ScribaLookAndFeel::drawPanel (g, knobsPanel, "REGOLAZIONI");
    ScribaLookAndFeel::drawPanel (g, filterPanel, "FILTRO");
    ScribaLookAndFeel::drawPanel (g, emulationPanel, "EMULAZIONE MONITOR");

    // titoli manopole
    g.setFont (ScribaLookAndFeel::caps (10.5f));
    g.setColour (textMuted);
    for (auto& k : knobs)
        g.drawText (k.title.toUpperCase(), k.slider.getBounds().withY (k.slider.getBottom()).withHeight (20).expanded (12, 0),
                    juce::Justification::centred);

    // legenda
    {
        auto legend = legendArea;
        auto item = [&g, &legend] (const juce::String& name, juce::Colour c, bool dashed)
        {
            auto r = legend.removeFromRight (juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 26);
            g.setColour (textMuted);
            g.drawText (name, r.withTrimmedLeft (20), juce::Justification::centredLeft);
            g.setColour (c);
            if (dashed) { g.fillRect (r.getX(), r.getCentreY() - 1, 5, 2); g.fillRect (r.getX() + 9, r.getCentreY() - 1, 5, 2); }
            else          g.fillRect (r.getX(), r.getCentreY() - 1, 14, 2);
            legend.removeFromRight (6);
        };
        item ("Target", textMuted, true);
        item ("Dopo", accent, false);
        item ("Filtro", left, false);
        item ("Prima", textDim, false);
    }

    // info filtro
    g.setColour (textMuted);
    g.setFont (ScribaLookAndFeel::font (12.0f));
    g.drawText ("Boost max", maxBoost.getBounds().withX (filterPanel.getX() + 14).withWidth (72), juce::Justification::centredLeft);

    {
        auto info = filterInfoArea;
        auto row = [&g, &info] (const juce::String& name, const juce::String& value)
        {
            auto r = info.removeFromTop (18);
            g.setFont (ScribaLookAndFeel::font (12.0f));
            g.setColour (textMuted);
            g.drawText (name, r, juce::Justification::centredLeft);
            g.setFont (ScribaLookAndFeel::mono (12.0f));
            g.setColour (text);
            g.drawText (value, r, juce::Justification::centredRight);
        };

        row ("Latenza", juce::String (processor.getLatencySamples()) + " smp");
        if (processor.getProfile().isValid())
            row ("Bilanciamento L / R", juce::String (processor.getBalanceDb (0), 1) + " / " + juce::String (processor.getBalanceDb (1), 1));
    }
}
