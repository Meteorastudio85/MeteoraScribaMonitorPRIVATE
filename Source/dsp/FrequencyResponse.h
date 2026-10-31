#pragma once

#include <vector>
#include <functional>

namespace scriba
{

// Griglia di frequenze logaritmica comune a tutto il plugin:
// misure, calibrazione, target, emulazioni e filtri vivono su questa griglia.
struct FreqGrid
{
    static constexpr int numPoints = 400;
    static constexpr double fMin = 10.0;
    static constexpr double fMax = 24000.0;

    static double freq (int index);                 // Hz del punto i
    static double position (double hz);             // indice frazionario per una frequenza
    static const std::vector<double>& freqs();
};

// Risposta in ampiezza (dB) sulla griglia logaritmica.
struct Response
{
    std::vector<float> db = std::vector<float> (FreqGrid::numPoints, 0.0f);

    float at (double hz) const;                     // interpolazione lineare in log-f
    bool isEmpty() const                            { return db.empty(); }

    static Response fromFunction (const std::function<double (double hz)>& dbOfHz);

    // Smoothing a frazione di ottava (media di potenza).
    // fraction = 6 → 1/6 di ottava. Se fractionHigh > 0, lo smoothing passa
    // gradualmente a 1/fractionHigh sopra 'transitionHz'.
    Response smoothed (double fraction, double fractionHigh = 0.0, double transitionHz = 1000.0) const;

    // Media in potenza di più risposte (media spaziale tra i punti di misura).
    static Response powerAverage (const std::vector<Response>& responses);

    // Livello medio (in dB, media di potenza) in una banda.
    float bandLevel (double lowHz, double highHz) const;

    Response& operator+= (const Response& other);
    Response& operator-= (const Response& other);
    Response& operator+= (float gainDb);
};

Response operator+ (Response a, const Response& b);
Response operator- (Response a, const Response& b);

} // namespace scriba
