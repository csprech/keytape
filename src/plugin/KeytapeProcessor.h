#pragma once

// Keytape as an audio processor, for hosts (the Tabletop studio, or a plug-in host): the same engine and panel as
// the app, without an audio device of its own. Its jacks become the processor's channels:
//   inputs   0, 1   mic / line in L, R
//   inputs   2, 3   USB audio L, R
//   outputs  0, 1   the output L, R (with sync out, the left carries the pulses)
//   MIDI in         notes, CCs and clock; MIDI out: clock and ctrl mode's notes
// Tapes, snapshots and mixdowns live in the same Keytape folder as the app's; the settings travel with the session.

#include "../engine/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace keytape
{
class KeytapeProcessor : public juce::AudioProcessor
{
public:
    KeytapeProcessor();
    ~KeytapeProcessor() override;

    static constexpr int kNumInputs = 4, kNumOutputs = 2;

    AudioEngine& getEngine() { return engine; }
    /** The host says which input jacks are cabled (an unplugged pair is silent). */
    void setInputPlugged (int channel, bool plugged);

    const juce::String getName() const override { return "Keytape"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    AudioEngine engine { true };
    juce::AudioBuffer<float> inputCopy;
    std::array<std::atomic<bool>, kNumInputs> plugged {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeytapeProcessor)
};
} // namespace keytape
