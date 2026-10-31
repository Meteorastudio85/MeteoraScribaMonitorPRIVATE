#include "MeasurePage.h"
#include "ScribaLookAndFeel.h"
#include "../PluginProcessor.h"
#include "../dsp/FilterDesigner.h"

using namespace ScribaColours;
using State = MeasurementSession::State;

// Posizioni dei punti di misura (unità: raggio del cerchio d'ascolto). Il punto 0 è il centro.
static std::vector<juce::Point<float>> pointLayout (int numPoints)
{
    if (numPoints <= 1)
        return { { 0.0f, 0.0f } };

    std::vector<juce::Point<float>> pts { { 0.0f, 0.0f } };
    const int around = numPoints - 1;
    for (int i = 0; i < around; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) around;
        pts.push_back ({ std::cos (a), std::sin (a) });
    }
    return pts;
}

// ===========================================================================
class MeasurePage::PositionMap : public juce::Component
{
public:
    explicit PositionMap (MeasurementSession& s) : session (s) {}

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (background.withAlpha (0.55f));
        g.fillRoundedRectangle (b.reduced (1.0f), 12.0f);
        g.setColour (border);
        g.drawRoundedRectangle (b.reduced (1.0f), 12.0f, 1.0f);

        const float cx = b.getCentreX();
        const auto listener = getListener();
        const float spread = juce::jmin (b.getWidth() * 0.22f, 110.0f);

        // monitor
        for (int side = 0; side < 2; ++side)
        {
            const float x = cx + (side == 0 ? -spread : spread);
            const float y = b.getY() + 30.0f;
            const float angle = side == 0 ? 0.35f : -0.35f;

            g.setColour (control.withAlpha (0.8f));
            g.drawLine (x, y, listener.x, listener.y, 1.0f);

            juce::Path box;
            box.addRoundedRectangle (-16.0f, -14.0f, 32.0f, 28.0f, 6.0f);
            box.applyTransform (juce::AffineTransform::rotation (angle).translated (x, y));
            g.setColour (chip);
            g.fillPath (box);
            g.setColour (controlHi);
            g.strokePath (box, juce::PathStrokeType (1.0f));
            g.setColour (textSoft);
            g.drawEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f, 1.5f);
            g.setFont (ScribaLookAndFeel::caps (11.0f));
            g.drawText (side == 0 ? "L" : "R", juce::Rectangle<float> (x - 10.0f, y + 18.0f, 20.0f, 14.0f), juce::Justification::centred);
        }

        // cerchio d'ascolto e punti
        const float r = getRadius();
        g.setColour (control);
        g.drawEllipse (listener.x - r, listener.y - r, r * 2.0f, r * 2.0f, 1.0f);

        const auto& points = session.getPoints();
        const auto layout = pointLayout ((int) points.size());

        for (int i = 0; i < (int) layout.size(); ++i)
        {
            const auto p = listener + layout[(size_t) i] * r;
            const bool current = i == session.getCurrentPoint();

            if (points[(size_t) i].done)
            {
                g.setColour (good);
                g.fillEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f);
            }
            else if (! current)
            {
                g.setColour (textDim);
                g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
            }

            if (current)
            {
                g.setColour (text);
                g.drawEllipse (p.x - 10.0f, p.y - 10.0f, 20.0f, 20.0f, 2.0f);
                g.setColour (accent);
                if (! points[(size_t) i].done)
                    g.fillEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto layout = pointLayout ((int) session.getPoints().size());
        const auto listener = getListener();

        for (int i = 0; i < (int) layout.size(); ++i)
            if (e.position.getDistanceFrom (listener + layout[(size_t) i] * getRadius()) < 12.0f)
                session.selectPoint (i);
    }

private:
    juce::Point<float> getListener() const
    {
        const auto b = getLocalBounds().toFloat();
        return { b.getCentreX(), b.getBottom() - getRadius() - 14.0f };
    }

    float getRadius() const     { return juce::jlimit (24.0f, 46.0f, (float) getHeight() * 0.22f); }

    MeasurementSession& session;
};

// ===========================================================================
class MeasurePage::ProgressGrid : public juce::Component
{
public:
    explicit ProgressGrid (MeasurementSession& s) : session (s) {}

