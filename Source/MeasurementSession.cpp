#include "MeasurementSession.h"

using namespace scriba;

namespace
{
    constexpr double levelNoiseSeconds  = 2.0;   // silenzio: rumore di fondo
    constexpr double levelSweepSeconds  = 2.0;   // sweep breve per il livello
    constexpr double measureSweepSeconds = 6.0;
    constexpr double preSilenceSeconds  = 0.2;
    constexpr double tailSeconds        = 1.5;   // coda: latenza + riverbero della stanza
}

MeasurementSession::MeasurementSession()
{
    calibration = MicCalibration::defaultECM8000();
    setNumPoints (9);
    startTimerHz (30);
}

MeasurementSession::~MeasurementSession()
{
    stopTimer();
    analysisPool.removeAllJobs (true, 10000);
}

void MeasurementSession::prepare (double newSampleRate)
{
    if (! juce::approximatelyEqual (newSampleRate, sampleRate) && isBusy())
        juce::MessageManager::callAsync ([this] { cancel(); });

    sampleRate = newSampleRate;
}

// ---------------------------------------------------------------------------
// Thread audio

void MeasurementSession::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0)
        return;

    const int mic = juce::jmin (micChannel.load(), numChannels - 1);
    const float* micData = buffer.getReadPointer (mic);

    // picco live del microfono per il meter
    {
        const float blockPeak = juce::FloatVectorOperations::findMaximum (micData, numSamples);
        const float blockMin  = juce::FloatVectorOperations::findMinimum (micData, numSamples);
        const float peak = juce::jmax (blockPeak, -blockMin);
        float previous = inputPeak.load();
        while (peak > previous && ! inputPeak.compare_exchange_weak (previous, peak)) {}
    }

    const auto st = state.load();
    const bool playing = (st == State::levelCheck || st == State::measuringLeft || st == State::measuringRight)
                         && ! phaseComplete.load();

    if (! playing)
    {
        buffer.clear();
        return;
    }

    const juce::SpinLock::ScopedTryLockType lock (bufferLock);
    if (! lock.isLocked())
    {
        buffer.clear();
        return;
    }

    const int pos = playPosition.load();
    const int total = (int) playSignal.size();
    const int n = juce::jmin (numSamples, total - pos);

    if (n > 0)
        std::copy (micData, micData + n, recording.begin() + pos);

    buffer.clear();

    const int out = juce::jmin (outputChannel.load(), numChannels - 1);
    if (n > 0)
        buffer.copyFrom (out, 0, playSignal.data() + pos, n);

    playPosition = pos + juce::jmax (0, n);

    if (pos + n >= total)
        phaseComplete = true;
}

// ---------------------------------------------------------------------------
// Thread messaggi

SweepSettings MeasurementSession::makeSweepSettings (double durationSeconds) const
{
    SweepSettings s;
    s.sampleRate = sampleRate;
    s.durationSeconds = durationSeconds;
    s.amplitude = juce::Decibels::decibelsToGain (outputLevelDb);
    return s;
}

void MeasurementSession::startPlayback (State newState, std::vector<float> signal, int channel)
{
    {
        const juce::SpinLock::ScopedLockType lock (bufferLock);
        playSignal = std::move (signal);
        recording.assign (playSignal.size(), 0.0f);
        playPosition = 0;
        outputChannel = channel;
        phaseComplete = false;
    }

    state = newState;
    notify();
}

void MeasurementSession::startLevelCheck()
{
    if (isBusy())
        return;

    lastError.clear();

    const int noise = (int) (levelNoiseSeconds * sampleRate);
    const int tail = (int) (0.5 * sampleRate);

    currentSweep = generateLogSweep (makeSweepSettings (levelSweepSeconds));

    std::vector<float> signal ((size_t) noise, 0.0f);
    signal.insert (signal.end(), currentSweep.begin(), currentSweep.end());
    signal.resize (signal.size() + (size_t) tail, 0.0f);

    silenceBefore = noise;
    startPlayback (State::levelCheck, std::move (signal), 0);
}

