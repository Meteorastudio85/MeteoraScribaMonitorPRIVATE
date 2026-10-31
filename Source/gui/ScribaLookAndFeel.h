#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Palette e componenti grafici di Scriba Monitor, nello stile della famiglia Scriba (Scriba Align):
// sfondo blu notte sfumato, pannelli scuri arrotondati, accento verde acqua, titoli maiuscoli spaziati.
namespace ScribaColours
{
    inline const juce::Colour bgTop       { 0xff141a26 };   // sfumatura dello sfondo, in alto a sinistra
    inline const juce::Colour bgBottom    { 0xff3e5b80 };   // ... in basso a destra
    inline const juce::Colour background  { 0xff121925 };   // campi (combo, testo, meter)
    inline const juce::Colour header      { 0xff161d2a };
    inline const juce::Colour panel       { 0xe6171f2d };   // pannelli, leggermente trasparenti
    inline const juce::Colour border      { 0xff2a3547 };
    inline const juce::Colour control     { 0xff2a3547 };
    inline const juce::Colour controlHi   { 0xff3d4b61 };
    inline const juce::Colour chip        { 0xff1b2333 };   // pulsanti non selezionati
    inline const juce::Colour grid        { 0xff243043 };
    inline const juce::Colour text        { 0xfff2f5f8 };
    inline const juce::Colour textSoft    { 0xffd0d6de };
    inline const juce::Colour textMuted   { 0xff8a97a8 };
    inline const juce::Colour textDim     { 0xff5f6d80 };
    inline const juce::Colour accent      { 0xff34dcc0 };   // verde acqua Scriba
    inline const juce::Colour accentSoft  { 0xffdaf9f2 };   // inizio della sfumatura del pulsante principale
    inline const juce::Colour accentBg    { 0xff173a3c };   // fondo degli elementi selezionati
    inline const juce::Colour accentInk   { 0xff0e3a3a };   // testo sul pulsante principale
    inline const juce::Colour good        { 0xff34dcc0 };
    inline const juce::Colour goodBg      { 0xff173a3c };
    inline const juce::Colour goodText    { 0xff34dcc0 };
    inline const juce::Colour warn        { 0xfff2c46d };
    inline const juce::Colour bad         { 0xfff07a7a };
    inline const juce::Colour badBg       { 0xff3a2430 };
    inline const juce::Colour left        { 0xff8fd3e8 };
    inline const juce::Colour right       { 0xffc9a7f0 };
}

class ScribaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ScribaLookAndFeel()
    {
        using namespace ScribaColours;
        setColour (juce::ResizableWindow::backgroundColourId, bgTop);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextButton::textColourOffId, textSoft);
        setColour (juce::TextButton::textColourOnId, accent);
        setColour (juce::ComboBox::backgroundColourId, background);
        setColour (juce::ComboBox::outlineColourId, border);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::PopupMenu::backgroundColourId, header);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accentBg);
        setColour (juce::PopupMenu::highlightedTextColourId, accent);
        setColour (juce::TextEditor::backgroundColourId, background);
        setColour (juce::TextEditor::outlineColourId, border);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accentBg);
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::backgroundColourId, border);
        setColour (juce::Slider::thumbColourId, text);
    }

    static juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Titoli e pulsanti: maiuscolo, grassetto, spaziato.
    static juce::Font caps (float height)
    {
        return font (height, true).withExtraKerningFactor (0.14f);
    }

    static juce::Font mono (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }

    // Sfondo della finestra: sfumatura diagonale blu notte.
    static void fillBackground (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setGradientFill (juce::ColourGradient (ScribaColours::bgTop, (float) area.getX(), (float) area.getY(),
                                                 ScribaColours::bgBottom, (float) area.getRight(), (float) area.getBottom(), false));
        g.fillRect (area);
    }

    // Pannello arrotondato con titolo maiuscolo spaziato, come in Scriba Align.
    static juce::Rectangle<int> drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
    {
        g.setColour (ScribaColours::panel);
        g.fillRoundedRectangle (area.toFloat(), 16.0f);
        g.setColour (ScribaColours::border);
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 16.0f, 1.0f);

        auto inner = area.reduced (14, 12);
        if (title.isNotEmpty())
        {
            g.setColour (ScribaColours::textMuted);
            g.setFont (caps (12.0f).withExtraKerningFactor (0.2f));
            g.drawText (title, inner.removeFromTop (16), juce::Justification::centredLeft);
            inner.removeFromTop (6);
        }
        return inner;
    }

    // Pastiglia con pallino verde acqua (es. "● 4 TRACCE" in Scriba Align).
    static void drawPill (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label)
    {
        g.setColour (ScribaColours::background);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (ScribaColours::border);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);

        auto inner = r.reduced (r.getHeight() * 0.45f, 0.0f);
        g.setColour (ScribaColours::accent);
        g.fillEllipse (inner.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 10.0f));
        inner.removeFromLeft (10.0f);
        g.setColour (ScribaColours::text);
        g.setFont (caps (13.0f));
        g.drawText (label, inner, juce::Justification::centredLeft);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        using namespace ScribaColours;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool primary = b.getProperties()["primary"];
        const bool on = b.getToggleState();
        const float radius = juce::jmin (12.0f, r.getHeight() * 0.3f);

        if (primary)
        {
            if (! b.isEnabled())
            {
                g.setColour (chip);
                g.fillRoundedRectangle (r, radius);
                g.setColour (border);
                g.drawRoundedRectangle (r, radius, 1.0f);
                return;
            }

            auto c1 = accentSoft, c2 = accent;
            if (down)             { c1 = c1.darker (0.15f); c2 = c2.darker (0.15f); }
            else if (highlighted) { c1 = c1.brighter (0.05f); c2 = c2.brighter (0.08f); }

            g.setGradientFill (juce::ColourGradient (c1, r.getX(), r.getY(), c2, r.getRight(), r.getY(), false));
            g.fillRoundedRectangle (r, radius);
            return;
        }

        g.setColour (on ? accentBg : (highlighted ? chip.brighter (0.08f) : chip));
        g.fillRoundedRectangle (r, radius);
        g.setColour (on ? accent : (highlighted ? controlHi : border));
        g.drawRoundedRectangle (r, radius, on ? 2.0f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        using namespace ScribaColours;
        const bool primary = b.getProperties()["primary"];
        const bool on = b.getToggleState();

        const float size = primary ? 15.0f : (b.getHeight() < 34 ? 11.5f : 12.5f);
        g.setFont (caps (size));
        g.setColour (primary ? (b.isEnabled() ? accentInk : textDim)
                             : on ? accent : (b.isEnabled() ? textSoft : textDim));
        g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (8, 2),
                          juce::Justification::centred, 2, 0.85f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override        { return caps (12.5f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override                  { return font (14.0f, true); }
    juce::Font getPopupMenuFont() override                                 { return font (14.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (ScribaColours::header);
        g.setColour (ScribaColours::border);
        g.drawRect (0, 0, w, h);
    }

    void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&) override
    {
        g.setColour (ScribaColours::background);
        g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 10.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        g.setColour (e.hasKeyboardFocus (true) ? ScribaColours::accent : ScribaColours::border);
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 10.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (ScribaColours::background);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (box.hasKeyboardFocus (true) ? ScribaColours::accent : ScribaColours::border);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);

        juce::Path arrow;
        const float cx = (float) w - 18.0f, cy = (float) h * 0.5f;
        arrow.startNewSubPath (cx - 6.0f, cy - 3.0f);
        arrow.lineTo (cx, cy + 3.0f);
        arrow.lineTo (cx + 6.0f, cy - 3.0f);
        g.setColour (box.isEnabled() ? ScribaColours::accent : ScribaColours::textDim);
        g.strokePath (arrow, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 0, box.getWidth() - 38, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace ScribaColours;
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
        const float thickness = juce::jmax (6.0f, bounds.getWidth() * 0.1f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - thickness * 0.5f - 3.0f;
        const auto centre = bounds.getCentre();

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (border);
        g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // cerchio interno, come il quadrante di Scriba Align
        g.setColour (background.withAlpha (0.6f));
        g.fillEllipse (juce::Rectangle<float> (radius * 1.4f, radius * 1.4f).withCentre (centre));
        g.setColour (border);
        g.drawEllipse (juce::Rectangle<float> (radius * 1.4f, radius * 1.4f).withCentre (centre), 1.0f);

        // i parametri bipolari partono dal centro
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float origin = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
        const float a0 = startAngle + origin * (endAngle - startAngle);
        const float a1 = startAngle + pos * (endAngle - startAngle);
        const auto colour = s.isEnabled() ? accent : textDim;

        if (std::abs (a1 - a0) > 0.001f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (colour);
            g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // pallino sulla punta, con bordo scuro
        const auto tip = centre.getPointOnCircumference (radius, a1);
        const float dot = thickness * 1.25f;
        g.setColour (background);
        g.fillEllipse (juce::Rectangle<float> (dot + 4.0f, dot + 4.0f).withCentre (tip));
        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (dot, dot).withCentre (tip));

        // valore al centro
        g.setColour (text);
        g.setFont (font (13.0f, true));
        g.drawText (s.getTextFromValue (s.getValue()), bounds.withSizeKeepingCentre (radius * 1.4f, 18.0f),
                    juce::Justification::centred);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace ScribaColours;
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (border);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, (float) w, 6.0f, 3.0f);
        g.setColour (s.isEnabled() ? accent : textDim);
        g.fillRoundedRectangle ((float) x, cy - 3.0f, sliderPos - (float) x, 6.0f, 3.0f);
        g.setColour (text);
        g.fillEllipse (sliderPos - 9.0f, cy - 9.0f, 18.0f, 18.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textColourId, ScribaColours::text);
        l->setFont (font (13.0f, true));
        l->setJustificationType (juce::Justification::centredRight);
        return l;
    }
};
