#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Demo.h"

namespace brl
{
//==============================================================================
void ChunkedRecorder::reset (int numChannels)
{
    channels = jlimit (1, 2, numChannels);
    frames = 0;
    // keep already-allocated chunks (they are reused), just top up
    topUp();
}

bool ChunkedRecorder::allocateNext()
{
    const SpinLock::ScopedLockType sl (allocLock);
    const int idx = allocated.load();
    if (idx >= kMaxChunks) return false;
    chunks[(size_t) idx].reset (new float[(size_t) kChunkFrames * 2]);
    allocated.store (idx + 1, std::memory_order_release);
    return true;
}

void ChunkedRecorder::topUp (int spare)
{
    const int needed = (int) (frames.load() / kChunkFrames) + 1 + spare;
    while (allocated.load() < jmin (needed, kMaxChunks))
        if (! allocateNext()) break;
}

bool ChunkedRecorder::write (const float* const* src, int numCh, int n, bool mayAllocate)
{
    int64 pos = frames.load();
    int done = 0;
    while (done < n)
    {
        const int chunk = (int) (pos / kChunkFrames);
        if (chunk >= allocated.load (std::memory_order_acquire))
        {
            if (! mayAllocate || ! allocateNext()) { frames = pos; return false; }
        }
        float* dst = chunks[(size_t) chunk].get();
        const int offset = (int) (pos % kChunkFrames);
        const int count = jmin (n - done, kChunkFrames - offset);
        for (int c = 0; c < 2; ++c)
            std::memcpy (dst + (size_t) c * kChunkFrames + offset, src[jmin (c, numCh - 1)] + done, sizeof (float) * (size_t) count);
        done += count;
        pos += count;
    }
    frames.store (pos, std::memory_order_release);
    return true;
}

AudioBuffer<float> ChunkedRecorder::collect() const
{
    const int64 total = frames.load();
    AudioBuffer<float> out (2, (int) total);
    for (int64 pos = 0; pos < total;)
    {
        const int chunk = (int) (pos / kChunkFrames);
        const int count = (int) jmin ((int64) kChunkFrames, total - pos);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, (int) pos, chunks[(size_t) chunk].get() + (size_t) c * kChunkFrames, count);
        pos += count;
    }
    return out;
}

//==============================================================================
BeatReelProcessor::BeatReelProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true))
{
    style = makeLook (0);
    style.title = "MY BEAT";
    license = License::current();
    previewAudio = (bool) settings().getBoolValue ("previewAudio", false);
    maxBars = settings().getIntValue ("maxBars", 0);

    // demo beat so the looks can be browsed before the first capture
    worker.run ([this]
    {
        CaptureInfo info;
        std::shared_ptr<MidiData> demoMidi;
        auto audio = makeDemoBeat (48000.0, info, demoMidi);
        auto d = analyse (std::move (audio), info, demoMidi.get());
        { const ScopedLock sl (dataLock); if (midi == nullptr) midi = demoMidi; }
        setData (d, true);
    });
    startTimerHz (30);
}

BeatReelProcessor::~BeatReelProcessor()
{
    stopTimer();
    exporter.cancel();
}

bool BeatReelProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void BeatReelProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    recorder.topUp();
}

String BeatReelProcessor::getHostShortName() const
{
    PluginHostType host;
    if (host.isFruityLoops()) return "FL";
    if (host.isAbletonLive()) return "LIVE";
    if (host.isLogic()) return "LOGIC";
    if (host.isReaper()) return "REAPER";
    if (host.isStudioOne()) return "STUDIO ONE";
    if (host.isCubase()) return "CUBASE";
    if (host.isBitwigStudio()) return "BITWIG";
    if (host.isGarageBand()) return "GARAGEBAND";
    if (wrapperType == wrapperType_Standalone) return "STANDALONE";
    return "YOUR DAW";
}

