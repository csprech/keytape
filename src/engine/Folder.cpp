#include "Folder.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace keytape
{
namespace
{
    juce::String safeName (juce::String s)
    {
        s = s.removeCharacters ("/\\:*?\"<>|").trim();
        return s.isEmpty() ? juce::String ("untitled") : s;
    }

    juce::var takeToJson (const Take& t)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("start", (juce::int64) t.start);
        o->setProperty ("length", (juce::int64) t.length);
        if (t.soundData.isObject())
            o->setProperty ("sound", t.soundData);
        return juce::var (o);
    }
} // namespace

Folder::Folder (juce::File r) : root (std::move (r))
{
    formats.registerBasicFormats();
    root.createDirectory();
    for (auto* sub : { "tapes", "mixdowns", "radio", "samples", "snapshots/synth", "snapshots/drum" })
        root.getChildFile (sub).createDirectory();
}

Folder::~Folder() { flush(); }

void Folder::flush()
{
    while (pending.load() > 0)
        juce::Thread::sleep (5);
}

void Folder::background (std::function<void()> job)
{
    ++pending;
    pool.addJob ([this, job = std::move (job)] {
        job();
        --pending;
    });
}

bool Folder::writeWav (const juce::File& f, const juce::AudioBuffer<float>& audio, int length, double sr, int bits)
{
    f.getParentDirectory().createDirectory();
    const auto tmp = f.getSiblingFile (f.getFileName() + ".part");
    tmp.deleteFile();
    {
        std::unique_ptr<juce::OutputStream> out (tmp.createOutputStream().release());
        if (out == nullptr)
            return false;
        juce::WavAudioFormat wav;
        auto w = wav.createWriterFor (out, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (std::max (1, audio.getNumChannels())).withBitsPerSample (bits));
        if (w == nullptr)
            return false;
        if (! w->writeFromAudioSampleBuffer (audio, 0, length))
            return false;
    }
    return tmp.moveFileTo (f);
}

AudioPtr Folder::readAudio (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return nullptr;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 600.0));
    auto a = std::make_shared<Audio>();
    a->sampleRate = reader->sampleRate;
    a->data.setSize (2, len);
    reader->read (&a->data, 0, len, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1)
        a->data.copyFrom (1, 0, a->data, 0, 0, len);
    return a;
}

// ======================================================================== snapshots

