#pragma once

// Keytape's files, in one folder (Music/Keytape, or $KEYTAPE_DIR for testing), which stands for the original's
// disk view:
//   tapes/tape 1 .. tape 8/   track 1-4.wav (each track from the start of the tape) and tape.json (its takes,
//                             name, style, loop and tempo)
//   snapshots/synth, /drum    saved sounds (json, with a wav for a sampler's sample)
//   mixdowns/                 the output recorded to side a or b
//   radio/                    audio files the radio receives, each on its own frequency
//   samples/                  the samples the sound slots use (referenced from the saved state)
// Big files are written on a background thread from copies, so the instrument never waits for the disk.

#include "../core/Machine.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace keytape
{
class Folder : public Library
{
public:
    explicit Folder (juce::File root);
    ~Folder() override;
    juce::File getRoot() const { return root; }
    void setSampleRate (double sr) { sampleRate = sr; }

    // ---- Library (the machine calls these on the message thread)
    juce::StringArray snapshots (bool drum) override;
    bool loadSnapshot (bool drum, const juce::String& name, SynthSound* s, DrumKit* k) override;
    void saveSnapshot (bool drum, const SynthSound* s, const DrumKit* k) override;
    void saveMixdown (int side, const juce::AudioBuffer<float>& audio, int length, double sampleRate) override;
    void reveal() override;
    juce::StringArray midiDevices() override;

    // ---- tapes
    /** Writes a tape (its takes are shared, not copied; the writing happens in the background). */
    void saveTape (int index, const Reel& reel);
    /** Reads a tape back (an empty reel if there's none). */
    Reel loadTape (int index);
    /** The radio's stations: the files in radio/, spread across the band. */
    std::vector<Machine::Station> loadStations();

    // ---- samples and sounds in the saved state
    /** A sample's file name in samples/ (written the first time it's asked for). */
    juce::String sampleFile (const AudioPtr& audio);
    AudioPtr loadSample (const juce::String& name);
    juce::var soundState (const SynthSound& s);
    void soundFromState (const juce::var& v, SynthSound& s);
    juce::var kitState (const DrumKit& k);
    void kitFromState (const juce::var& v, DrumKit& k);

    /** Reads any audio file (stereo float); empty if it can't. */
    AudioPtr readAudio (const juce::File& f);
    /** Waits for the files being written (tests, quitting). */
    void flush();
    std::function<void (const juce::String&)> onMessage;

private:
    juce::File tapeDir (int index) const { return root.getChildFile ("tapes").getChildFile ("tape " + juce::String (index + 1)); }
    juce::File snapDir (bool drum) const { return root.getChildFile ("snapshots").getChildFile (drum ? "drum" : "synth"); }
    static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& audio, int length, double sampleRate, int bits);
    void background (std::function<void()> job);

    juce::File root;
    double sampleRate = 48000.0;
    juce::AudioFormatManager formats;
    juce::ThreadPool pool { 1 };
    std::map<const Audio*, std::pair<std::weak_ptr<const Audio>, juce::String>> sampleNames; // (samples already in samples/)
    std::map<juce::String, std::weak_ptr<const Audio>> loadedSamples;
    std::atomic<int> pending { 0 };
};
} // namespace keytape
