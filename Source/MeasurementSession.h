#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include "dsp/SweepAnalyzer.h"
#include "dsp/MicCalibration.h"
#include "dsp/Profile.h"

#include <atomic>
#include <vector>

// Sessione di misura (solo standalone).
// Il thread audio riproduce i segnali di test e registra il microfono;
// l'analisi gira su un thread separato; la GUI interroga lo stato con un timer.
class MeasurementSession : private juce::Timer
{
public:
    enum class State { idle, levelCheck, measuringLeft, measuringRight, analysing };

    enum class LevelStatus { notChecked, ok, tooLow, tooHigh, clipping, noSignal, noisy };

    struct LevelResult
    {
        LevelStatus status = LevelStatus::notChecked;
        float peakDbfs = -120.0f;
        float snrDb = 0.0f;
        juce::String message { "Esegui il check del livello" };
        bool allowed = false;                   // si può procedere con la misura
    };

    struct PointResult
    {
        bool done = false;
        scriba::Response left, right;        // calibrate, non smussate
    };

    MeasurementSession();
    ~MeasurementSession() override;

    // --- thread audio -----------------------------------------------------
    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>& buffer);   // sostituisce l'uscita con i segnali di test

    // --- thread messaggi --------------------------------------------------
    void startLevelCheck();
    void measureCurrentPoint();
    void cancel();
    void resetPoints();
    void selectPoint (int index);

    State getState() const                      { return state.load(); }
    bool isBusy() const                         { return getState() != State::idle; }
    float getProgress() const;                  // 0..1 della fase corrente
    float getInputPeakDbfs();                   // picco live del microfono (con decadimento)

    const LevelResult& getLevelResult() const   { return levelResult; }
    const std::vector<PointResult>& getPoints() const { return points; }
    int getCurrentPoint() const                 { return currentPoint; }
    int getNumPointsDone() const;
    bool allPointsDone() const                  { return getNumPointsDone() == (int) points.size(); }
    juce::String getLastError() const          { return lastError; }
    int getDetectedLatency() const              { return detectedLatency; }
    double getSampleRate() const                { return sampleRate; }

    scriba::Response getAverage (bool rightChannel) const;
    scriba::Profile buildProfile (const juce::String& name) const;

    // impostazioni
    void setNumPoints (int num);
    int getNumPoints() const                    { return (int) points.size(); }
    void setMicChannel (int channel)            { micChannel = juce::jlimit (0, 1, channel); }
    int getMicChannel() const                   { return micChannel.load(); }
    void setOutputLevelDb (float db)            { outputLevelDb = juce::jlimit (-40.0f, -6.0f, db); }
    float getOutputLevelDb() const              { return outputLevelDb; }

    void setCalibration (const scriba::MicCalibration& cal) { calibration = cal; }
    const scriba::MicCalibration& getCalibration() const   { return calibration; }

    std::function<void()> onChange;             // chiamata sul thread messaggi

private:
    void timerCallback() override;
    void startPlayback (State newState, std::vector<float> signal, int outputChannel);
    void finishLevelCheck();
    void finishChannel (bool right);
    void notify()                               { if (onChange) onChange(); }

    scriba::SweepSettings makeSweepSettings (double durationSeconds) const;

    double sampleRate = 48000.0;
    std::atomic<State> state { State::idle };

    // buffer condivisi con il thread audio (allocati prima di partire,
    // scambiati solo sotto bufferLock; il thread audio usa solo tryLock)
    juce::SpinLock bufferLock;
    std::vector<float> playSignal, recording;
    std::atomic<int> playPosition { 0 };
    std::atomic<int> outputChannel { 0 };
    std::atomic<bool> phaseComplete { false };
    std::atomic<int> micChannel { 0 };
    std::atomic<float> inputPeak { 0.0f };

    std::vector<float> currentSweep;
    std::vector<float> recordingLeft;
    int silenceBefore = 0;

    float outputLevelDb = -20.0f;
    float displayedPeak = -120.0f;
    LevelResult levelResult;
    std::vector<PointResult> points;
    int currentPoint = 0;
    int detectedLatency = 0;
    juce::String lastError;
    scriba::MicCalibration calibration;

    // risultato dell'analisi (scritto dal thread di analisi, letto dal timer)
    juce::CriticalSection analysisLock;
    bool analysisReady = false;
    PointResult analysisResult;
    juce::String analysisError;
    int analysisLatency = 0;

    juce::ThreadPool analysisPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeasurementSession)
};