void MeasurementSession::measureCurrentPoint()
{
    if (isBusy() || ! levelResult.allowed)
        return;

    lastError.clear();

    currentSweep = generateLogSweep (makeSweepSettings (measureSweepSeconds));
    silenceBefore = (int) (preSilenceSeconds * sampleRate);

    std::vector<float> signal ((size_t) silenceBefore, 0.0f);
    signal.insert (signal.end(), currentSweep.begin(), currentSweep.end());
    signal.resize (signal.size() + (size_t) (tailSeconds * sampleRate), 0.0f);

    startPlayback (State::measuringLeft, std::move (signal), 0);
}

void MeasurementSession::cancel()
{
    {
        const juce::SpinLock::ScopedLockType lock (bufferLock);
        phaseComplete = true;
    }
    state = State::idle;
    notify();
}

void MeasurementSession::resetPoints()
{
    if (isBusy())
        return;

    for (auto& p : points)
        p = {};

    currentPoint = 0;
    notify();
}

void MeasurementSession::selectPoint (int index)
{
    if (! isBusy() && juce::isPositiveAndBelow (index, (int) points.size()))
    {
        currentPoint = index;
        notify();
    }
}

void MeasurementSession::setNumPoints (int num)
{
    if (isBusy())
        return;

    points.assign ((size_t) juce::jlimit (1, 9, num), {});
    currentPoint = 0;
    notify();
}

int MeasurementSession::getNumPointsDone() const
{
    int n = 0;
    for (auto& p : points)
        if (p.done) ++n;
    return n;
}

float MeasurementSession::getProgress() const
{
    const auto total = playSignal.size();
    return total > 0 ? juce::jlimit (0.0f, 1.0f, (float) playPosition.load() / (float) total) : 0.0f;
}

float MeasurementSession::getInputPeakDbfs()
{
    const float peak = inputPeak.exchange (0.0f);
    const float db = juce::Decibels::gainToDecibels (peak, -120.0f);

    // tenuta veloce, discesa lenta
    displayedPeak = db > displayedPeak ? db : juce::jmax (db, displayedPeak - 1.5f);
    return displayedPeak;
}

Response MeasurementSession::getAverage (bool rightChannel) const
{
    std::vector<Response> done;
    for (auto& p : points)
        if (p.done)
            done.push_back (rightChannel ? p.right : p.left);

    return done.empty() ? Response() : Response::powerAverage (done);
}

Profile MeasurementSession::buildProfile (const juce::String& name) const
{
    Profile profile;
    profile.name = name.isNotEmpty() ? name : juce::String ("Profilo");
    profile.created = juce::Time::getCurrentTime();
    profile.measuredSampleRate = sampleRate;
    profile.numPoints = getNumPointsDone();
    profile.calibrationName = calibration.getName();
    profile.left = getAverage (false);
    profile.right = getAverage (true);
    return profile;
}

// ---------------------------------------------------------------------------

void MeasurementSession::timerCallback()
{
    const auto st = state.load();

    if (st == State::analysing)
    {
        const juce::ScopedLock sl (analysisLock);
        if (analysisReady)
        {
            analysisReady = false;
            state = State::idle;

            if (analysisError.isNotEmpty())
            {
                lastError = analysisError;
            }
            else
            {
                detectedLatency = analysisLatency;
                points[(size_t) currentPoint] = analysisResult;

                // passa al prossimo punto non ancora misurato
                for (int i = 1; i <= (int) points.size(); ++i)
                {
                    const int next = (currentPoint + i) % (int) points.size();
                    if (! points[(size_t) next].done) { currentPoint = next; break; }
                }
            }

            notify();
        }
        return;
    }

    if (! phaseComplete.load())
        return;

    if (st == State::levelCheck)        finishLevelCheck();
    else if (st == State::measuringLeft)  finishChannel (false);
    else if (st == State::measuringRight) finishChannel (true);
}

