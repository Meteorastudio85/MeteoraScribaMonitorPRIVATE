#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "MeasurementSession.h"
#include "dsp/FilterDesigner.h"
#include "dsp/Profile.h"

namespace ParamIDs
{
    inline constexpr const char* bypass      = "bypass";
    inline constexpr const char* target      = "target";
    inline constexpr const char* tilt        = "tilt";
    inline constexpr const char* bass        = "bass";
    inline constexpr const char* treble      = "treble";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
    inline constexpr const char* phase       = "phase";
    inline constexpr const char* maxBoost    = "maxBoost";
    inline constexpr const char* range       = "range";
    inline constexpr const char* emulation   = "emulation";
    inline constexpr const char* emulationOn = "emulationOn";
}

class ScribaMonitorProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::Timer
{
public:
    ScribaMonitorProcessor();
    ~ScribaMonitorProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.2; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- API per la GUI (thread messaggi) --------------------------------
    bool isStandalone() const           { return wrapperType == wrapperType_Standalone; }

    void loadProfile (const scriba::Profile& profile);
    const scriba::Profile& getProfile() const   { return profile; }

    bool loadCustomTarget (const juce::File& file);
    juce::String getCustomTargetName() const    { return customTargetFile.getFileNameWithoutExtension(); }

    // progetto corrente per canale (0 = L, 1 = R); incrementa a ogni ricalcolo
    const scriba::CorrectionDesign& getDesign (int channel) const { return designs[juce::jlimit (0, 1, channel)]; }
    int getDesignVersion() const                { return designVersion; }
    float getBalanceDb (int channel) const      { return balanceDb[juce::jlimit (0, 1, channel)]; }

    MeasurementSession& getSession()            { return session; }
    juce::AudioProcessorValueTreeState& getState() { return apvts; }

    float getOutputPeakDb (int channel);

    static juce::StringArray getTargetNames()    { return { "Flat", "Studio", "Personalizzata" }; }
    static juce::StringArray getRangeNames()     { return { "Fino a 300 Hz", "Fino a 1 kHz", "Full range" }; }
    static juce::StringArray getPhaseNames()     { return { "Fase minima", "Fase lineare" }; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override   { filterDirty = true; }
    void timerCallback() override;
    void rebuildFilter();
    scriba::CorrectionSettings readSettings() const;

    juce::AudioProcessorValueTreeState apvts;
    MeasurementSession session;

    scriba::Profile profile;
    scriba::Response preparedMeasurement[2];
    float balanceDb[2] { 0.0f, 0.0f };

    scriba::Response customTarget;
    juce::File customTargetFile;

    scriba::CorrectionDesign designs[2];
    int designVersion = 0;
    std::atomic<bool> filterDirty { true };

    // --- audio -----------------------------------------------------------
    juce::dsp::Convolution convolution;
    // ritardo del ramo dry (buffer circolare, potenza di 2)
    static constexpr int dryDelaySize = 1 << 17;
    juce::AudioBuffer<float> dryDelayBuffer, dryBuffer;
    int dryWritePos = 0;
    juce::SmoothedValue<float> wetGain, outputGain;
    std::atomic<bool> hasFilter { false };
    std::atomic<int> currentLatency { 0 };
    std::atomic<float> outputPeak[2] { { 0.0f }, { 0.0f } };
    double currentSampleRate = 48000.0;
    int lastRequestedLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScribaMonitorProcessor)
};
