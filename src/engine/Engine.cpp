#include "Engine.h"

namespace keytape
{
namespace
{
    juce::File folderRoot()
    {
        if (auto* dir = std::getenv ("KEYTAPE_DIR")) // testing: keep the real folder out of it
            return juce::File (dir);
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Keytape");
    }
} // namespace

AudioEngine::AudioEngine (bool isHosted) : hosted (isHosted)
{
    folder = std::make_unique<Folder> (folderRoot());
    folder->onMessage = [this] (const juce::String& m) {
        if (onMessage)
            onMessage (m);
    };
    machine.setLibrary (folder.get());
    // the tapes and the radio's stations from the folder
    for (int t = 0; t < kTapes; ++t)
        machine.reels[(size_t) t] = folder->loadTape (t);
    machine.tape.reel = machine.reels[0];
    machine.stations = folder->loadStations();
    if (! hosted)
    {
        const auto f = getSettingsFile();
        if (f.existsAsFile())
            pendingState = juce::JSON::parse (f);
        deviceManager.initialiseWithDefaultDevices (4, 2);
        deviceManager.addAudioCallback (this);
        // every MIDI input plays it
        for (const auto& d : juce::MidiInput::getAvailableDevices())
            deviceManager.setMidiInputDeviceEnabled (d.identifier, true);
        deviceManager.addMidiInputDeviceCallback ({}, this);
#if JUCE_MAC || JUCE_LINUX
        virtualOut = juce::MidiOutput::createNewDevice ("Keytape");
        if (virtualOut != nullptr)
            virtualOut->startBackgroundThread();
        virtualIn = juce::MidiInput::createNewDevice ("Keytape", this);
        if (virtualIn != nullptr)
            virtualIn->start();
#endif
    }
    startTimerHz (30);
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    if (! hosted)
    {
        deviceManager.removeMidiInputDeviceCallback ({}, this);
        if (virtualIn != nullptr)
            virtualIn->stop();
        deviceManager.removeAudioCallback (this);
        deviceManager.closeAudioDevice();
    }
    saveAll();
}

juce::File AudioEngine::getSettingsFile() const
{
    if (auto* dir = std::getenv ("KEYTAPE_SETTINGS_DIR")) // testing
        return juce::File (dir).getChildFile ("settings.json");
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Keytape").getChildFile ("settings.json");
}

// ======================================================================== state

juce::var AudioEngine::getState()
{
    const juce::ScopedLock sl (lock);
    auto& m = machine;
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "keytape-state");
    o->setProperty ("settings", m.settings.toJson());
    juce::Array<juce::var> synths, kits;
    for (auto& s : m.sounds.synth)
        synths.add (folder->soundState (s));
    for (auto& k : m.sounds.drum)
        kits.add (folder->kitState (k));
    o->setProperty ("synth", synths);
    o->setProperty ("kits", kits);
    juce::Array<juce::var> savedSynths, savedKits;
    for (auto& s : m.savedSynth)
        savedSynths.add (folder->soundState (s));
    for (auto& k : m.savedDrum)
        savedKits.add (folder->kitState (k));
    o->setProperty ("savedSynth", savedSynths);
    o->setProperty ("savedKits", savedKits);
    o->setProperty ("synthSlot", m.sounds.synthSlot);
    o->setProperty ("drumSlot", m.sounds.drumSlot);
    o->setProperty ("mixer", mixerToJson (m.mixer));
    auto* t = new juce::DynamicObject();
    t->setProperty ("bpm", m.tempo.bpm);
    t->setProperty ("sync", (int) m.tempo.sync);
    t->setProperty ("sixteenth", m.tempo.sixteenth);
    t->setProperty ("metronome", m.tempo.metronome);
    t->setProperty ("sound", m.tempo.metronomeSound);
    o->setProperty ("tempo", juce::var (t));
    auto* in = new juce::DynamicObject();
    in->setProperty ("source", (int) m.input.source);
    in->setProperty ("stereo", m.input.stereo);
    in->setProperty ("fm", m.input.fmMHz);
    in->setProperty ("threshold", m.input.threshold);
    in->setProperty ("gain", m.input.gain);
    o->setProperty ("input", juce::var (in));
    o->setProperty ("tapeIndex", m.tapeIndex);
    o->setProperty ("track", m.tape.track);
    o->setProperty ("recLevel", m.tape.recLevel);
    o->setProperty ("octave", juce::Array<juce::var> { m.octave[0], m.octave[1] });
    o->setProperty ("instrument", (int) m.instrument);
    o->setProperty ("volume", volume.load());
    o->setProperty ("midiOut", midiOutId);
    return juce::var (o);
}

