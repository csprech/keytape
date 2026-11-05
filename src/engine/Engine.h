#pragma once

// Audio, MIDI and files around the instrument.
//  * The Machine lives under one lock: the message thread holds it for each key, encoder and file job; the audio
//    thread tries it each block (if it's busy, that block is silent).
//  * Standalone, the computer's inputs 1-2 are mic / line in and 3-4 USB audio; outputs 1-2 are the output.
//    MIDI comes from every MIDI input and a virtual port ("Keytape"); MIDI goes out to the virtual port and the
//    chosen output.
//  * The volume knob scales the output after everything (as the original's does: it isn't recorded).
//  * The sounds, mixer and settings are kept in a file; tapes, snapshots and mixdowns in the Keytape folder.

#include "Folder.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

namespace keytape
{
class AudioEngine : public juce::AudioIODeviceCallback, private juce::MidiInputCallback, private juce::Timer
{
public:
    /** Standalone, the engine opens the audio device and MIDI itself and keeps its settings in a file.
        Hosted (in a studio or a plug-in host), the host calls prepareHosted() and processHosted(), and the
        settings travel with the host's session (getState / setState). */
    explicit AudioEngine (bool hosted = false);
    ~AudioEngine() override;
    bool isHosted() const { return hosted; }

    void prepareHosted (double sampleRate, int maxBlockSize);
    /** Hosted: one block. in[0..3]: mic / line L, R, USB L, R (any may be null); out[0..1]: L, R. MIDI in is
        played; MIDI out (clock, ctrl notes) is returned in `midi`. */
    void processHosted (const float* const* in, float* const* out, int numSamples, juce::MidiBuffer& midi);

    template <typename Fn>
    auto withMachine (Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        return fn (machine);
    }

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    void setMidiOutput (const juce::String& identifier);
    juce::String getMidiOutputIdentifier() const { return midiOutId; }
    void setVolume (float v);
    float getVolume() const { return volume.load(); }
    float getOutputLevel() const { return outputLevel.load(); }
    std::function<void (const juce::String&)> onMessage;

    juce::var getState();
    void setState (const juce::var& state);
    Folder& getFolder() { return *folder; }
    juce::File getSettingsFile() const;
    /** Saves now: the settings, and any tape that changed (then waits for the files). */
    void saveAll();

private:
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;
    void prepare (double sampleRate, int block);
    void render (const float* const* in, int numIn, float* L, float* R, int n, juce::MidiBuffer* midiOut);
    void saveSettings();
    void loadState (const juce::var& v);
    void saveChangedTapes();

    const bool hosted;
    juce::CriticalSection lock;
    std::unique_ptr<Folder> folder;
    Machine machine;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::MidiOutput> midiOut, virtualOut;
    std::unique_ptr<juce::MidiInput> virtualIn;
    juce::MidiMessageCollector collector;
    juce::String midiOutId;
    juce::MidiBuffer midiIn, midiBlock;
    std::vector<float> spare;
    std::atomic<float> volume { 0.8f }, outputLevel { 0.0f };
    float volumeSmoothed = 0.8f;
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<bool> prepared { false };
    bool started = false;
    juce::var pendingState;
    juce::String lastSavedState;
    juce::uint32 lastSaveCheck = 0;
};
} // namespace keytape
