#include "FilterDesigner.h"
#include "SweepAnalyzer.h"

#include <cmath>
#include <algorithm>
#include <complex>

namespace scriba
{

// ------------------------------------------------------------------------
// Forme di base (in dB, continue in log-frequenza)

static double lowShelfDb (double hz, double cornerHz, double gainDb)
{
    return gainDb * 0.5 * (1.0 - std::tanh (1.5 * std::log2 (hz / cornerHz)));
}

static double highShelfDb (double hz, double cornerHz, double gainDb)
{
    return gainDb * 0.5 * (1.0 + std::tanh (1.5 * std::log2 (hz / cornerHz)));
}

static double highPassDb (double hz, double cornerHz, int order)
{
    return -10.0 * std::log10 (1.0 + std::pow (cornerHz / hz, 2.0 * order));
}

static double lowPassDb (double hz, double cornerHz, int order)
{
    return -10.0 * std::log10 (1.0 + std::pow (hz / cornerHz, 2.0 * order));
}

static double bumpDb (double hz, const EmulationBump& b)
{
    const double sigma = b.widthOctaves / 2.355;   // larghezza a metà altezza
    const double x = std::log2 (hz / b.hz) / sigma;
    return b.gainDb * std::exp (-0.5 * x * x);
}

// Peso 1 sotto 'hz', 0 un'ottava sopra, raccordo a coseno.
static double fadeAbove (double f, double hz)
{
    const double oct = std::log2 (f / hz);
    if (oct <= 0.0) return 1.0;
    if (oct >= 1.0) return 0.0;
    return 0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * oct);
}

// ------------------------------------------------------------------------
// Emulazioni

const std::vector<EmulationModel>& getEmulations()
{
    static const std::vector<EmulationModel> models =
    {
        { "Nessuna", 0.0, 2, 0.0, 2, {} },
        { "Nearfield bianco anni '80", 70.0, 2, 16000.0, 1, { { 1300.0, 3.5, 1.5 }, { 4500.0, -1.5, 1.0 } } },
        { "Cubo mid-range",           180.0, 2,  8000.0, 2, { { 1200.0, 3.0, 1.2 } } },
        { "Studio 3 vie",              35.0, 2, 22000.0, 1, { { 3000.0, -1.0, 1.0 } } },
        { "Hi-fi da salotto",          45.0, 2, 20000.0, 1, { { 80.0, 3.0, 1.5 }, { 10000.0, 2.0, 2.0 } } },
        { "Autoradio",                 60.0, 2, 12000.0, 2, { { 70.0, 4.0, 1.5 }, { 2500.0, -3.0, 1.0 } } },
        { "Cassa Bluetooth",           90.0, 4, 14000.0, 2, { { 150.0, 3.0, 1.0 } } },
        { "Laptop",                   250.0, 4, 15000.0, 2, { { 3000.0, 4.0, 1.5 } } },
        { "Smartphone",               450.0, 4, 12000.0, 2, { { 2500.0, 5.0, 1.2 } } },
    };
    return models;
}

Response emulationCurve (int index)
{
    const auto& models = getEmulations();
    if (index <= 0 || index >= (int) models.size())
        return {};

    const auto& m = models[(size_t) index];

    auto r = Response::fromFunction ([&m] (double hz)
    {
        double db = 0.0;
        if (m.highPassHz > 0.0) db += highPassDb (hz, m.highPassHz, m.highPassOrder);
        if (m.lowPassHz  > 0.0) db += lowPassDb  (hz, m.lowPassHz,  m.lowPassOrder);
        for (auto& b : m.bumps)  db += bumpDb (hz, b);
        return std::max (db, -40.0);
    });

    r += -r.bandLevel (300.0, 3000.0);
    return r;
}

// ------------------------------------------------------------------------
// Target

Response targetCurve (const CorrectionSettings& s)
{
    Response base;

    if (s.target == TargetType::studio)
        base = Response::fromFunction ([] (double hz)
        {
            const double f = std::clamp (hz, 20.0, 20000.0);
            // leggera pendenza verso il basso e un po' di corpo sotto i 120 Hz
            return -0.5 * std::log2 (f / 1000.0) + lowShelfDb (hz, 120.0, 1.5);
        });
    else if (s.target == TargetType::custom && ! s.customTarget.isEmpty())
        base = s.customTarget;

    auto tweaks = Response::fromFunction ([&s] (double hz)
    {
        const double f = std::clamp (hz, 20.0, 20000.0);
        return s.tiltDbPerOctave * std::log2 (f / 1000.0)
             + lowShelfDb (hz, 100.0, s.bassDb)
             + highShelfDb (hz, 8000.0, s.trebleDb);
    });

    return base + tweaks;
}

// ------------------------------------------------------------------------
// Correzione

Response prepareMeasurement (const Response& averagedRaw)
{
    auto m = averagedRaw.smoothed (6.0, 3.0, 1000.0);
    m += -m.bandLevel (200.0, 2000.0);
    return m;
}