void AudioEngine::setState (const juce::var& v)
{
    const juce::ScopedLock sl (lock);
    loadState (v);
}

void AudioEngine::loadState (const juce::var& v)
{
    if (! v.isObject())
        return;
    auto& m = machine;
    m.settings.fromJson (v["settings"]);
    if (const auto* a = v["synth"].getArray())
        for (int i = 0; i < kSlots && i < a->size(); ++i)
            folder->soundFromState ((*a)[i], m.sounds.synth[(size_t) i]);
    if (const auto* a = v["kits"].getArray())
        for (int i = 0; i < kSlots && i < a->size(); ++i)
            folder->kitFromState ((*a)[i], m.sounds.drum[(size_t) i]);
    m.savedSynth = m.sounds.synth;
    m.savedDrum = m.sounds.drum;
    if (const auto* a = v["savedSynth"].getArray())
        for (int i = 0; i < kSlots && i < a->size(); ++i)
            folder->soundFromState ((*a)[i], m.savedSynth[(size_t) i]);
    if (const auto* a = v["savedKits"].getArray())
        for (int i = 0; i < kSlots && i < a->size(); ++i)
            folder->kitFromState ((*a)[i], m.savedDrum[(size_t) i]);
    m.sounds.synthSlot = juce::jlimit (0, kSlots - 1, (int) v.getProperty ("synthSlot", 0));
    m.sounds.drumSlot = juce::jlimit (0, kSlots - 1, (int) v.getProperty ("drumSlot", 0));
    if (v["mixer"].isObject())
        mixerFromJson (v["mixer"], m.mixer);
    const auto t = v["tempo"];
    m.tempo.bpm = juce::jlimit (40.0, 240.0, (double) t.getProperty ("bpm", 120.0));
    m.tempo.sync = (Sync) juce::jlimit (0, 3, (int) t.getProperty ("sync", (int) Sync::BeatMatch));
    m.tempo.sixteenth = (bool) t.getProperty ("sixteenth", false);
    m.tempo.metronome = (float) t.getProperty ("metronome", 0.0);
    m.tempo.metronomeSound = juce::jlimit (0, 3, (int) t.getProperty ("sound", 0));
    const auto in = v["input"];
    m.input.source = (Input) juce::jlimit (0, 3, (int) in.getProperty ("source", 0));
    m.input.stereo = (bool) in.getProperty ("stereo", true);
    m.input.fmMHz = juce::jlimit (87.5, 108.0, (double) in.getProperty ("fm", 98.0));
    m.input.threshold = (float) in.getProperty ("threshold", 0.0);
    m.input.gain = (float) in.getProperty ("gain", 0.5);
    const int index = juce::jlimit (0, kTapes - 1, (int) v.getProperty ("tapeIndex", 0));
    if (index != m.tapeIndex)
        m.selectTape (index);
    m.tape.track = juce::jlimit (0, kTracks - 1, (int) v.getProperty ("track", 0));
    m.tape.recLevel = (float) v.getProperty ("recLevel", 0.8);
    if (const auto* o = v["octave"].getArray(); o != nullptr && o->size() == 2)
        m.octave = { juce::jlimit (-4, 4, (int) (*o)[0]), juce::jlimit (-4, 4, (int) (*o)[1]) };
    m.instrument = (int) v.getProperty ("instrument", 0) == (int) Mode::Drum ? Mode::Drum : Mode::Synth;
    volume = juce::jlimit (0.0f, 1.0f, (float) v.getProperty ("volume", 0.8));
    const auto out = v.getProperty ("midiOut", {}).toString();
    if (! hosted && out.isNotEmpty() && out != midiOutId)
        juce::MessageManager::callAsync ([this, out] { setMidiOutput (out); });
}

void AudioEngine::saveSettings()
{
    if (hosted)
        return;
    const auto text = juce::JSON::toString (getState());
    if (text == lastSavedState)
        return;
    lastSavedState = text;
    const auto f = getSettingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (text);
}

void AudioEngine::saveChangedTapes()
{
    // a tape that changed is written once the transport has stopped recording
    const juce::ScopedLock sl (lock);
    auto& m = machine;
    if (m.tape.isRecording() || m.tape.isArmed())
        return;
    if (m.tape.reel.dirty)
    {
        m.tape.reel.dirty = false;
        folder->saveTape (m.tapeIndex, m.tape.reel);
    }
    for (int t = 0; t < kTapes; ++t)
        if (t != m.tapeIndex && m.reels[(size_t) t].dirty)
        {
            m.reels[(size_t) t].dirty = false;
            folder->saveTape (t, m.reels[(size_t) t]);
        }
}

