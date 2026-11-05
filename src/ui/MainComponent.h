#pragma once

// The app's window: the panel scaled to fit, a slim bar under it with the app's own buttons (audio and MIDI
// settings, help, the Keytape folder), and the testing aids (a snapshot of the panel, the self-test).

#include "Panel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace keytape::ui
{
class MainComponent : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent (AudioEngine& engine);
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void prepareToQuit();
    static constexpr int kBar = 34;

private:
    void timerCallback() override;
    void showAudioSettings();
    void showHelp();
    void loadSettings();
    void saveSettings();
    void runSelfTest (const juce::File& report);

    AudioEngine& engine;
    Panel panel;
    juce::TextButton audioBtn { "audio / midi" }, helpBtn { "help" }, folderBtn { "folder" };
    std::unique_ptr<juce::PropertiesFile> settings;
    juce::String message;
    juce::uint32 messageUntil = 0;
};
} // namespace keytape::ui