CorrectionDesign designCorrection (const Response& measured, const CorrectionSettings& s)
{
    CorrectionDesign d;
    d.measured = measured;
    d.target = targetCurve (s);

    // 1) dove i monitor smettono di emettere: niente boost sotto questo punto,
    //    altrimenti si spinge il woofer oltre i suoi limiti.
    const auto coarse = measured.smoothed (3.0);
    d.lowRolloffHz = FreqGrid::fMin;
    for (int i = (int) std::floor (FreqGrid::position (100.0)); i >= 0; --i)
    {
        if (coarse.db[(size_t) i] < -6.0f)
        {
            d.lowRolloffHz = FreqGrid::freq (i);
            break;
        }
    }

    const double rangeHz = s.range == CorrectionRange::upTo300Hz ? 300.0
                         : s.range == CorrectionRange::upTo1kHz  ? 1000.0
                                                                 : 20000.0;

    for (int i = 0; i < FreqGrid::numPoints; ++i)
    {
        const double f = FreqGrid::freq (i);
        double c = (double) d.target.db[(size_t) i] - (double) measured.db[(size_t) i];

        // limiti di boost/taglio (più prudenti in alta frequenza)
        const double boostLimit = f > 2000.0 ? std::min ((double) s.maxBoostDb, 4.0) : (double) s.maxBoostDb;
        const double cutLimit   = f > 2000.0 ? std::min ((double) s.maxCutDb, 6.0)   : (double) s.maxCutDb;
        c = std::clamp (c, -cutLimit, boostLimit);

        // sotto il roll-off dei monitor: solo tagli, poi sfuma a zero
        if (f < d.lowRolloffHz * 1.1)
        {
            c = std::min (c, 0.0);
            const double w = std::clamp (std::log2 (f / (d.lowRolloffHz * 0.5)), 0.0, 1.0);
            c *= w;
        }

        // range di correzione scelto dall'utente
        if (s.range != CorrectionRange::fullRange)
            c *= fadeAbove (f, rangeHz);

        // sopra 18 kHz la misura è poco affidabile
        c *= fadeAbove (f, 18000.0);

        // sotto 15 Hz nessuna correzione
        if (f < 15.0) c = 0.0;

        d.correction.db[(size_t) i] = (float) c;
    }

    d.correction = d.correction.smoothed (12.0);

    d.filter = d.correction;
    if (s.emulationEnabled && s.emulation > 0)
        d.filter += emulationCurve (s.emulation);

    // la regolazione di tilt/bassi/acuti è già nel target; per i range
    // parziali la aggiungiamo anche sopra il range, altrimenti non si sentirebbe
    if (s.range != CorrectionRange::fullRange)
    {
        CorrectionSettings onlyTweaks = s;
        onlyTweaks.target = TargetType::flat;
        const auto tweaks = targetCurve (onlyTweaks);

        for (int i = 0; i < FreqGrid::numPoints; ++i)
        {
            const double f = FreqGrid::freq (i);
            d.filter.db[(size_t) i] += (float) ((1.0 - fadeAbove (f, rangeHz)) * tweaks.db[(size_t) i]);
        }
    }

    // livello: 0 dB medi tra 200 Hz e 5 kHz, così bypass e correzione suonano alla stessa intensità
    d.filter += -d.filter.bandLevel (200.0, 5000.0);

    d.predicted = measured + d.filter;
    return d;
}

// ------------------------------------------------------------------------
// FIR

int firLengthForSampleRate (double sampleRate)
{
    // ~170 ms: risoluzione sufficiente per i modi della stanza sotto i 50 Hz
    return 1 << nextPowerOfTwoOrder ((size_t) (sampleRate * 0.17));
}

std::vector<float> buildFir (const Response& filterDb, double sampleRate, int numTaps, bool linearPhase)
{
    const size_t N = (size_t) numTaps * 2;
    std::vector<std::complex<float>> spectrum (N);

    // ampiezza desiderata su ogni bin
    std::vector<double> magDb (N / 2 + 1);
    for (size_t k = 0; k <= N / 2; ++k)
    {
        const double hz = std::max (FreqGrid::fMin, (double) k * sampleRate / (double) N);
        magDb[k] = filterDb.at (std::min (hz, FreqGrid::fMax));
    }

    std::vector<float> fir ((size_t) numTaps, 0.0f);

    if (linearPhase)
    {
        for (size_t k = 0; k <= N / 2; ++k)
        {
            const float mag = (float) std::pow (10.0, magDb[k] / 20.0);
            spectrum[k] = mag;
            if (k > 0 && k < N / 2)
                spectrum[N - k] = mag;
        }

        fftInPlace (spectrum, true);

        // risposta a fase zero → centrata a numTaps/2, finestra di Hann
        const int half = numTaps / 2;
        for (int n = 0; n < numTaps; ++n)
        {
            const int src = (n - half + (int) N) % (int) N;
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) numTaps);
            fir[(size_t) n] = spectrum[(size_t) src].real() * w;
        }

        return fir;
    }

    // Fase minima tramite cepstrum reale
    for (size_t k = 0; k <= N / 2; ++k)
    {
        const float lnMag = (float) (magDb[k] * std::log (10.0) / 20.0);
        spectrum[k] = lnMag;
        if (k > 0 && k < N / 2)
            spectrum[N - k] = lnMag;
    }

    fftInPlace (spectrum, true);                     // cepstrum reale

    for (size_t n = 1; n < N / 2; ++n)
        spectrum[n] *= 2.0f;
    for (size_t n = N / 2 + 1; n < N; ++n)
        spectrum[n] = 0.0f;
    spectrum[0] = spectrum[0].real();
    spectrum[N / 2] = spectrum[N / 2].real();

    fftInPlace (spectrum, false);

    for (auto& v : spectrum)
        v = std::exp (v);

    fftInPlace (spectrum, true);

    const int fade = numTaps / 8;
    for (int n = 0; n < numTaps; ++n)
    {
        float w = 1.0f;
        if (n > numTaps - fade)
            w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (n - (numTaps - fade)) / (float) fade);
        fir[(size_t) n] = spectrum[(size_t) n].real() * w;
    }

    return fir;
}

} // namespace scriba
