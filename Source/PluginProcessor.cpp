#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/MicCalibration.h"

using namespace scriba;

ScribaMonitorProcessor::ScribaMonitorProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ScribaMonitor", createLayout()),
      convolution (juce::dsp::Convolution::NonUniform { 512 })
{
    for (auto* id : { ParamIDs::target, ParamIDs::tilt, ParamIDs::bass, ParamIDs::treble, ParamIDs::phase,
                      ParamIDs::maxBoost, ParamIDs::range, ParamIDs::emulation, ParamIDs::emulationOn })
        apvts.addParameterListener (id, this);

    // carica il profilo più recente, se esiste (lo stato della sessione DAW lo sostituisce)
    auto files = Profile::getProfilesFolder().findChildFiles (juce::File::findFiles, false,
                                                              juce::String ("*") + Profile::fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getLastModificationTime() > b.getLastModificationTime(); });

    if (! files.isEmpty())
    {
        auto p = Profile::loadFrom (files.getFirst());
        if (p.isValid())
            loadProfile (p);
    }

    startTimerHz (20);
}

ScribaMonitorProcessor::~ScribaMonitorProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout ScribaMonitorProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbAttr = AudioParameterFloatAttributes().withLabel ("dB");

    juce::StringArray emulationNames;
    for (auto& e : getEmulations())
        emulationNames.add (e.name);

    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::target, 1 }, "Target", getTargetNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::tilt, 1 }, "Tilt",
                                                        NormalisableRange<float> (-1.5f, 1.5f, 0.05f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB/oct")));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::bass, 1 }, "Bassi",
                                                        NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::treble, 1 }, "Acuti",
                                                        NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::mix, 1 }, "Dry/Wet",
                                                        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::output, 1 }, "Output",
                                                        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::phase, 1 }, "Fase", getPhaseNames(), 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { ParamIDs::maxBoost, 1 }, "Boost max",
                                                        NormalisableRange<float> (0.0f, 12.0f, 0.5f), 6.0f, dbAttr));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::range, 1 }, "Range", getRangeNames(), 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::emulation, 1 }, "Emulazione", emulationNames, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::emulationOn, 1 }, "Emulazione attiva", true));

    return layout;
}

bool ScribaMonitorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ScribaMonitorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    session.prepare (sampleRate);

    const auto channels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, channels };

    convolution.prepare (spec);
    dryDelayBuffer.setSize ((int) channels, dryDelaySize);
    dryDelayBuffer.clear();
    dryWritePos = 0;
    dryBuffer.setSize ((int) channels, samplesPerBlock);

    wetGain.reset (sampleRate, 0.05);
    outputGain.reset (sampleRate, 0.05);
    wetGain.setCurrentAndTargetValue (0.0f);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::output)->load()));

    filterDirty = true;   // il FIR dipende dalla frequenza di campionamento
}

void ScribaMonitorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (isStandalone())
    {
        // lo standalone serve per la misura: in uscita solo i segnali di test
        session.process (buffer);
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) dryBuffer.getNumChannels());

    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (dryBuffer.getNumChannels(), numSamples, false, false, true);

    // ramo dry, ritardato della stessa latenza del FIR (fase lineare)
    const int delay = juce::jlimit (0, dryDelaySize - 1, currentLatency.load());
    constexpr int mask = dryDelaySize - 1;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* src = buffer.getReadPointer (ch);
        auto* dst = dryBuffer.getWritePointer (ch);
        auto* ring = dryDelayBuffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const int w = (dryWritePos + i) & mask;
            ring[w] = src[i];
            dst[i] = ring[(w - delay) & mask];
        }
    }
    dryWritePos = (dryWritePos + numSamples) & mask;

    const bool bypassed = apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    const bool filterReady = hasFilter.load();

    wetGain.setTargetValue (bypassed || ! filterReady ? 0.0f
                                                      : apvts.getRawParameterValue (ParamIDs::mix)->load() / 100.0f);
    outputGain.setTargetValue (bypassed ? 1.0f
                                        : juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::output)->load()));

    if (filterReady)
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
        convolution.process (juce::dsp::ProcessContextReplacing<float> (block));
    }
    else
    {
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const float w = wetGain.getNextValue();
        const float g = outputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* out = buffer.getWritePointer (ch);
            out[i] = (out[i] * w + dryBuffer.getSample (ch, i) * (1.0f - w)) * g;
        }
    }

    for (int ch = 0; ch < juce::jmin (numChannels, 2); ++ch)
    {
        const float peak = buffer.getMagnitude (ch, 0, numSamples);
        float previous = outputPeak[ch].load();
        while (peak > previous && ! outputPeak[ch].compare_exchange_weak (previous, peak)) {}
    }
}

float ScribaMonitorProcessor::getOutputPeakDb (int channel)
{
    return juce::Decibels::gainToDecibels (outputPeak[juce::jlimit (0, 1, channel)].exchange (0.0f), -90.0f);
}

// ---------------------------------------------------------------------------
// Profilo e filtro

