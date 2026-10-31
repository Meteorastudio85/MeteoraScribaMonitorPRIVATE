#pragma once

#include "FrequencyResponse.h"
#include <juce_core/juce_core.h>

namespace scriba
{

// Un profilo è il risultato di una sessione di misura: la risposta media
// (calibrata, non smussata) dei due monitor. Il filtro NON viene salvato:
// il plugin lo ricalcola dal profilo con target e regolazioni correnti,
// alla frequenza di campionamento della sessione DAW.
struct Profile
{
    juce::String name;
    juce::Time created;
    double measuredSampleRate = 48000.0;
    int numPoints = 0;
    juce::String calibrationName;
    Response left, right;

    bool isValid() const    { return numPoints > 0; }

    juce::var toVar() const;
    static Profile fromVar (const juce::var& v);

    bool saveTo (const juce::File& file) const;
    static Profile loadFrom (const juce::File& file);

    juce::String getDisplayName() const;

    static juce::File getProfilesFolder();          // Documenti/Meteora Plugins/Scriba Monitor/Profiles
    static constexpr const char* fileExtension = ".scribamon";
};

} // namespace scriba