    void paint (juce::Graphics& g) override
    {
        const auto& points = session.getPoints();
        const int n = (int) points.size();
        const int cols = n >= 5 ? 3 : n;
        const int rows = (n + cols - 1) / cols;

        auto area = getLocalBounds();
        auto gridArea = area.removeFromTop (rows * 44 + (rows - 1) * 8);
        const int cellW = (gridArea.getWidth() - (cols - 1) * 8) / cols;

        const auto st = session.getState();
        const bool measuring = st == State::measuringLeft || st == State::measuringRight || st == State::analysing;

        for (int i = 0; i < n; ++i)
        {
            const int row = i / cols, col = i % cols;
            const auto cell = juce::Rectangle<int> (gridArea.getX() + col * (cellW + 8), gridArea.getY() + row * 52, cellW, 44).toFloat();
            const bool current = i == session.getCurrentPoint();
            const bool done = points[(size_t) i].done;

            g.setColour (done ? goodBg : chip);
            g.fillRoundedRectangle (cell, 10.0f);

            if (current && measuring)
            {
                // avanzamento: L nella prima metà, R nella seconda
                float progress = session.getProgress() * 0.5f;
                if (st == State::measuringRight) progress += 0.5f;
                if (st == State::analysing)      progress = 1.0f;
                g.setColour (accent.withAlpha (0.3f));
                g.fillRoundedRectangle (cell.withWidth (cell.getWidth() * progress), 10.0f);
            }

            g.setColour (current ? text : done ? accent : border);
            g.drawRoundedRectangle (cell.reduced (1.0f), 10.0f, current || done ? 2.0f : 1.0f);
            g.setColour (done ? accent : current ? text : textMuted);
            g.setFont (ScribaLookAndFeel::font (15.0f, true));
            g.drawText (juce::String (i + 1), cell, juce::Justification::centred);
        }

        area.removeFromTop (10);
        g.setFont (ScribaLookAndFeel::font (12.0f));

        juce::String info;
        juce::Colour colour = textMuted;

        switch (st)
        {
            case State::measuringLeft:  info = "In misura: monitor sinistro";  colour = text; break;
            case State::measuringRight: info = "In misura: monitor destro";    colour = text; break;
            case State::analysing:      info = "Analisi in corso...";          colour = text; break;
            case State::levelCheck:     info = "Check del livello in corso..."; colour = text; break;
            case State::idle:
                if (session.getLastError().isNotEmpty()) { info = session.getLastError(); colour = bad; }
                else info = juce::String (session.getNumPointsDone()) + " di " + juce::String (n) + " punti misurati";
                break;
        }

        g.setColour (colour);
        g.drawFittedText (info, area, juce::Justification::topLeft, 3);
    }

private:
    MeasurementSession& session;
};

