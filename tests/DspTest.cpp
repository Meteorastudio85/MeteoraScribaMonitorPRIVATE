// Test del motore DSP di Scriba Monitor su una stanza simulata.
// Esegui dalla cartella del progetto (serve Resources/ECM8000_generic.cal).

#include <juce_dsp/juce_dsp.h>
#include "dsp/SweepAnalyzer.h"
#include "dsp/FilterDesigner.h"
#include "dsp/MicCalibration.h"
#include "dsp/Profile.h"

#include <iostream>
#include <cmath>

using namespace scriba;

static int failures = 0;

static void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  OK    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

// Stanza + monitor simulati come catena di biquad, più ritardo e una riflessione.
static std::vector<float> simulateRoom (const std::vector<float>& input, double sr, int delaySamples)
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    std::vector<juce::dsp::IIR::Filter<float>> chain;
    chain.emplace_back (Coeffs::makeHighPass (sr, 45.0, 0.7));          // woofer
    chain.emplace_back (Coeffs::makePeakFilter (sr, 55.0, 5.0, 2.5f));  // modo  +8 dB
    chain.emplace_back (Coeffs::makePeakFilter (sr, 120.0, 4.0, 0.32f));// buco -10 dB
    chain.emplace_back (Coeffs::makePeakFilter (sr, 2500.0, 1.0, 1.4f));// monitor +3 dB
    chain.emplace_back (Coeffs::makeHighShelf (sr, 12000.0, 0.7, 0.7f));// caduta acuti

    std::vector<float> out (input.size() + (size_t) delaySamples + (size_t) sr, 0.0f);
    const int reflection = (int) (0.003 * sr);

    for (size_t i = 0; i < out.size(); ++i)
    {
        float x = i < input.size() ? input[i] : 0.0f;
        for (auto& f : chain)
            x = f.processSample (x);

        const size_t d = i + (size_t) delaySamples;
        if (d < out.size())                       out[d] += x;
        if (d + (size_t) reflection < out.size()) out[d + (size_t) reflection] += 0.4f * x;
    }

    return out;
}

static std::vector<float> convolve (const std::vector<float>& a, const std::vector<float>& b)
{
    const size_t n = (size_t) 1 << nextPowerOfTwoOrder (a.size() + b.size());
    std::vector<std::complex<float>> A (n), B (n);
    for (size_t i = 0; i < a.size(); ++i) A[i] = a[i];
    for (size_t i = 0; i < b.size(); ++i) B[i] = b[i];
    fftInPlace (A, false);
    fftInPlace (B, false);
    for (size_t i = 0; i < n; ++i) A[i] *= B[i];
    fftInPlace (A, true);
    std::vector<float> out (a.size() + b.size() - 1);
    for (size_t i = 0; i < out.size(); ++i) out[i] = A[i].real();
    return out;
}

static double deviation (const Response& r, const Response& target, double lo, double hi)
{
    double sum = 0.0; int count = 0;
    for (int i = 0; i < FreqGrid::numPoints; ++i)
    {
        const double f = FreqGrid::freq (i);
        if (f < lo || f > hi) continue;
        const double e = r.db[(size_t) i] - target.db[(size_t) i];
        sum += e * e; ++count;
    }
    return std::sqrt (sum / std::max (1, count));
}

