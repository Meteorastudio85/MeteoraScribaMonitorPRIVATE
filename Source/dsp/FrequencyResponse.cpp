#include "FrequencyResponse.h"

#include <cmath>
#include <algorithm>

namespace scriba
{

static const double logMin  = std::log (FreqGrid::fMin);
static const double logSpan = std::log (FreqGrid::fMax) - std::log (FreqGrid::fMin);

double FreqGrid::freq (int index)
{
    return std::exp (logMin + logSpan * (double) index / (double) (numPoints - 1));
}

double FreqGrid::position (double hz)
{
    hz = std::max (hz, 1.0e-3);
    return (std::log (hz) - logMin) / logSpan * (double) (numPoints - 1);
}

const std::vector<double>& FreqGrid::freqs()
{
    static const std::vector<double> f = []
    {
        std::vector<double> v ((size_t) numPoints);
        for (int i = 0; i < numPoints; ++i)
            v[(size_t) i] = freq (i);
        return v;
    }();
    return f;
}

float Response::at (double hz) const
{
    const auto pos = FreqGrid::position (hz);
    if (pos <= 0.0)                                 return db.front();
    if (pos >= (double) (FreqGrid::numPoints - 1))  return db.back();

    const auto i0 = (int) pos;
    const auto frac = (float) (pos - (double) i0);
    return db[(size_t) i0] + frac * (db[(size_t) i0 + 1] - db[(size_t) i0]);
}

Response Response::fromFunction (const std::function<double (double)>& dbOfHz)
{
    Response r;
    for (int i = 0; i < FreqGrid::numPoints; ++i)
        r.db[(size_t) i] = (float) dbOfHz (FreqGrid::freq (i));
    return r;
}

Response Response::smoothed (double fraction, double fractionHigh, double transitionHz) const
{
    Response out;
    const double pointsPerOctave = (double) (FreqGrid::numPoints - 1) / (logSpan / std::log (2.0));

    std::vector<double> power ((size_t) FreqGrid::numPoints);
    for (size_t i = 0; i < power.size(); ++i)
        power[i] = std::pow (10.0, db[i] / 10.0);

    for (int i = 0; i < FreqGrid::numPoints; ++i)
    {
        double frac = fraction;

        if (fractionHigh > 0.0)
        {
            // passaggio graduale (su un'ottava) tra le due risoluzioni
            const double octAbove = std::log2 (FreqGrid::freq (i) / transitionHz);
            const double t = std::clamp (octAbove + 0.5, 0.0, 1.0);
            frac = fraction + t * (fractionHigh - fraction);
        }

        const int half = std::max (0, (int) std::round (pointsPerOctave / frac * 0.5));
        const int a = std::max (0, i - half);
        const int b = std::min (FreqGrid::numPoints - 1, i + half);

        double sum = 0.0;
        for (int k = a; k <= b; ++k)
            sum += power[(size_t) k];

        out.db[(size_t) i] = (float) (10.0 * std::log10 (std::max (sum / (double) (b - a + 1), 1.0e-20)));
    }

    return out;
}

Response Response::powerAverage (const std::vector<Response>& responses)
{
    Response out;
    if (responses.empty())
        return out;

    for (int i = 0; i < FreqGrid::numPoints; ++i)
    {
        double sum = 0.0;
        for (auto& r : responses)
            sum += std::pow (10.0, r.db[(size_t) i] / 10.0);

        out.db[(size_t) i] = (float) (10.0 * std::log10 (std::max (sum / (double) responses.size(), 1.0e-20)));
    }

    return out;
}

float Response::bandLevel (double lowHz, double highHz) const
{
    double sum = 0.0;
    int count = 0;

    for (int i = 0; i < FreqGrid::numPoints; ++i)
    {
        const auto f = FreqGrid::freq (i);
        if (f >= lowHz && f <= highHz)
        {
            sum += std::pow (10.0, db[(size_t) i] / 10.0);
            ++count;
        }
    }

    return count > 0 ? (float) (10.0 * std::log10 (std::max (sum / count, 1.0e-20))) : 0.0f;
}

Response& Response::operator+= (const Response& other)
{
    for (size_t i = 0; i < db.size(); ++i)
        db[i] += other.db[i];
    return *this;
}

Response& Response::operator-= (const Response& other)
{
    for (size_t i = 0; i < db.size(); ++i)
        db[i] -= other.db[i];
    return *this;
}

Response& Response::operator+= (float gainDb)
{
    for (auto& v : db)
        v += gainDb;
    return *this;
}

Response operator+ (Response a, const Response& b)  { a += b; return a; }
Response operator- (Response a, const Response& b)  { a -= b; return a; }

} // namespace scriba