// ===========================================================================
class MeasurePage::LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (MeasurementSession& s) : session (s) {}

    void setLivePeak (float db)     { livePeak = db; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        auto meterArea = area.removeFromLeft (64).withTrimmedBottom (96).withTrimmedTop (8);
        auto bar = meterArea.removeFromLeft (24).toFloat();

        auto yFor = [&bar] (float db) { return bar.getY() + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * bar.getHeight(); };

        g.setColour (background);
        g.fillRoundedRectangle (bar, 6.0f);
        g.setColour (border);
        g.drawRoundedRectangle (bar, 6.0f, 1.0f);

        // zona ottimale −12 … −6 dBFS
        g.setColour (good.withAlpha (0.18f));
        g.fillRect (bar.getX() + 1.0f, yFor (-6.0f), bar.getWidth() - 2.0f, yFor (-12.0f) - yFor (-6.0f));

        const float y = yFor (livePeak);
        g.setColour (livePeak > -3.0f ? bad : livePeak > -12.0f ? good : left);
        g.fillRoundedRectangle (bar.getX() + 4.0f, y, bar.getWidth() - 8.0f, juce::jmax (0.0f, bar.getBottom() - 3.0f - y), 4.0f);

        auto mark = [&] (float db, juce::Colour c, float thickness)
        {
            g.setColour (c);
            g.fillRect (bar.getX() - 4.0f, yFor (db) - thickness * 0.5f, bar.getWidth() + 8.0f, thickness);
        };
        mark (-3.0f, bad, 2.0f);
        mark (-6.0f, good, 1.0f);
        mark (-12.0f, good, 1.0f);

        g.setFont (ScribaLookAndFeel::mono (10.0f));
        for (float db : { 0.0f, -3.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f })
        {
            g.setColour (juce::approximatelyEqual (db, -3.0f) ? bad : textMuted);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (bar.getRight() + 6.0f, yFor (db) - 6.0f, 34.0f, 12.0f),
                        juce::Justification::centredLeft);
        }

        // letture
        const auto& r = session.getLevelResult();
        auto readouts = area.withTrimmedLeft (6).withTrimmedBottom (90);
        auto drawReadout = [&] (const juce::String& title, const juce::String& value, const juce::String& unit)
        {
            auto block = readouts.removeFromTop (58);
            g.setColour (textMuted);
            g.setFont (ScribaLookAndFeel::caps (10.5f));
            g.drawText (title.toUpperCase(), block.removeFromTop (14), juce::Justification::centredLeft);
            g.setColour (text);
            g.setFont (ScribaLookAndFeel::font (22.0f, true));
            g.drawText (value, block.removeFromTop (24), juce::Justification::centredLeft);
            g.setColour (textMuted);
            g.setFont (ScribaLookAndFeel::font (11.0f));
            g.drawText (unit, block.removeFromTop (14), juce::Justification::centredLeft);
        };

        readouts.removeFromTop (10);
        drawReadout ("Picco", r.status == MeasurementSession::LevelStatus::notChecked ? "--" : juce::String (r.peakDbfs, 1), "dBFS");
        drawReadout ("SNR", r.status == MeasurementSession::LevelStatus::notChecked ? "--" : juce::String (juce::roundToInt (r.snrDb)), "dB");

        // stato
        auto statusBox = getLocalBounds().removeFromBottom (82).toFloat();
        const bool ok = r.allowed;
        const bool neutral = r.status == MeasurementSession::LevelStatus::notChecked;
        g.setColour (neutral ? chip : ok ? goodBg : badBg);
        g.fillRoundedRectangle (statusBox, 12.0f);
        g.setColour (neutral ? border : ok ? accent : bad.withAlpha (0.6f));
        g.drawRoundedRectangle (statusBox.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (neutral ? textMuted : ok ? goodText : bad);
        g.setFont (ScribaLookAndFeel::font (13.0f, true));
        g.drawFittedText (r.message, statusBox.reduced (10.0f, 6.0f).toNearestInt(), juce::Justification::centredLeft, 5);
    }

private:
    MeasurementSession& session;
    float livePeak = -120.0f;
};

// ===========================================================================
class MeasurePage::StepsBar : public juce::Component
{
public:
    void setState (int completedUpTo, int current)  { completed = completedUpTo; active = current; repaint(); }

    void paint (juce::Graphics& g) override
    {
        static const char* names[] = { "1 \xc2\xb7 Setup", "2 \xc2\xb7 Livello", "3 \xc2\xb7 Misura",
                                       "4 \xc2\xb7 Filtro", "5 \xc2\xb7 Salva" };
        auto area = getLocalBounds().toFloat();
        g.setFont (ScribaLookAndFeel::caps (11.0f));

        float x = area.getX();
        for (int i = 0; i < 5; ++i)
        {
            const juce::String name = juce::String::fromUTF8 (names[i]).toUpperCase();
            const float w = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 22.0f;
            const auto chip = juce::Rectangle<float> (x, area.getY(), w, area.getHeight());
            const bool done = i < completed;
            const bool current = i == active;

            g.setColour (done ? goodBg : ScribaColours::chip);
            g.fillRoundedRectangle (chip, 10.0f);
            g.setColour (current ? text : done ? accent.withAlpha (0.6f) : border);
            g.drawRoundedRectangle (chip.reduced (1.0f), 10.0f, current ? 2.0f : 1.0f);

            g.setColour (done ? goodText : current ? text : textMuted);
            g.drawText (name, chip, juce::Justification::centred);
            x += w;

            if (i < 4)
            {
                g.setColour (controlHi);
                g.drawText (juce::String::fromUTF8 ("\xe2\x80\x94"), juce::Rectangle<float> (x, area.getY(), 22.0f, area.getHeight()), juce::Justification::centred);
                x += 22.0f;
            }
        }
    }

private:
    int completed = 1, active = 1;
};