//==============================================================================
void BeatReelProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);
    lastAudioCallbackMs = Time::getMillisecondCounter();
    if (n == 0 || numIn == 0) return;

    const double sr = currentSampleRate.load() > 0 ? currentSampleRate.load() : 44100.0;
    bool playing = false, looping = false;
    Optional<double> ppq, bpmOpt;
    Optional<int64> timeSamples;
    Optional<AudioPlayHead::LoopPoints> loop;
    int beatsPerBar = 4;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying() || pos->getIsRecording();
            ppq = pos->getPpqPosition();
            bpmOpt = pos->getBpm();
            timeSamples = pos->getTimeInSamples();
            looping = pos->getIsLooping();
            loop = pos->getLoopPoints();
            if (auto ts = pos->getTimeSignature()) beatsPerBar = jmax (1, ts->numerator);
        }
    const double bpm = bpmOpt.hasValue() && *bpmOpt > 1.0 ? *bpmOpt : hostBpm.load();
    hostPlaying = playing; hostLooping = looping; hostBpm = bpm;
    if (ppq.hasValue()) hostPpq = *ppq;

    int state = recState.load();
    if (state == recArmed && playing && ! stopRequested)
    {
        pendingInfo = {};
        pendingInfo.sampleRate = sr;
        pendingInfo.bpm = bpm;
        pendingInfo.beatsPerBar = beatsPerBar;
        pendingInfo.startPpq = ppq.hasValue() ? *ppq : 0.0;
        pendingInfo.bpmFromHost = bpmOpt.hasValue();
        hadPpq = ppq.hasValue();  expectedPpq = hadPpq ? *ppq : 0.0;
        hadTime = timeSamples.hasValue(); expectedTime = hadTime ? *timeSamples : 0;

        limitFrames = (int64) (sr * 60 * 20);
        if (maxBars > 0) limitFrames = (int64) (maxBars * beatsPerBar * 60.0 / bpm * sr);
        int64 lf = 0;
        if (looping && loop.hasValue() && ppq.hasValue() && loop->ppqEnd > *ppq + 0.01)
            lf = (int64) std::llround ((loop->ppqEnd - *ppq) * 60.0 / bpm * sr);
        loopFrames = lf;
        if (lf > 0) limitFrames = jmin (limitFrames, lf);
        recState = state = recRecording;
    }

    if (state == recRecording)
    {
        const int64 have = recorder.getNumFrames();
        bool stop = stopRequested.load() || ! playing;
        // the host jumped back = the loop wrapped around -> exactly one loop captured
        if (! stop && have > 0 && hadPpq && ppq.hasValue() && *ppq < expectedPpq - 0.02) stop = true;
        if (! stop && have > 0 && ! hadPpq && hadTime && timeSamples.hasValue() && *timeSamples < expectedTime - 64) stop = true;

        if (! stop)
        {
            int toWrite = n;
            const int64 remaining = limitFrames - have;
            if (remaining <= n) { toWrite = (int) jmax ((int64) 0, remaining); stop = true; }
            if (toWrite > 0 && ! recorder.write (buffer.getArrayOfReadPointers(), numIn, toWrite, isNonRealtime()))
                stop = true;   // ran out of pre-allocated memory
            lastWriteMs = Time::getMillisecondCounter();
            if (ppq.hasValue()) expectedPpq = *ppq + n / sr * bpm / 60.0;
            if (timeSamples.hasValue()) expectedTime = *timeSamples + n;
        }
        if (stop) recState = recFinished;
    }

    // preview playback while the DAW is stopped
    bool holding = false;
    if (auto hold = previewHold.load(); hold > 0 && ! playing)
    {
        previewHold = jmax ((int64) 0, hold - n);
        holding = true;
    }
    if (! playing && previewPlaying.load() && state != recRecording && ! holding)
    {
        const SpinLock::ScopedTryLockType tl (previewLock);
        if (tl.isLocked() && previewData != nullptr && previewData->audio.getNumSamples() > 0)
        {
            const auto& a = previewData->audio;
            const int64 total = a.getNumSamples();
            const double ratio = previewData->info.sampleRate / sr;
            double pos = (double) (previewPos.load() % total);
            if (previewAudio.load())
            {
                const int outCh = jmin (2, buffer.getNumChannels());
                for (int i = 0; i < n; ++i)
                {
                    const double p = pos + i * ratio;
                    const int64 i0 = (int64) p % total, i1 = (i0 + 1) % total;
                    const float fr = (float) (p - std::floor (p));
                    for (int c = 0; c < outCh; ++c)
                    {
                        const float* src = a.getReadPointer (jmin (c, a.getNumChannels() - 1));
                        buffer.addSample (c, i, src[i0] + (src[i1] - src[i0]) * fr);
                    }
                }
            }
            previewPos = (int64) (pos + n * ratio) % total;
        }
    }
}

//==============================================================================
void BeatReelProcessor::toggleRec()
{
    switch (recState.load())
    {
        case recIdle:
            stopRequested = false;
            recorder.reset (2);
            loopFrames = 0;
            lastWriteMs = Time::getMillisecondCounter();
            recState = recArmed;
            break;
        case recArmed:
            recState = recIdle;
            break;
        case recRecording:
            stopRequested = true;
            break;
        default: break;
    }
}

void BeatReelProcessor::cancelRec()
{
    stopRequested = true;
    recState = recIdle;
}

