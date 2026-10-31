#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/FrequencyResponse.h"

// Grafico della risposta in frequenza (asse log 20 Hz – 20 kHz).
class ResponseGraph : public juce::Component
{
public:
    struct Curve
    {
        scriba::Response response;
        juce::Colour colour;
        float thickness = 2.0f;
        bool dashed = false;
    };

    void setCurves (std::vector<Curve> newCurves)       { curves = std::move (newCurves); repaint(); }
    void setDbRange (float range)                       { dbRange = range; repaint(); }
    void setHighlightBelow (double hz, const juce::String& label)
    {
        highlightHz = hz; highlightLabel = label; repaint();
    }
    void setEmptyMessage (const juce::String& m)        { emptyMessage = m; repaint(); }

    void paint (juce::Graphics& g) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForFreq (double hz) const;
    float yForDb (float db) const;

    std::vector<Curve> curves;
    float dbRange = 15.0f;
    double highlightHz = 0.0;
    juce::String highlightLabel, emptyMessage;
};