juce::StringArray Folder::snapshots (bool drum)
{
    juce::StringArray names;
    for (const auto& f : snapDir (drum).findChildFiles (juce::File::findFiles, false, "*.json"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool Folder::loadSnapshot (bool drum, const juce::String& name, SynthSound* s, DrumKit* k)
{
    const auto f = snapDir (drum).getChildFile (name + ".json");
    const auto v = juce::JSON::parse (f);
    if (! v.isObject())
        return false;
    if (drum && k != nullptr)
        kitFromState (v, *k);
    if (! drum && s != nullptr)
        soundFromState (v, *s);
    return true;
}

void Folder::saveSnapshot (bool drum, const SynthSound* s, const DrumKit* k)
{
    // snapshots are numbered after the sound: "cloud 1", "cloud 2" ...
    const juce::String base = safeName (drum ? (k != nullptr ? k->name : juce::String ("kit")) : (s != nullptr ? s->name : juce::String ("sound")));
    int n = 1;
    while (snapDir (drum).getChildFile (base + " " + juce::String (n) + ".json").exists())
        ++n;
    const auto f = snapDir (drum).getChildFile (base + " " + juce::String (n) + ".json");
    const auto v = drum ? kitState (*k) : soundState (*s);
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (v));
}

void Folder::saveMixdown (int side, const juce::AudioBuffer<float>& audio, int length, double sr)
{
    // a copy for the writer; the name says which side and when
    auto copy = std::make_shared<juce::AudioBuffer<float>> (2, length);
    for (int ch = 0; ch < 2; ++ch)
        copy->copyFrom (ch, 0, audio, ch, 0, length);
    const auto f = root.getChildFile ("mixdowns").getChildFile ("side " + juce::String (side == 0 ? "a" : "b") + " " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S") + ".wav")
                       .getNonexistentSibling();
    background ([f, copy, length, sr] { writeWav (f, *copy, length, sr, 24); });
}

void Folder::reveal() { root.revealToUser(); }

juce::StringArray Folder::midiDevices()
{
    juce::StringArray names;
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        names.add ("in: " + d.name);
    for (const auto& d : juce::MidiOutput::getAvailableDevices())
        names.add ("out: " + d.name);
    if (names.isEmpty())
        names.add ("(none connected)");
    return names;
}

// ======================================================================== tapes

void Folder::saveTape (int index, const Reel& reel)
{
    // each track as one file from the start of the tape (silence between takes), and the takes in tape.json
    const auto dir = tapeDir (index);
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "keytape-tape");
    o->setProperty ("name", reel.name);
    o->setProperty ("style", (int) reel.style);
    o->setProperty ("loopIn", (juce::int64) reel.loopIn);
    o->setProperty ("loopOut", (juce::int64) reel.loopOut);
    o->setProperty ("loopOn", reel.loopOn);
    o->setProperty ("bpm", reel.bpmAtNormal);
    o->setProperty ("sampleRate", sampleRate);
    juce::Array<juce::var> tracks;
    for (const auto& t : reel.tracks)
    {
        juce::Array<juce::var> takes;
        for (const auto& take : t.takes)
            takes.add (takeToJson (take));
        tracks.add (takes);
    }
    o->setProperty ("tracks", tracks);
    const juce::var json (o);
    const auto tracksCopy = reel.tracks; // (shares the takes' audio)
    const double sr = sampleRate;
    background ([dir, json, tracksCopy, sr] {
        dir.createDirectory();
        for (int t = 0; t < kTracks; ++t)
        {
            const auto& tr = tracksCopy[(size_t) t];
            const auto f = dir.getChildFile ("track " + juce::String (t + 1) + ".wav");
            int64_t end = 0;
            for (const auto& take : tr.takes)
                end = std::max (end, take.end());
            if (end <= 0)
            {
                f.deleteFile();
                continue;
            }
            juce::AudioBuffer<float> buf (2, (int) end);
            buf.clear();
            for (const auto& take : tr.takes)
                if (take.audio != nullptr)
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const int from = (int) take.offset;
                        const int len = (int) std::min<int64_t> (take.length, take.audio->length() - take.offset);
                        if (len > 0)
                            buf.copyFrom (ch, (int) take.start, take.audio->data, std::min (ch, take.audio->data.getNumChannels() - 1), from, len);
                    }
            writeWav (f, buf, (int) end, sr, 32);
        }
        dir.getChildFile ("tape.json").replaceWithText (juce::JSON::toString (json));
    });
}

Reel Folder::loadTape (int index)
{
    Reel reel;
    reel.name = "tape " + juce::String (index + 1);
    const auto dir = tapeDir (index);
    const auto v = juce::JSON::parse (dir.getChildFile ("tape.json"));
    if (! v.isObject())
        return reel;
    reel.name = v.getProperty ("name", reel.name).toString();
    reel.style = (TapeStyle) juce::jlimit (0, (int) TapeStyle::Count - 1, (int) v.getProperty ("style", 0));
    reel.loopIn = (int64_t) (juce::int64) v.getProperty ("loopIn", -1);
    reel.loopOut = (int64_t) (juce::int64) v.getProperty ("loopOut", -1);
    reel.loopOn = (bool) v.getProperty ("loopOn", false);
    reel.bpmAtNormal = (double) v.getProperty ("bpm", 120.0);
    const auto* tracks = v["tracks"].getArray();
    for (int t = 0; t < kTracks && tracks != nullptr && t < tracks->size(); ++t)
    {
        const auto audio = readAudio (dir.getChildFile ("track " + juce::String (t + 1) + ".wav"));
        const auto* takes = (*tracks)[t].getArray();
        if (takes == nullptr)
            continue;
        for (const auto& tv : *takes)
        {
            Take take;
            take.start = (int64_t) (juce::int64) tv.getProperty ("start", 0);
            take.length = (int64_t) (juce::int64) tv.getProperty ("length", 0);
            take.offset = take.start;
            take.audio = audio;
            take.soundData = tv["sound"];
            if (take.length <= 0)
                continue;
            if (audio == nullptr || take.end() > audio->length())
            {
                // (the audio is missing or short: the take keeps its place, silent)
                auto silent = std::make_shared<Audio>();
                silent->data.setSize (2, (int) take.length);
                silent->data.clear();
                take.audio = silent;
                take.offset = 0;
            }
            reel.tracks[(size_t) t].takes.push_back (take);
        }
        std::sort (reel.tracks[(size_t) t].takes.begin(), reel.tracks[(size_t) t].takes.end(), [] (const Take& a, const Take& b) { return a.start < b.start; });
    }
    reel.dirty = false;
    return reel;
}

std::vector<Machine::Station> Folder::loadStations()
{
    std::vector<Machine::Station> st;
    auto files = root.getChildFile ("radio").findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    // spread across the band, on odd tenths as broadcasters are
    const int n = std::min (12, files.size());
    for (int i = 0; i < n; ++i)
        if (auto a = readAudio (files[i]))
        {
            Machine::Station s;
            s.mhz = std::round ((88.1 + (107.9 - 88.1) * (n > 1 ? (double) i / (n - 1) : 0.5)) * 5.0) / 5.0 + 0.1;
            s.mhz = juce::jlimit (87.5, 108.0, std::round (s.mhz * 10.0) / 10.0);
            s.audio = a;
            s.name = files[i].getFileNameWithoutExtension();
            st.push_back (s);
        }
    return st;
}

// ======================================================================== samples and sounds

juce::String Folder::sampleFile (const AudioPtr& audio)
{
    if (audio == nullptr)
        return {};
    auto it = sampleNames.find (audio.get());
    if (it != sampleNames.end() && ! it->second.first.expired())
        return it->second.second;
    const auto name = juce::Uuid().toString().substring (0, 12) + ".wav";
    sampleNames[audio.get()] = { audio, name };
    loadedSamples[name] = audio;
    const auto f = root.getChildFile ("samples").getChildFile (name);
    background ([f, audio] { writeWav (f, audio->data, audio->length(), audio->sampleRate, 32); });
    return name;
}

AudioPtr Folder::loadSample (const juce::String& name)
{
    if (name.isEmpty())
        return nullptr;
    auto it = loadedSamples.find (name);
    if (it != loadedSamples.end())
        if (auto a = it->second.lock())
            return a;
    flush(); // (it may still be on its way to the disk)
    auto a = readAudio (root.getChildFile ("samples").getChildFile (name));
    if (a != nullptr)
    {
        loadedSamples[name] = a;
        sampleNames[a.get()] = { a, name };
    }
    return a;
}

juce::var Folder::soundState (const SynthSound& s)
{
    auto v = soundToJson (s);
    if (s.sample != nullptr)
        v.getDynamicObject()->setProperty ("sample", sampleFile (s.sample));
    return v;
}

void Folder::soundFromState (const juce::var& v, SynthSound& s)
{
    soundFromJson (v, s);
    if (auto a = loadSample (v.getProperty ("sample", {}).toString()))
        s.sample = a;
}

juce::var Folder::kitState (const DrumKit& k)
{
    auto v = kitToJson (k);
    if (k.sample != nullptr)
        v.getDynamicObject()->setProperty ("sample", sampleFile (k.sample));
    return v;
}

void Folder::kitFromState (const juce::var& v, DrumKit& k)
{
    kitFromJson (v, k);
    if (auto a = loadSample (v.getProperty ("sample", {}).toString()))
        k.sample = a;
}
} // namespace keytape