double BeatReelProcessor::getRecordedBars() const
{
    if (recState.load() != recRecording) return 0.0;
    return getRecordedSeconds() * pendingInfo.bpm / 60.0 / jmax (1, pendingInfo.beatsPerBar);
}

float BeatReelProcessor::getLoopProgress() const
{
    const auto lf = loopFrames.load();
    if (lf <= 0) return -1.0f;
    return jlimit (0.0f, 1.0f, (float) recorder.getNumFrames() / (float) lf);
}

void BeatReelProcessor::timerCallback()
{
    const int state = recState.load();
    if (state == recArmed || state == recRecording) recorder.topUp();

    // offline render finished without a transport stop: no audio for 2 s
    if (state == recRecording && recorder.getNumFrames() > 0
        && Time::getMillisecondCounter() - lastWriteMs.load() > 2000)
        recState = recFinished;

    if (recState.load() == recFinished)
        finishCapture();
}

void BeatReelProcessor::finishCapture()
{
    recState = recIdle;
    stopRequested = false;
    auto info = pendingInfo;
    if (recorder.getNumFrames() < (int64) (info.sampleRate * 0.5))
    {
        const ScopedLock sl (dataLock);
        lastError = "Capture too short - press REC, then play at least half a second.";
        return;
    }
    info.title = style.title;
    reanalyse (recorder.collect(), info);
}

void BeatReelProcessor::reanalyse (AudioBuffer<float> audio, CaptureInfo info)
{
    { const ScopedLock sl (dataLock); lastError = {}; }
    auto shared = std::make_shared<AudioBuffer<float>> (std::move (audio));
    worker.run ([this, shared, info]
    {
        // keep the bit-exact capture on disk so the session can be reopened later
        auto f = getCaptureDir().getChildFile ("capture_" + Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".wav");
        writeFloatWav (f, *shared, info.sampleRate);
        auto files = getCaptureDir().findChildFiles (File::findFiles, false, "capture_*.wav");
        files.sort();
        for (int i = 0; i < files.size() - 30; ++i) files.getReference (i).deleteFile();

        auto m = getMidi();
        auto d = analyse (std::move (*shared), info, m.get());
        if (! info.bpmFromHost) d->info.bpm = estimateBpm (*d);
        captureFile = f;
        setData (d, false);
    });
}

void BeatReelProcessor::setData (std::shared_ptr<AnalysisData> d, bool isDemoData)
{
    std::shared_ptr<const AnalysisData> old;
    {
        const ScopedLock sl (dataLock);
        old = data;
        data = d;
        demo = isDemoData;
    }
    {
        const SpinLock::ScopedLockType sl (previewLock);
        std::swap (old, previewData);
        previewData = d;
        previewPos = 0;
    }
    ++styleVersion;
}   // the old data is released here, outside the locks

void BeatReelProcessor::importAudio (const File& file)
{
    AudioBuffer<float> audio;
    CaptureInfo info;
    String err;
    if (! loadAudioFile (file, audio, info.sampleRate, err)) { const ScopedLock sl (dataLock); lastError = err; return; }
    info.bpmFromHost = false;
    info.bpm = hostBpm.load();
    info.title = file.getFileNameWithoutExtension().toUpperCase();
    style.title = info.title;
    reanalyse (std::move (audio), info);
}

void BeatReelProcessor::loadMidi (const File& file)
{
    auto d = getData();
    String err;
    auto m = MidiData::load (file, d != nullptr ? d->info.beatsPerBar : 4, err);
    if (m == nullptr) { const ScopedLock sl (dataLock); lastError = err; return; }
    { const ScopedLock sl (dataLock); midi = m; midiFile = file; lastError = {}; }
    if (d != nullptr)
    {
        auto copy = std::make_shared<AnalysisData> (*d);
        detectKey (*copy, m.get());
        setData (copy, demo.load());
    }
    ++styleVersion;
}

void BeatReelProcessor::clearMidi()
{
    { const ScopedLock sl (dataLock); midi = nullptr; midiFile = File(); }
    if (auto d = getData())
    {
        auto copy = std::make_shared<AnalysisData> (*d);
        detectKey (*copy, nullptr);
        setData (copy, demo.load());
    }
}

void BeatReelProcessor::setBpm (double bpm)
{
    if (auto d = getData())
    {
        auto copy = std::make_shared<AnalysisData> (*d);
        copy->info.bpm = jlimit (40.0, 300.0, bpm);
        setData (copy, demo.load());
    }
}

