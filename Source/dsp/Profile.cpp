#include "Profile.h"

namespace scriba
{

static juce::var responseToVar (const Response& r)
{
    juce::Array<juce::var> values;
    for (auto v : r.db)
        values.add (juce::roundToInt (v * 100.0f) / 100.0);
    return values;
}

static Response responseFromVar (const juce::var& v)
{
    Response r;
    if (auto* arr = v.getArray(); arr != nullptr && arr->size() == FreqGrid::numPoints)
        for (int i = 0; i < FreqGrid::numPoints; ++i)
            r.db[(size_t) i] = (float) (double) arr->getReference (i);
    return r;
}

juce::var Profile::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("format", "ScribaMonitorProfile");
    obj->setProperty ("version", 1);
    obj->setProperty ("name", name);
    obj->setProperty ("created", created.toISO8601 (true));
    obj->setProperty ("sampleRate", measuredSampleRate);
    obj->setProperty ("points", numPoints);
    obj->setProperty ("calibration", calibrationName);
    obj->setProperty ("gridPoints", FreqGrid::numPoints);
    obj->setProperty ("gridMinHz", FreqGrid::fMin);
    obj->setProperty ("gridMaxHz", FreqGrid::fMax);
    obj->setProperty ("left", responseToVar (left));
    obj->setProperty ("right", responseToVar (right));
    return juce::var (obj);
}

Profile Profile::fromVar (const juce::var& v)
{
    Profile p;
    if (v.getProperty ("format", {}).toString() != "ScribaMonitorProfile"
        || (int) v.getProperty ("gridPoints", 0) != FreqGrid::numPoints)
        return p;

    p.name = v.getProperty ("name", "Profilo").toString();
    p.created = juce::Time::fromISO8601 (v.getProperty ("created", {}).toString());
    p.measuredSampleRate = v.getProperty ("sampleRate", 48000.0);
    p.numPoints = v.getProperty ("points", 0);
    p.calibrationName = v.getProperty ("calibration", {}).toString();
    p.left  = responseFromVar (v.getProperty ("left", {}));
    p.right = responseFromVar (v.getProperty ("right", {}));
    return p;
}

bool Profile::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (toVar()));
}

Profile Profile::loadFrom (const juce::File& file)
{
    return fromVar (juce::JSON::parse (file));
}

juce::String Profile::getDisplayName() const
{
    const auto dot = juce::String::fromUTF8 (" \xc2\xb7 ");
    return name + dot + juce::String (numPoints) + " punti" + dot + created.formatted ("%d/%m/%Y");
}

juce::File Profile::getProfilesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Meteora Plugins")
               .getChildFile ("Scriba Monitor")
               .getChildFile ("Profiles");
}

} // namespace scriba