// ===========================================================================
MeasurePage::MeasurePage (ScribaMonitorProcessor& p)
    : processor (p), session (p.getSession())
{
    positionMap  = std::make_unique<PositionMap> (session);
    progressGrid = std::make_unique<ProgressGrid> (session);
    levelMeter   = std::make_unique<LevelMeter> (session);
    stepsBar     = std::make_unique<StepsBar>();

    for (auto* c : std::initializer_list<juce::Component*> { positionMap.get(), progressGrid.get(), levelMeter.get(),
                                                             stepsBar.get(), &graph })
        addAndMakeVisible (c);

    auto addField = [this] (juce::Component& comp, const juce::String& title)
    {
        addAndMakeVisible (comp);
        auto* label = new juce::Label ({}, title.toUpperCase());
        label->setFont (ScribaLookAndFeel::caps (10.5f));
        label->setColour (juce::Label::textColourId, textMuted);
        label->setBorderSize ({ 0, 0, 0, 0 });
        label->attachToComponent (&comp, false);
        ownedLabels.add (label);
    };

    micBox.addItemList ({ "Input 1", "Input 2" }, 1);
    micBox.setSelectedItemIndex (session.getMicChannel(), juce::dontSendNotification);
    micBox.onChange = [this] { session.setMicChannel (micBox.getSelectedItemIndex()); };
    addField (micBox, "Ingresso microfono");

    calBox.addItem ("Behringer ECM8000 (default)", 1);
    calBox.addItem (juce::String::fromUTF8 ("File personalizzato\xe2\x80\xa6"), 2);
    calBox.setSelectedId (1, juce::dontSendNotification);
    calBox.onChange = [this]
    {
        if (calBox.getSelectedId() == 1)
        {
            session.setCalibration (scriba::MicCalibration::defaultECM8000());
            calibrationFileName = "ECM8000_generic.cal";
            repaint();
        }
        else
        {
            chooseCalibrationFile();
        }
    };
    addField (calBox, "Calibrazione microfono");

    outputLevel.setSliderStyle (juce::Slider::LinearHorizontal);
    outputLevel.setRange (-40.0, -6.0, 0.5);
    outputLevel.setValue (session.getOutputLevelDb(), juce::dontSendNotification);
    outputLevel.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    outputLevel.setTextValueSuffix (" dBFS");
    outputLevel.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    outputLevel.onValueChange = [this] { session.setOutputLevelDb ((float) outputLevel.getValue()); };
    addField (outputLevel, "Livello di uscita (sweep)");

    pointsBox.addItem ("1 punto (veloce)", 1);
    pointsBox.addItem ("5 punti", 5);
    pointsBox.addItem ("9 punti (consigliato)", 9);
    pointsBox.setSelectedId (session.getNumPoints(), juce::dontSendNotification);
    pointsBox.onChange = [this] { session.setNumPoints (pointsBox.getSelectedId()); };
    addField (pointsBox, "Punti di misura");

    profileName.setText ("Studio A");
    profileName.setFont (ScribaLookAndFeel::font (14.0f, true));
    profileName.setIndents (8, 7);
    addField (profileName, "Nome profilo");

    checkButton.onClick = [this] { session.startLevelCheck(); };

    measureButton.getProperties().set ("primary", true);
    measureButton.onClick = [this]
    {
        if (session.isBusy()) session.cancel();
        else                  session.measureCurrentPoint();
    };

    resetButton.onClick = [this]
    {
        session.resetPoints();
        profileSaved = false;
        refresh();
    };

    saveButton.getProperties().set ("primary", true);
    saveButton.onClick = [this] { saveProfile(); };

    for (auto* b : { &checkButton, &measureButton, &resetButton, &saveButton })
        addAndMakeVisible (b);

    graph.setDbRange (15.0f);
    graph.setEmptyMessage ("Le misure appariranno qui");

    session.onChange = [this] { refresh(); };
    refresh();
    startTimerHz (30);
}

MeasurePage::~MeasurePage()
{
    session.onChange = nullptr;
}

void MeasurePage::chooseCalibrationFile()
{
    chooser = std::make_unique<juce::FileChooser> ("File di calibrazione del microfono", juce::File(), "*.cal;*.txt;*.frd");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        auto cal = scriba::MicCalibration::fromFile (file);

        if (file.existsAsFile() && cal.isValid())
        {
            session.setCalibration (cal);
            calibrationFileName = file.getFileName();
        }
        else
        {
            if (file.existsAsFile())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Calibrazione",
                                                        "Il file non contiene una curva di calibrazione leggibile.");
            calBox.setSelectedId (1, juce::dontSendNotification);
            session.setCalibration (scriba::MicCalibration::defaultECM8000());
            calibrationFileName = "ECM8000_generic.cal";
        }
        repaint();
    });
}