void ScribaMonitorProcessor::loadProfile (const Profile& newProfile)
{
    profile = newProfile;

    if (profile.isValid())
    {
        preparedMeasurement[0] = prepareMeasurement (profile.left);
        preparedMeasurement[1] = prepareMeasurement (profile.right);

        // bilanciamento: abbassa il monitor più forte (misurato tra 200 Hz e 2 kHz)
        const float diff = profile.left.bandLevel (200.0, 2000.0) - profile.right.bandLevel (200.0, 2000.0);
        const float clamped = juce::jlimit (-6.0f, 6.0f, diff);
        balanceDb[0] = clamped > 0.0f ? -clamped : 0.0f;
        balanceDb[1] = clamped < 0.0f ?  clamped : 0.0f;
    }

    filterDirty = true;
}

bool ScribaMonitorProcessor::loadCustomTarget (const juce::File& file)
{
    auto curve = MicCalibration::fromFile (file);
    if (! curve.isValid())
        return false;

    customTarget = curve.getResponse();
    customTarget += -customTarget.bandLevel (200.0, 2000.0);
    customTargetFile = file;
    filterDirty = true;
    return true;
}

CorrectionSettings ScribaMonitorProcessor::readSettings() const
{
    auto value = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    CorrectionSettings s;
    s.target = (TargetType) juce::roundToInt (value (ParamIDs::target));
    s.customTarget = customTarget;
    s.tiltDbPerOctave = value (ParamIDs::tilt);
    s.bassDb = value (ParamIDs::bass);
    s.trebleDb = value (ParamIDs::treble);
    s.maxBoostDb = value (ParamIDs::maxBoost);
    s.range = (CorrectionRange) juce::roundToInt (value (ParamIDs::range));
    s.emulation = juce::roundToInt (value (ParamIDs::emulation));
    s.emulationEnabled = value (ParamIDs::emulationOn) > 0.5f;
    s.linearPhase = juce::roundToInt (value (ParamIDs::phase)) == 1;

    if (s.target == TargetType::custom && customTarget.isEmpty())
        s.target = TargetType::flat;

    return s;
}

void ScribaMonitorProcessor::timerCallback()
{
    if (filterDirty.exchange (false))
        rebuildFilter();
}

void ScribaMonitorProcessor::rebuildFilter()
{
    const auto settings = readSettings();
    int latency = 0;

    if (! profile.isValid())
    {
        hasFilter = false;
        designs[0] = designs[1] = {};
    }
    else
    {
        const int taps = firLengthForSampleRate (currentSampleRate);
        juce::AudioBuffer<float> ir (2, taps);

        for (int ch = 0; ch < 2; ++ch)
        {
            designs[ch] = designCorrection (preparedMeasurement[ch], settings);
            designs[ch].filter += balanceDb[ch];

            const auto fir = buildFir (designs[ch].filter, currentSampleRate, taps, settings.linearPhase);
            ir.copyFrom (ch, 0, fir.data(), taps);
        }

        convolution.loadImpulseResponse (std::move (ir), currentSampleRate,
                                         juce::dsp::Convolution::Stereo::yes,
                                         juce::dsp::Convolution::Trim::no,
                                         juce::dsp::Convolution::Normalise::no);

        latency = settings.linearPhase ? taps / 2 : 0;
        hasFilter = true;
    }

    currentLatency = latency;
    if (latency != lastRequestedLatency)
    {
        lastRequestedLatency = latency;
        setLatencySamples (latency);
    }

    ++designVersion;
}

// ---------------------------------------------------------------------------
// Stato

void ScribaMonitorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::ValueTree data ("ScribaData");
    if (profile.isValid())
        data.setProperty ("profile", juce::JSON::toString (profile.toVar(), true), nullptr);
    data.setProperty ("customTarget", customTargetFile.getFullPathName(), nullptr);
    data.setProperty ("micChannel", session.getMicChannel(), nullptr);
    data.setProperty ("outputLevel", session.getOutputLevelDb(), nullptr);

    state.removeChild (state.getChildWithName ("ScribaData"), nullptr);
    state.appendChild (data, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ScribaMonitorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto extra = state.getChildWithName ("ScribaData");
    state.removeChild (extra, nullptr);
    apvts.replaceState (state);

    if (extra.isValid())
    {
        auto p = Profile::fromVar (juce::JSON::parse (extra.getProperty ("profile").toString()));
        if (p.isValid())
            loadProfile (p);

        const auto targetPath = extra.getProperty ("customTarget").toString();
        if (juce::File::isAbsolutePath (targetPath) && juce::File (targetPath).existsAsFile())
            loadCustomTarget (juce::File (targetPath));

        session.setMicChannel (extra.getProperty ("micChannel", 0));
        session.setOutputLevelDb (extra.getProperty ("outputLevel", -20.0f));
    }

    filterDirty = true;
}

juce::AudioProcessorEditor* ScribaMonitorProcessor::createEditor()
{
    return new ScribaMonitorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScribaMonitorProcessor();
}