const Image* BeatReelProcessor::getBackgroundImage()
{
    if (style.bgImagePath != bgImagePath)
    {
        bgImagePath = style.bgImagePath;
        bgImage = {};
        if (bgImagePath.isNotEmpty())
        {
            auto img = ImageFileFormat::loadFrom (File (bgImagePath));
            if (img.isValid() && jmax (img.getWidth(), img.getHeight()) > 1600)
            {
                const float k = 1600.0f / (float) jmax (img.getWidth(), img.getHeight());
                img = img.rescaled (roundToInt (img.getWidth() * k), roundToInt (img.getHeight() * k), Graphics::highResamplingQuality);
            }
            bgImage = img;
        }
    }
    return bgImage.isValid() ? &bgImage : nullptr;
}

double BeatReelProcessor::getPreviewSeconds()
{
    auto d = getData();
    const double nowMs = Time::getMillisecondCounterHiRes();
    const double elapsed = previewClockMs > 0 ? (nowMs - previewClockMs) / 1000.0 : 0.0;
    previewClockMs = nowMs;
    if (d == nullptr || d->duration <= 0) return 0.0;

    // follow the DAW while it plays back the captured section
    if (hostPlaying.load() && ! demo.load() && d->info.bpmFromHost && recState.load() != recRecording)
    {
        double t = (hostPpq.load() - d->info.startPpq) * 60.0 / d->info.bpm;
        t = std::fmod (t, d->duration);
        if (t < 0) t += d->duration;
        previewPos = (int64) (t * d->info.sampleRate);
        return t;
    }
    // the host isn't calling processBlock (or the standalone has no audio): run our own clock
    if (previewPlaying.load() && Time::getMillisecondCounter() - lastAudioCallbackMs.load() > 150)
    {
        const int64 total = jmax (1, d->audio.getNumSamples());
        previewPos = (previewPos.load() + (int64) (elapsed * d->info.sampleRate)) % total;
    }
    return (double) previewPos.load() / d->info.sampleRate;
}

PropertiesFile& BeatReelProcessor::settings()
{
    if (props == nullptr)
    {
        PropertiesFile::Options o;
        o.storageFormat = PropertiesFile::storeAsXML;
        props = std::make_unique<PropertiesFile> (getUserDataDir().getChildFile ("settings.xml"), o);
    }
    return *props;
}

File BeatReelProcessor::findFfmpeg() { return Exporter::findFfmpeg (settings().getValue ("ffmpegPath")); }

//==============================================================================
void BeatReelProcessor::getStateInformation (MemoryBlock& dest)
{
    XmlElement xml ("BEATREEL");
    xml.setAttribute ("style", JSON::toString (style.toVar(), true));
    if (auto d = getData(); d != nullptr && ! demo.load())
    {
        xml.setAttribute ("capture", captureFile.getFullPathName());
        xml.setAttribute ("bpm", d->info.bpm);
        xml.setAttribute ("beatsPerBar", d->info.beatsPerBar);
        xml.setAttribute ("startPpq", d->info.startPpq);
        xml.setAttribute ("bpmFromHost", d->info.bpmFromHost);
    }
    { const ScopedLock sl (dataLock); if (midiFile.existsAsFile()) xml.setAttribute ("midi", midiFile.getFullPathName()); }
    copyXmlToBinary (xml, dest);
}

void BeatReelProcessor::setStateInformation (const void* raw, int size)
{
    auto xml = getXmlFromBinary (raw, size);
    if (xml == nullptr || ! xml->hasTagName ("BEATREEL")) return;
    style.fromVar (JSON::parse (xml->getStringAttribute ("style")));
    ++styleVersion;

    File midiF (xml->getStringAttribute ("midi"));
    if (midiF.existsAsFile())
    {
        String err;
        auto m = MidiData::load (midiF, xml->getIntAttribute ("beatsPerBar", 4), err);
        const ScopedLock sl (dataLock);
        if (m != nullptr) { midi = m; midiFile = midiF; }
    }

    File cap (xml->getStringAttribute ("capture"));
    if (cap.existsAsFile())
    {
        CaptureInfo info;
        AudioBuffer<float> audio;
        String err;
        if (loadAudioFile (cap, audio, info.sampleRate, err))
        {
            info.bpm = xml->getDoubleAttribute ("bpm", 120.0);
            info.beatsPerBar = xml->getIntAttribute ("beatsPerBar", 4);
            info.startPpq = xml->getDoubleAttribute ("startPpq", 0.0);
            info.bpmFromHost = true;   // keep the saved tempo
            info.title = style.title;
            auto shared = std::make_shared<AudioBuffer<float>> (std::move (audio));
            worker.run ([this, shared, info, cap]
            {
                auto m = getMidi();
                auto d = analyse (std::move (*shared), info, m.get());
                captureFile = cap;
                setData (d, false);
            });
        }
    }
}

AudioProcessorEditor* BeatReelProcessor::createEditor() { return new BeatReelEditor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new brl::BeatReelProcessor(); }
