#include "SweepAnalyzer.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace scriba
{

int nextPowerOfTwoOrder (size_t n)
{
    int order = 1;
    while (((size_t) 1 << order) < n)
        ++order;
    return order;
}

void fftInPlace (std::vector<std::complex<float>>& data, bool inverse)
{
    const int order = nextPowerOfTwoOrder (data.size());
    jassert (((size_t) 1 << order) == data.size());

    juce::dsp::FFT fft (order);
    std::vector<std::complex<float>> out (data.size());
    fft.perform (data.data(), out.data(), inverse);
    data.swap (out);
}

std::vector<float> generateLogSweep (const SweepSettings& s)
{
    const double f1 = s.startHz;
    const double f2 = std::min (s.endHz, 0.45 * s.sampleRate);
    const double T  = s.durationSeconds;
    const double L  = T / std::log (f2 / f1);

    const int numSamples = (int) std::round (T * s.sampleRate);
    std::vector<float> sweep ((size_t) numSamples);

    const int fadeIn  = (int) (0.05 * s.sampleRate);
    const int fadeOut = (int) (0.01 * s.sampleRate);

    for (int n = 0; n < numSamples; ++n)
    {
        const double t = (double) n / s.sampleRate;
        double v = std::sin (juce::MathConstants<double>::twoPi * f1 * L * (std::exp (t / L) - 1.0));

        if (n < fadeIn)
            v *= 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * n / fadeIn);
        else if (n > numSamples - fadeOut)
            v *= 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * (numSamples - n) / fadeOut);

        sweep[(size_t) n] = (float) (v * s.amplitude);
    }

    return sweep;
}

ImpulseResult deconvolveSweep (const std::vector<float>& recording,
                               const std::vector<float>& sweep,
                               const SweepSettings& s)
{
    ImpulseResult result;

    if (recording.empty() || sweep.empty())
    {
        result.error = "Registrazione vuota";
        return result;
    }

    const size_t N = (size_t) 1 << nextPowerOfTwoOrder (recording.size() + sweep.size());

    std::vector<std::complex<float>> R (N), S (N);
    for (size_t i = 0; i < recording.size(); ++i)  R[i] = recording[i];
    for (size_t i = 0; i < sweep.size(); ++i)      S[i] = sweep[i];

    fftInPlace (R, false);
    fftInPlace (S, false);

    float maxPower = 0.0f;
    for (auto& v : S)
        maxPower = std::max (maxPower, std::norm (v));

    const double f1 = s.startHz * 1.2;
    const double f2 = std::min (s.endHz, 0.45 * s.sampleRate) * 0.98;

    for (size_t k = 0; k < N; ++k)
    {
        const size_t kk = k <= N / 2 ? k : N - k;   // frequenza simmetrica
        const double hz = (double) kk * s.sampleRate / (double) N;

        const bool inBand = hz >= f1 && hz <= f2;
        const float eps = maxPower * (inBand ? 1.0e-6f : 1.0f);

        R[k] = R[k] * std::conj (S[k]) / (std::norm (S[k]) + eps);
    }

    fftInPlace (R, true);

    result.ir.resize (N);
    for (size_t i = 0; i < N; ++i)
        result.ir[i] = R[i].real();

    // il picco va cercato solo nella parte causale plausibile
    const size_t searchEnd = std::min (N, recording.size());
    size_t peak = 0;
    float peakValue = 0.0f;

    for (size_t i = 0; i < searchEnd; ++i)
    {
        const float a = std::abs (result.ir[i]);
        if (a > peakValue) { peakValue = a; peak = i; }
    }

    if (peakValue <= 1.0e-9f)
    {
        result.error = "Nessuna risposta rilevata: controlla microfono e uscite";
        return result;
    }

    result.peakIndex = (int) peak;
    result.valid = true;
    return result;
}

Response responseFromImpulse (const ImpulseResult& impulse, double sampleRate,
                              double windowMs, double preWindowMs)
{
    Response response;
    if (! impulse.valid)
        return response;

    const int pre  = (int) (preWindowMs * 0.001 * sampleRate);
    const int post = (int) (windowMs * 0.001 * sampleRate);
    const int start = impulse.peakIndex - pre;
    const int length = pre + post;

    const size_t N = (size_t) 1 << std::max (15, nextPowerOfTwoOrder ((size_t) length * 2));
    std::vector<std::complex<float>> X (N);

    const int fallLength = std::max (1, post / 5);

    for (int i = 0; i < length; ++i)
    {
        const int src = start + i;
        if (src < 0 || src >= (int) impulse.ir.size())
            continue;

        float w = 1.0f;
        if (i < pre)
            w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) pre);
        else if (i > length - fallLength)
            w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (i - (length - fallLength)) / (float) fallLength);

        X[(size_t) i] = impulse.ir[(size_t) src] * w;
    }

    fftInPlace (X, false);

    const double binHz = sampleRate / (double) N;
    const size_t maxBin = N / 2;

    auto powerAtBin = [&X] (size_t k) { return (double) std::norm (X[k]); };

    for (int i = 0; i < FreqGrid::numPoints; ++i)
    {
        const double fLo = FreqGrid::freq (i) * std::pow (2.0, -0.5 * std::log2 (FreqGrid::freq (1) / FreqGrid::freq (0)));
        const double fHi = FreqGrid::freq (i) * std::pow (2.0,  0.5 * std::log2 (FreqGrid::freq (1) / FreqGrid::freq (0)));

        const size_t kLo = (size_t) std::ceil (fLo / binHz);
        const size_t kHi = std::min (maxBin, (size_t) std::floor (fHi / binHz));

        double power = 0.0;

        if (kHi >= kLo && kLo > 0)
        {
            for (size_t k = kLo; k <= kHi; ++k)
                power += powerAtBin (k);
            power /= (double) (kHi - kLo + 1);
        }
        else
        {
            // cella più stretta di un bin: interpolazione tra i due bin vicini
            const double pos = FreqGrid::freq (i) / binHz;
            const size_t k0 = std::min (maxBin - 1, (size_t) pos);
            const double t = std::clamp (pos - (double) k0, 0.0, 1.0);
            power = powerAtBin (std::max ((size_t) 1, k0)) * (1.0 - t) + powerAtBin (k0 + 1) * t;
        }

        response.db[(size_t) i] = (float) (10.0 * std::log10 (std::max (power, 1.0e-20)));
    }

    return response;
}

LevelStats measureLevel (const float* data, int numSamples)
{
    LevelStats stats;
    double sumSquares = 0.0;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float a = std::abs (data[i]);
        peak = std::max (peak, a);
        sumSquares += (double) data[i] * data[i];
        if (std::abs (data[i]) > 0.0f)
            stats.silent = false;
    }

    stats.clipped = peak >= 0.999f;
    stats.peakDbfs = juce::Decibels::gainToDecibels (peak, -120.0f);
    stats.rmsDbfs  = juce::Decibels::gainToDecibels ((float) std::sqrt (sumSquares / std::max (1, numSamples)), -120.0f);
    return stats;
}

} // namespace scriba