void AudioEngine::saveAll()
{
    {
        const juce::ScopedLock sl (lock);
        machine.tape.stop(); // (a take being recorded is filed first)
        machine.idle();
    }
    saveChangedTapes();
    saveSettings();
    folder->flush();
}

void AudioEngine::setMidiOutput (const juce::String& identifier)
{
    auto out = identifier.isEmpty() ? nullptr : juce::MidiOutput::openDevice (identifier);
    if (out != nullptr)
        out->startBackgroundThread();
    const juce::ScopedLock sl (lock);
    std::swap (midiOut, out);
    midiOutId = identifier;
}

void AudioEngine::setVolume (float v) { volume = juce::jlimit (0.0f, 1.0f, v); }

// ======================================================================== audio

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) { collector.addMessageToQueue (m); }

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device) { prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples()); }

void AudioEngine::prepareHosted (double sr, int maxBlock) { prepare (sr, maxBlock); }

void AudioEngine::prepare (double sr, int block)
{
    const juce::ScopedLock sl (lock);
    machine.prepare (sr, std::max (512, block));
    folder->setSampleRate (sr);
    collector.reset (sr);
    midiIn.ensureSize (4096);
    spare.assign ((size_t) std::max (8192, block * 2), 0.0f);
    midiBlock.ensureSize (4096);
    if (pendingState.isObject())
    {
        loadState (pendingState);
        pendingState = juce::var();
    }
    sampleRate = sr;
    prepared = true;
}

void AudioEngine::render (const float* const* in, int numIn, float* L, float* R, int n, juce::MidiBuffer* out)
{
    Machine::AudioIO io;
    io.micL = numIn > 0 ? in[0] : nullptr;
    io.micR = numIn > 1 ? in[1] : io.micL;
    io.usbL = numIn > 2 ? in[2] : nullptr;
    io.usbR = numIn > 3 ? in[3] : io.usbL;
    io.outL = L;
    io.outR = R;
    machine.process (n, io, &midiIn, out);
    // the volume knob (after everything; smoothed)
    const float target = volume.load();
    float level = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        volumeSmoothed += (target - volumeSmoothed) * 0.002f;
        const float g = volumeSmoothed * volumeSmoothed * 1.4f;
        L[i] *= g;
        R[i] *= g;
        level = std::max (level, std::max (std::abs (L[i]), std::abs (R[i])));
    }
    outputLevel = std::max (level, outputLevel.load() * 0.9f);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                               const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], n);
    midiIn.clear();
    collector.removeNextBlockOfMessages (midiIn, n);
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || ! prepared || numOut < 1 || out[0] == nullptr)
        return;
    float* L = out[0];
    float* R = numOut > 1 && out[1] != nullptr ? out[1] : nullptr;
    if (R == nullptr)
    {
        if ((int) spare.size() < n)
            return; // (prepare sized it for the device's blocks)
        R = spare.data();
    }
    midiBlock.clear();
    render (in, numIn, L, R, n, &midiBlock);
    if (numOut == 1)
        for (int i = 0; i < n; ++i)
            L[i] = 0.5f * (L[i] + R[i]);
    if (! midiBlock.isEmpty())
    {
        const double rate = sampleRate.load();
        for (auto* port : { midiOut.get(), virtualOut.get() })
            if (port != nullptr)
                port->sendBlockOfMessages (midiBlock, juce::Time::getMillisecondCounterHiRes(), rate);
    }
}

void AudioEngine::processHosted (const float* const* in, float* const* out, int n, juce::MidiBuffer& midi)
{
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || ! prepared)
    {
        midi.clear();
        for (int c = 0; c < 2; ++c)
            if (out[c] != nullptr)
                juce::FloatVectorOperations::clear (out[c], n);
        return;
    }
    midiIn.clear();
    midiIn.addEvents (midi, 0, n, 0);
    midi.clear();
    render (in, 4, out[0], out[1], n, &midi);
}

void AudioEngine::timerCallback()
{
    if (! prepared)
        return;
    {
        const juce::ScopedLock sl (lock);
        machine.idle();
        started = true;
    }
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastSaveCheck > 2000)
    {
        lastSaveCheck = now;
        saveChangedTapes();
        saveSettings();
    }
}
} // namespace keytape