void MeasurePage::saveProfile()
{
    auto name = profileName.getText().trim();
    if (name.isEmpty())
        name = "Profilo";

    const auto profile = session.buildProfile (name);
    const auto file = scriba::Profile::getProfilesFolder()
                          .getChildFile (juce::File::createLegalFileName (name) + scriba::Profile::fileExtension);

    if (! profile.saveTo (file))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Salvataggio",
                                                "Impossibile salvare il profilo in:\n" + file.getFullPathName());
        return;
    }

    processor.loadProfile (profile);
    profileSaved = true;
    refresh();

    if (onProfileSaved)
        onProfileSaved();
}

void MeasurePage::refresh()
{
    const bool busy = session.isBusy();
    const auto& level = session.getLevelResult();
    const bool allDone = session.allPointsDone();

    measureButton.setButtonText (busy ? "Annulla"
                                      : "Misura punto " + juce::String (session.getCurrentPoint() + 1));
    measureButton.setEnabled (busy ? session.getState() != State::analysing
                                   : level.allowed && ! (allDone && profileSaved));
    checkButton.setEnabled (! busy);
    resetButton.setEnabled (! busy && session.getNumPointsDone() > 0);
    saveButton.setEnabled (! busy && allDone);
    saveButton.setVisible (allDone);
    measureButton.setVisible (! allDone || busy);

    for (auto* c : std::initializer_list<juce::Component*> { &micBox, &calBox, &outputLevel, &pointsBox })
        c->setEnabled (! busy);

    // grafico: media dei punti misurati
    std::vector<ResponseGraph::Curve> curves;
    if (session.getNumPointsDone() > 0)
    {
        curves.push_back ({ scriba::Response(), textMuted, 1.5f, true });
        curves.push_back ({ scriba::prepareMeasurement (session.getAverage (false)), left, 2.0f, false });
        curves.push_back ({ scriba::prepareMeasurement (session.getAverage (true)),  right, 2.0f, false });
    }
    graph.setCurves (std::move (curves));

    // passaggi
    int completed = 1;
    if (level.allowed) completed = 2;
    if (level.allowed && allDone) completed = 4;
    if (completed == 4 && profileSaved) completed = 5;
    stepsBar->setState (completed, juce::jmin (completed, 4));

    resized();
    repaint();
}

void MeasurePage::timerCallback()
{
    levelMeter->setLivePeak (session.getInputPeakDbfs());

    if (session.isBusy())
    {
        progressGrid->repaint();
        positionMap->repaint();
    }
}

void MeasurePage::resized()
{
    auto area = getLocalBounds().reduced (16);

    setupPanel = area.removeFromLeft (250);
    area.removeFromLeft (16);
    levelPanel = area.removeFromRight (200);
    area.removeFromRight (16);

    // --- setup ---
    {
        auto inner = setupPanel.reduced (16, 12);
        inner.removeFromTop (22);                          // titolo
        setupInfoArea = inner.removeFromTop (46);          // istruzioni scheda audio
        inner.removeFromTop (6);

        auto field = [&inner] (juce::Component& c, int height)
        {
            inner.removeFromTop (18);
            c.setBounds (inner.removeFromTop (height));
            inner.removeFromTop (10);
        };

        field (micBox, 34);
        field (calBox, 34);
        inner.removeFromTop (14);                          // nome file calibrazione
        field (outputLevel, 26);
        field (pointsBox, 34);

        auto nameArea = inner.removeFromBottom (52);
        nameArea.removeFromTop (18);
        profileName.setBounds (nameArea.removeFromTop (34));
        inner.removeFromBottom (8);
        progressInfoArea = inner.removeFromBottom (44);    // sample rate e latenza
    }

    // --- centro ---
    auto steps = area.removeFromBottom (30);
    stepsBar->setBounds (steps);
    area.removeFromBottom (14);

    auto top = area.removeFromTop (250);
    progressPanel = top.removeFromRight (220);
    top.removeFromRight (16);
    mapPanel = top;
    area.removeFromTop (16);
    graphPanel = area;

    {
        auto inner = mapPanel.reduced (16, 12);
        inner.removeFromTop (22);
        mapCaptionArea = inner.removeFromBottom (32);
        positionMap->setBounds (inner);
    }
    {
        auto inner = progressPanel.reduced (16, 12);
        inner.removeFromTop (22);
        progressGrid->setBounds (inner);
    }
    {
        auto inner = graphPanel.reduced (16, 12);
        inner.removeFromTop (22);
        graph.setBounds (inner);
    }

    // --- livello ---
    {
        auto inner = levelPanel.reduced (16, 12);
        inner.removeFromTop (22);

        auto buttons = inner.removeFromBottom (130);
        saveButton.setBounds (buttons.removeFromBottom (44));
        measureButton.setBounds (saveButton.getBounds());
        buttons.removeFromBottom (8);
        checkButton.setBounds (buttons.removeFromBottom (36));
        buttons.removeFromBottom (8);
        resetButton.setBounds (buttons.removeFromBottom (32));

        inner.removeFromBottom (12);
        levelMeter->setBounds (inner);
    }
}