void MeasurementSession::finishLevelCheck()
{
    std::vector<float> rec;
    {
        const juce::SpinLock::ScopedLockType lock (bufferLock);
        rec.swap (recording);
    }
    state = State::idle;

    const int noiseStart = (int) (0.2 * sampleRate);
    const int noiseEnd   = silenceBefore - (int) (0.2 * sampleRate);
    const auto noise = measureLevel (rec.data() + noiseStart, noiseEnd - noiseStart);
    const auto signal = measureLevel (rec.data() + silenceBefore, (int) rec.size() - silenceBefore);

    LevelResult r;
    r.peakDbfs = signal.peakDbfs;
    r.snrDb = signal.rmsDbfs - noise.rmsDbfs;

    if (signal.silent || signal.peakDbfs < -70.0f)
    {
        r.status = LevelStatus::noSignal;
        r.message = "Nessun segnale dal microfono. Controlla ingresso e phantom; nello standalone "
                    "disattiva \"Mute audio input\" nelle Opzioni.";
    }
    else if (signal.clipped || signal.peakDbfs > -0.5f)
    {
        r.status = LevelStatus::clipping;
        r.message = "Clipping: abbassa il guadagno del microfono.";
    }
    else if (signal.peakDbfs > -3.0f)
    {
        r.status = LevelStatus::tooHigh;
        r.message = "Livello troppo alto: abbassa il guadagno del microfono.";
    }
    else if (r.snrDb < 40.0f)
    {
        r.status = LevelStatus::noisy;
        r.message = signal.peakDbfs < -12.0f
                  ? "Segnale troppo vicino al rumore: alza il volume dei monitor."
                  : "Stanza rumorosa: riduci i rumori di fondo e riprova.";
    }
    else if (signal.peakDbfs < -18.0f)
    {
        r.status = LevelStatus::tooLow;
        r.message = "Livello troppo basso: alza il guadagno del microfono.";
    }
    else if (signal.peakDbfs < -12.0f)
    {
        r.status = LevelStatus::tooLow;
        r.message = "Livello un po' basso, ma utilizzabile.";
        r.allowed = true;
    }
    else
    {
        r.status = LevelStatus::ok;
        r.message = signal.peakDbfs > -6.0f ? "Livello buono (un po' alto)." : "Livello corretto.";
        r.allowed = true;
    }

    levelResult = r;
    notify();
}

void MeasurementSession::finishChannel (bool right)
{
    std::vector<float> rec;
    {
        const juce::SpinLock::ScopedLockType lock (bufferLock);
        rec.swap (recording);
    }

    if (! right)
    {
        recordingLeft = std::move (rec);

        // stesso segnale, ora sul canale destro
        auto signal = playSignal;
        startPlayback (State::measuringRight, std::move (signal), 1);
        return;
    }

    state = State::analysing;
    notify();

    analysisPool.addJob ([this,
                          recLeft = recordingLeft,
                          recRight = std::move (rec),
                          sweep = currentSweep,
                          pre = silenceBefore,
                          settings = makeSweepSettings (measureSweepSeconds),
                          cal = calibration.getResponse()]
    {
        PointResult result;
        juce::String error;
        int latency = 0;

        auto analyse = [&] (const std::vector<float>& samples, Response& out, const char* side)
        {
            const auto stats = measureLevel (samples.data(), (int) samples.size());
            if (stats.silent)   { error = juce::String ("Nessun segnale registrato (") + side + ")."; return; }
            if (stats.clipped)  { error = juce::String ("Clipping durante la misura (") + side + "): abbassa il guadagno."; return; }

            std::vector<float> aligned (samples.begin() + pre, samples.end());
            const auto impulse = deconvolveSweep (aligned, sweep, settings);
            if (! impulse.valid) { error = impulse.error; return; }

            latency = impulse.peakIndex;
            out = responseFromImpulse (impulse, settings.sampleRate) - cal;
        };

        analyse (recLeft, result.left, "sinistro");
        if (error.isEmpty())
            analyse (recRight, result.right, "destro");

        result.done = error.isEmpty();

        const juce::ScopedLock sl (analysisLock);
        analysisResult = result;
        analysisError = error;
        analysisLatency = latency;
        analysisReady = true;
    });
}
