#pragma once

#include "FrequencyResponse.h"
#include <juce_core/juce_core.h>

namespace scriba
{

// File di calibrazione del microfono nel formato testo usato da REW e dai
// produttori (righe "frequenza  dB  [fase]"; le righe di intestazione,
// i commenti con * " # e la riga "Sens Factor" vengono ignorati).
class MicCalibration
{
public:
    MicCalibration() = default;

    static MicCalibration defaultECM8000();
    static MicCalibration fromFile (const juce::File& file);
    static MicCalibration fromText (const juce::String& text, const juce::String& name);

    bool isValid() const                    { return valid; }
    const juce::String& getName() const     { return name; }

    // Risposta del microfono sulla griglia: va SOTTRATTA alla misura.
    const Response& getResponse() const     { return response; }

private:
    juce::String name { "Nessuna calibrazione" };
    Response response;
    bool valid = false;
};

} // namespace scriba
