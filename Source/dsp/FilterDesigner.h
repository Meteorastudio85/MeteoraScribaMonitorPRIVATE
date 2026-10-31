#pragma once

#include "FrequencyResponse.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace scriba
{

enum class TargetType     { flat = 0, studio, custom };
enum class CorrectionRange { upTo300Hz = 0, upTo1kHz, fullRange };

struct CorrectionSettings
{
    TargetType target = TargetType::flat;
    Response customTarget;                  // usato con TargetType::custom

    float tiltDbPerOctave = 0.0f;           // pivot 1 kHz
    float bassDb = 0.0f;                    // shelf ~ 100 Hz
    float trebleDb = 0.0f;                  // shelf ~ 8 kHz

    float maxBoostDb = 6.0f;
    float maxCutDb = 15.0f;
    CorrectionRange range = CorrectionRange::fullRange;

    int emulation = 0;                      // 0 = nessuna
    bool emulationEnabled = true;

    bool linearPhase = false;
};

// ---- Emulazioni di monitor/sistemi di ascolto ----------------------------
// Curve parametriche indicative: punto di partenza da rifinire con misure reali.
struct EmulationBump { double hz, gainDb, widthOctaves; };

struct EmulationModel
{
    juce::String name;
    double highPassHz = 0.0;  int highPassOrder = 2;
    double lowPassHz  = 0.0;  int lowPassOrder  = 2;
    std::vector<EmulationBump> bumps;
};

const std::vector<EmulationModel>& getEmulations();     // indice 0 = "Nessuna"
Response emulationCurve (int index);                    // normalizzata a 0 dB in 300 Hz–3 kHz

// ---- Target e correzione ------------------------------------------------
Response targetCurve (const CorrectionSettings& settings);

// Normalizza la misura (0 dB medi tra 200 Hz e 2 kHz) e applica lo smoothing
// usato per la correzione (1/6 di ottava, 1/3 sopra 1 kHz).
Response prepareMeasurement (const Response& averagedRaw);

struct CorrectionDesign
{
    Response measured;          // misura preparata
    Response target;            // target completo (con tilt/bassi/acuti)
    Response correction;        // solo la correzione della stanza
    Response filter;            // filtro totale = correzione + emulazione
    Response predicted;         // risultato previsto = misura + filtro
    double lowRolloffHz = 0.0;  // sotto questa frequenza niente boost
};

CorrectionDesign designCorrection (const Response& preparedMeasurement, const CorrectionSettings& settings);

// ---- Costruzione del FIR ------------------------------------------------
int firLengthForSampleRate (double sampleRate);

std::vector<float> buildFir (const Response& filterDb, double sampleRate,
                             int numTaps, bool linearPhase);

} // namespace scriba
