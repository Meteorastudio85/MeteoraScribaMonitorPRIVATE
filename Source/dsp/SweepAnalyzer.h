#pragma once

#include "FrequencyResponse.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <complex>

namespace scriba
{

// FFT complessa in-place (dimensione potenza di 2). L'inversa è normalizzata 1/N.
void fftInPlace (std::vector<std::complex<float>>& data, bool inverse);
int nextPowerOfTwoOrder (size_t n);

struct SweepSettings
{
    double sampleRate = 48000.0;
    double durationSeconds = 6.0;
    double startHz = 15.0;
    double endHz = 22000.0;      // limitato automaticamente a 0.45 * sampleRate
    float amplitude = 0.1f;       // lineare (picco)
};

// Sweep sinusoidale esponenziale (Farina) con brevi fade di ingresso e uscita.
std::vector<float> generateLogSweep (const SweepSettings& settings);

struct ImpulseResult
{
    std::vector<float> ir;
    int peakIndex = 0;            // posizione del picco = latenza di sistema + tempo di volo
    bool valid = false;
    juce::String error;
};

// Deconvoluzione registrazione / sweep (divisione spettrale regolarizzata).
ImpulseResult deconvolveSweep (const std::vector<float>& recording,
                               const std::vector<float>& sweep,
                               const SweepSettings& settings);

// Risposta in ampiezza dalla risposta all'impulso, finestrata attorno al picco.
// La finestra lunga (default 500 ms) include la stanza: è quello che vogliamo
// per correggere i modi in bassa frequenza.
Response responseFromImpulse (const ImpulseResult& impulse, double sampleRate,
                              double windowMs = 500.0, double preWindowMs = 5.0);

// Statistiche di livello per il check di ingresso.
struct LevelStats
{
    float peakDbfs = -120.0f;
    float rmsDbfs = -120.0f;
    bool clipped = false;
    bool silent = true;          // nessun segnale (tutti zeri)
};

LevelStats measureLevel (const float* data, int numSamples);

} // namespace scriba