int main()
{
    const double sr = 48000.0;
    std::cout << "Scriba Monitor - test DSP" << std::endl;

    // 1) calibrazione microfono
    auto cal = MicCalibration::defaultECM8000();
    check (cal.isValid(), "calibrazione ECM8000 caricata");
    check (std::abs (cal.getResponse().at (1000.0)) < 0.01f && cal.getResponse().at (12000.0) > 2.5f,
           "calibrazione interpolata (1 kHz = " + juce::String (cal.getResponse().at (1000.0), 2)
           + " dB, 12 kHz = " + juce::String (cal.getResponse().at (12000.0), 2) + " dB)");

    // 2) sweep → stanza → deconvoluzione
    SweepSettings sweepSettings;
    sweepSettings.sampleRate = sr;
    sweepSettings.amplitude = 0.3f;
    const auto sweep = generateLogSweep (sweepSettings);

    const int systemLatency = 777;
    auto recording = simulateRoom (sweep, sr, systemLatency);

    juce::Random rng (1);
    for (auto& v : recording)
        v += (rng.nextFloat() * 2.0f - 1.0f) * 1.0e-4f;   // rumore di fondo

    const auto impulse = deconvolveSweep (recording, sweep, sweepSettings);
    check (impulse.valid, "risposta all'impulso calcolata");
    check (std::abs (impulse.peakIndex - systemLatency) < 40,
           "latenza rilevata " + juce::String (impulse.peakIndex) + " campioni (attesa ~" + juce::String (systemLatency) + ")");

    auto raw = responseFromImpulse (impulse, sr);
    auto measured = prepareMeasurement (raw);

    check (measured.at (55.0) > 5.0f,  "modo a 55 Hz visibile nella misura (" + juce::String (measured.at (55.0), 1) + " dB)");
    check (measured.at (120.0) < -5.0f, "buco a 120 Hz visibile nella misura (" + juce::String (measured.at (120.0), 1) + " dB)");

    // 3) progetto della correzione
    CorrectionSettings settings;
    const auto design = designCorrection (measured, settings);

    check (design.lowRolloffHz > 15.0 && design.lowRolloffHz < 60.0,
           "roll-off dei monitor rilevato a " + juce::String (design.lowRolloffHz, 1) + " Hz");
    check (design.correction.at (55.0) < -4.0f, "il modo a 55 Hz viene tagliato");
    check (design.correction.at (120.0) <= settings.maxBoostDb + 0.01f, "il boost sul buco resta entro il limite");

    // 4) FIR a fase minima: la sua risposta deve seguire il filtro progettato
    const int taps = firLengthForSampleRate (sr);
    check (taps == 8192, "lunghezza FIR a 48 kHz = " + juce::String (taps));

    for (bool linear : { false, true })
    {
        const auto fir = buildFir (design.filter, sr, taps, linear);

        ImpulseResult firAsImpulse;
        firAsImpulse.ir = fir;
        firAsImpulse.valid = true;
        firAsImpulse.peakIndex = linear ? taps / 2 : 0;

        auto firResponse = responseFromImpulse (firAsImpulse, sr, linear ? 85.0 : 170.0, linear ? 85.0 : 0.0).smoothed (12.0);
        const double err = deviation (firResponse, design.filter.smoothed (12.0), 40.0, 16000.0);
        check (err < 0.75, juce::String (linear ? "FIR fase lineare" : "FIR fase minima")
                           + ": errore rispetto al filtro " + juce::String (err, 2) + " dB RMS");
    }

    // 5) catena completa: stanza + correzione, misurata di nuovo
    {
        const auto fir = buildFir (design.filter, sr, taps, false);
        const auto corrected = convolve (recording, fir);

        auto impulse2 = deconvolveSweep (corrected, sweep, sweepSettings);
        auto after = prepareMeasurement (responseFromImpulse (impulse2, sr));

        const auto target = targetCurve (settings);
        const double before = deviation (measured, target, 50.0, 16000.0);
        const double afterDev = deviation (after, target, 50.0, 16000.0);

        check (afterDev < before * 0.5, "scostamento dal target 50 Hz–16 kHz: prima "
                                        + juce::String (before, 2) + " dB, dopo " + juce::String (afterDev, 2) + " dB");
        check (std::abs (after.at (55.0)) < 2.0f, "modo a 55 Hz dopo la correzione: " + juce::String (after.at (55.0), 1) + " dB");
    }

    // 6) emulazioni
    for (int i = 1; i < (int) getEmulations().size(); ++i)
    {
        auto e = emulationCurve (i);
        check (std::abs (e.bandLevel (300.0, 3000.0)) < 0.1f && e.at (30.0) < -3.0f,
               "emulazione \"" + getEmulations()[(size_t) i].name + "\" normalizzata");
    }

    // 7) profilo: salvataggio e ricarica
    {
        Profile p;
        p.name = "Test";
        p.created = juce::Time::getCurrentTime();
        p.numPoints = 9;
        p.calibrationName = cal.getName();
        p.left = raw;
        p.right = raw;

        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("scriba_test.scribamon");
        check (p.saveTo (file), "profilo salvato");
        auto q = Profile::loadFrom (file);
        check (q.isValid() && std::abs (q.left.at (55.0) - p.left.at (55.0)) < 0.02f, "profilo ricaricato identico");
        file.deleteFile();
    }

    // 8) livello
    {
        std::vector<float> clip (1000, 1.0f);
        check (measureLevel (clip.data(), 1000).clipped, "clipping rilevato");
        std::vector<float> silence (1000, 0.0f);
        check (measureLevel (silence.data(), 1000).silent, "silenzio rilevato");
    }

    std::cout << (failures == 0 ? "\nTutti i test superati." : "\nTest falliti: " + std::to_string (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