void MeasurePage::paint (juce::Graphics& g)
{

    ScribaLookAndFeel::drawPanel (g, setupPanel, "SETUP AUDIO");
    ScribaLookAndFeel::drawPanel (g, mapPanel, "POSIZIONE MICROFONO");
    ScribaLookAndFeel::drawPanel (g, progressPanel, "AVANZAMENTO");
    ScribaLookAndFeel::drawPanel (g, graphPanel, "RISPOSTA MISURATA");
    ScribaLookAndFeel::drawPanel (g, levelPanel, "LIVELLO INGRESSO");

    // istruzioni scheda audio
    g.setColour (textMuted);
    g.setFont (ScribaLookAndFeel::font (11.5f));
    g.drawFittedText (juce::String::fromUTF8 ("Scheda audio e uscite: Opzioni \xe2\x86\x92 Audio/MIDI Settings. "
                                              "Disattiva \"Mute audio input\"."),
                      setupInfoArea, juce::Justification::topLeft, 3);

    // nome del file di calibrazione
    g.setFont (ScribaLookAndFeel::mono (11.0f));
    g.drawText (calibrationFileName, calBox.getBounds().translated (0, calBox.getHeight() + 2).withHeight (14),
                juce::Justification::centredLeft);

    // sample rate e latenza
    {
        auto info = progressInfoArea;
        g.setColour (border);
        g.drawHorizontalLine (info.getY(), (float) info.getX(), (float) info.getRight());
        info.removeFromTop (6);

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

        row ("Sample rate", juce::String (session.getSampleRate() / 1000.0, 1) + " kHz");
        row ("Latenza rilevata", session.getDetectedLatency() > 0 ? juce::String (session.getDetectedLatency()) + " smp" : "--");
    }

    // didascalia della mappa
    {
        const int n = (int) session.getPoints().size();
        auto caption = mapCaptionArea;
        g.setColour (text);
        g.setFont (ScribaLookAndFeel::font (13.0f, true));
        g.drawText ("Punto " + juce::String (session.getCurrentPoint() + 1) + " di " + juce::String (n),
                    mapPanel.reduced (16, 12).removeFromTop (16), juce::Justification::centredRight);
        g.setColour (textMuted);
        g.setFont (ScribaLookAndFeel::font (12.0f));
        g.drawFittedText (juce::String (session.getCurrentPoint() == 0
                                ? "Microfono al punto d'ascolto, all'altezza delle orecchie, puntato in avanti tra i due monitor."
                                : "Sposta il microfono sul punto evidenziato (circa 30 cm dal centro): stessa altezza, sempre puntato in avanti."),
                          caption, juce::Justification::centredLeft, 2);
    }

    // legenda del grafico
    {
        auto legend = graphPanel.reduced (16, 12).removeFromTop (16);
        g.setFont (ScribaLookAndFeel::font (12.0f));

        auto item = [&g, &legend] (const juce::String& name, juce::Colour c, bool dashed)
        {
            auto r = legend.removeFromRight (juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), name) + 26);
            g.setColour (textMuted);
            g.drawText (name, r.withTrimmedLeft (20), juce::Justification::centredLeft);
            g.setColour (c);
            if (dashed)
            {
                g.fillRect (r.getX(), r.getCentreY() - 1, 5, 2);
                g.fillRect (r.getX() + 9, r.getCentreY() - 1, 5, 2);
            }
            else
            {
                g.fillRect (r.getX(), r.getCentreY() - 1, 14, 2);
            }
            legend.removeFromRight (8);
        };

        item ("Target", textMuted, true);
        item ("Destro", right, false);
        item ("Sinistro", left, false);
    }
}
