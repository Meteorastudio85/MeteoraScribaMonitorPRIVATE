#include "MicCalibration.h"

#if ! SCRIBA_NO_BINARY_DATA
 #include "BinaryData.h"
#endif

#include <cmath>
#include <algorithm>

namespace scriba
{

MicCalibration MicCalibration::defaultECM8000()
{
   #if SCRIBA_NO_BINARY_DATA
    auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("Resources/ECM8000_generic.cal");
    return fromText (file.loadFileAsString(), "Behringer ECM8000 (generica)");
   #else
    return fromText (juce::String::createStringFromData (BinaryData::ECM8000_generic_cal,
                                                         BinaryData::ECM8000_generic_calSize),
                     "Behringer ECM8000 (generica)");
   #endif
}

MicCalibration MicCalibration::fromFile (const juce::File& file)
{
    return fromText (file.loadFileAsString(), file.getFileName());
}

MicCalibration MicCalibration::fromText (const juce::String& text, const juce::String& calName)
{
    std::vector<std::pair<double, double>> points;

    for (auto line : juce::StringArray::fromLines (text))
    {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar ('*') || line.startsWithChar ('"') || line.startsWithChar ('#'))
            continue;

        auto tokens = juce::StringArray::fromTokens (line.replaceCharacter (',', ' ')
                                                         .replaceCharacter (';', ' ')
                                                         .replaceCharacter ('\t', ' '), " ", "");
        tokens.removeEmptyStrings();

        if (tokens.size() < 2 || ! tokens[0].containsOnly ("0123456789.eE+-"))
            continue;

        const auto f = tokens[0].getDoubleValue();
        const auto d = tokens[1].getDoubleValue();

        if (f > 0.0 && std::isfinite (d))
            points.emplace_back (f, d);
    }

    MicCalibration cal;
    cal.name = calName;

    if (points.size() < 2)
        return cal;

    std::sort (points.begin(), points.end());

    cal.response = Response::fromFunction ([&points] (double hz)
    {
        if (hz <= points.front().first)  return points.front().second;
        if (hz >= points.back().first)   return points.back().second;

        auto it = std::lower_bound (points.begin(), points.end(), std::make_pair (hz, -1.0e9));
        auto prev = std::prev (it);
        const double t = (std::log (hz) - std::log (prev->first))
                       / (std::log (it->first) - std::log (prev->first));
        return prev->second + t * (it->second - prev->second);
    });

    cal.valid = true;
    return cal;
}

} // namespace scriba
