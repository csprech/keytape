#include "MainComponent.h"

namespace keytape::ui
{
namespace
{
    bool testing() { return std::getenv ("KEYTAPE_SELFTEST") != nullptr || std::getenv ("KEYTAPE_SNAPSHOT") != nullptr; }

    struct SmallButtonLook : juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colour (0xff26272b).brighter (down ? 0.25f : over ? 0.12f : 0.0f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return legendFont ((float) h * 0.48f, false); }
    };
    SmallButtonLook& buttonLook()
    {
        static SmallButtonLook l;
        return l;
    }

    /** A tape with a few takes on it, for pictures. */
    void demoTape (Machine& m)
    {
        const double sr = m.getSampleRate();
        auto take = [&] (int track, double start, double len, float hz) {
            auto a = std::make_shared<Audio>();
            a->sampleRate = sr;
            const int n = (int) (len * sr);
            a->data.setSize (2, n);
            for (int i = 0; i < n; ++i)
            {
                const float v = 0.3f * std::sin (6.2831853f * hz * (float) i / (float) sr) * std::exp (-(float) std::fmod ((double) i / sr, 0.5) * 4.0f);
                a->data.setSample (0, i, v);
                a->data.setSample (1, i, v);
            }
            Take t;
            t.audio = a;
            t.start = (int64_t) (start * sr);
            t.length = n;
            m.tape.reel.tracks[(size_t) track].takes.push_back (t);
        };
        take (0, 0.0, 8.0, 110.0f);
        take (1, 2.0, 4.0, 220.0f);
        take (1, 6.5, 3.5, 330.0f);
        take (2, 4.0, 6.0, 440.0f);
        take (3, 0.5, 1.5, 660.0f);
        take (3, 3.0, 2.0, 550.0f);
        m.tape.reel.name = "sketch 1";
        m.tape.reel.loopIn = (int64_t) (4.0 * sr);
        m.tape.reel.loopOut = (int64_t) (8.0 * sr);
        m.tape.reel.loopOn = true;
        m.tape.locate ((int64_t) (5.2 * sr));
        m.tape.track = 1;
        m.press (Key::Tape);
        m.release (Key::Tape);
    }
} // namespace

MainComponent::MainComponent (AudioEngine& e) : engine (e), panel (e)
{
    addAndMakeVisible (panel);
    for (auto* b : { &audioBtn, &helpBtn, &folderBtn })
    {
        b->setLookAndFeel (&buttonLook());
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8d8d4));
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    audioBtn.onClick = [this] { showAudioSettings(); };
    helpBtn.onClick = [this] { showHelp(); };
    folderBtn.onClick = [this] { engine.getFolder().reveal(); };
    engine.onMessage = [this] (const juce::String& s) {
        message = s;
        messageUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint();
    };
    loadSettings();
    setSize (1380, 493 + kBar);
    if (auto* forced = std::getenv ("KEYTAPE_WINDOW")) // testing aid: a window size ("1400x540")
    {
        const auto wh = juce::StringArray::fromTokens (forced, "x", {});
        if (wh.size() == 2)
            setSize (wh[0].getIntValue(), wh[1].getIntValue());
    }
    startTimerHz (4);
    if (auto* snap = std::getenv ("KEYTAPE_SNAPSHOT")) // testing aid: the panel to a PNG, then quit
    {
        const juce::File f (snap);
        const float scale = std::getenv ("KEYTAPE_SNAPSHOT_SCALE") != nullptr ? (float) std::atof (std::getenv ("KEYTAPE_SNAPSHOT_SCALE")) : 1.0f;
        const juce::String demo = std::getenv ("KEYTAPE_SNAPSHOT_DEMO") != nullptr ? std::getenv ("KEYTAPE_SNAPSHOT_DEMO") : "";
        juce::Timer::callAfterDelay (900, [this, demo] {
            if (demo == "tape")
                engine.withMachine ([] (Machine& m) { demoTape (m); });
            else if (demo.isNotEmpty())
                engine.withMachine ([demo] (Machine& m) {
                    const std::pair<const char*, Key> keys[] = { { "drum", Key::Drum }, { "mixer", Key::Mixer }, { "tempo", Key::Tempo }, { "seq", Key::Seq } };
                    for (auto& [n, k] : keys)
                        if (demo == n)
                        {
                            m.press (k);
                            m.release (k);
                        }
                    if (demo == "env" || demo == "fx" || demo == "lfo")
                    {
                        const Key k = demo == "env" ? Key::T2 : demo == "fx" ? Key::T3 : Key::T4;
                        m.press (k);
                        m.release (k);
                    }
                    m.keyDown (12, 0.8f); // (something on the scope)
                });
        });
        juce::Timer::callAfterDelay (1600, [safe = juce::Component::SafePointer<MainComponent> (this), f, scale] {
            if (safe == nullptr)
                return;
            safe->panel.refreshNow();
            const auto img = safe->panel.createComponentSnapshot (safe->panel.getLocalBounds(), true, scale);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            os.flush();
            safe->engine.withMachine ([] (Machine& m) { m.keyUp (12); });
            juce::JUCEApplicationBase::quit();
        });
    }
    if (auto* report = std::getenv ("KEYTAPE_SELFTEST"))
    {
        const juce::File f (report);
        juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<MainComponent> (this), f] {
            if (safe != nullptr)
                safe->runSelfTest (f);
        });
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.onMessage = nullptr;
    for (auto* b : { &audioBtn, &helpBtn, &folderBtn })
        b->setLookAndFeel (nullptr);
}

void MainComponent::prepareToQuit() { saveSettings(); }

void MainComponent::resized()
{
    // the panel keeps its proportions, centred above the bar
    auto area = getLocalBounds();
    auto bar = area.removeFromBottom (kBar);
    const float sc = std::min (area.getWidth() / (float) Panel::kWidth, area.getHeight() / (float) Panel::kHeight);
    const float ox = (area.getWidth() - Panel::kWidth * sc) * 0.5f, oy = (area.getHeight() - Panel::kHeight * sc) * 0.5f;
    panel.setTransform (juce::AffineTransform::scale (sc).translated (ox, oy));
    bar.reduce (10, 5);
    audioBtn.setBounds (bar.removeFromLeft (120));
    bar.removeFromLeft (8);
    folderBtn.setBounds (bar.removeFromLeft (80));
    helpBtn.setBounds (bar.removeFromRight (70));
}

void MainComponent::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0d0d0e)); }

void MainComponent::paintOverChildren (juce::Graphics& g)
{
    if (message.isEmpty() || juce::Time::getMillisecondCounter() > messageUntil)
        return;
    auto r = getLocalBounds().removeFromBottom (kBar).reduced (getWidth() / 4, 5).toFloat();
    g.setColour (juce::Colour (0xe0202125));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white);
    g.setFont (legendFont (14.0f, false));
    g.drawText (message, r, juce::Justification::centred);
}

void MainComponent::mouseDown (const juce::MouseEvent&) { panel.grabKeyboardFocus(); }

void MainComponent::timerCallback()
{
    if (! panel.hasKeyboardFocus (true) && isShowing() && getPeer() != nullptr && getPeer()->isFocused())
        panel.grabKeyboardFocus();
    if (message.isNotEmpty() && juce::Time::getMillisecondCounter() > messageUntil)
    {
        message = {};
        repaint();
    }
}

void MainComponent::showAudioSettings()
{
    auto* content = new juce::Component();
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(), 0, 4, 1, 2, true, false, true, false);
    selector->setSize (560, 470);
    auto* outLabel = new juce::Label ({}, "MIDI out:");
    auto* outBox = new juce::ComboBox();
    outBox->addItem ("(the Keytape port only)", 1);
    const auto outs = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < outs.size(); ++i)
    {
        outBox->addItem (outs[i].name, i + 2);
        if (outs[i].identifier == engine.getMidiOutputIdentifier())
            outBox->setSelectedId (i + 2, juce::dontSendNotification);
    }
    if (outBox->getSelectedId() == 0)
        outBox->setSelectedId (1, juce::dontSendNotification);
    outBox->onChange = [this, outBox, outs] {
        const int id = outBox->getSelectedId();
        engine.setMidiOutput (id >= 2 ? outs[id - 2].identifier : juce::String());
    };
    auto* note = new juce::Label ({}, "Inputs 1-2: mic / line in. Inputs 3-4: USB audio (a second pair). Outputs 1-2: the output. "
                                      "MIDI from every input and the \"Keytape\" port plays it; clock and ctrl mode go out.");
    note->setFont (juce::FontOptions (12.0f));
    note->setColour (juce::Label::textColourId, juce::Colours::grey);
    content->addAndMakeVisible (selector);
    content->addAndMakeVisible (outLabel);
    content->addAndMakeVisible (outBox);
    content->addAndMakeVisible (note);
    selector->setTopLeftPosition (0, 0);
    outLabel->setBounds (16, 476, 80, 24);
    outBox->setBounds (100, 476, 440, 24);
    note->setBounds (16, 506, 540, 36);
    content->setSize (560, 550);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (content);
    o.dialogTitle = "Audio / MIDI settings";
    o.dialogBackgroundColour = juce::Colour (0xff1d1f24);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::NoIcon, "Keytape KT-4",
        "A portable synthesizer around a 4-track tape: 13 synth engines, a drum machine and a sampler, 8 effects, 6 LFOs, "
        "7 sequencers, a mixer with EQ and drive, and the tape, with its tricks and edits.\n\n"
        "Keys: click them. Shift + a key or an encoder is its second function; help + a key explains it; shift + help opens the guide.\n"
        "Encoders: drag or scroll to turn, click to tap. The volume knob: drag.\n\n"
        "Computer keyboard: A W S E D R F G Y H U J K O L P ; [ ' play the keys from F. Shift is shift, 1-8 the sound keys, "
        "the arrows the arrow keys, space play, return record, escape stop, / help.\n"
        "Hold option and move the mouse to tilt Keytape (the g-force LFO); shake the mouse to shake it.\n\n"
        "Files live in Music/Keytape: tapes (a WAV per track), mixdowns, snapshots, samples, and a radio folder whose audio files "
        "become stations on the radio.");
}

void MainComponent::loadSettings()
{
    if (testing())
        return; // the self-test and snapshots leave the user's settings alone
    juce::PropertiesFile::Options o;
    o.applicationName = "Keytape";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.folderName = "Keytape";
    settings = std::make_unique<juce::PropertiesFile> (o);
    if (auto xml = settings->getXmlValue ("audioDevice"))
        engine.getDeviceManager().initialise (4, 2, xml.get(), true);
}

void MainComponent::saveSettings()
{
    if (settings == nullptr)
        return;
    if (auto xml = engine.getDeviceManager().createStateXml())
        settings->setValue ("audioDevice", xml.get());
    settings->saveIfNeeded();
}

// ======================================================================== self-test

void MainComponent::runSelfTest (const juce::File& reportFile)
{
    auto lines = std::make_shared<juce::StringArray>();
    auto failures = std::make_shared<int> (0);
    auto log = [lines] (const juce::String& s) { lines->add (s); };
    auto fail = [lines, failures] (const juce::String& s) {
        lines->add ("FAIL  " + s);
        ++*failures;
    };
    const auto results = panel.selfTestControls();
    for (auto& r : results.first)
        log (r);
    for (auto& r : results.second)
        fail (r);
    auto machine = [this] (std::function<void (Machine&)> fn) { engine.withMachine ([&] (Machine& m) { fn (m); }); };
    const auto root = engine.getFolder().getRoot();
    struct Step
    {
        int ms;
        std::function<void()> fn;
    };
    auto steps = std::make_shared<std::vector<Step>>();
    // a key of the keyboard sounds
    steps->push_back ({ 100, [=] { machine ([] (Machine& m) { m.keyDown (12, 0.8f); }); } });
    steps->push_back ({ 500, [=, this] {
                           const float l = engine.getOutputLevel();
                           l < 0.02f ? fail ("the keyboard isn't heard (" + juce::String (l, 4) + ")") : log ("ok    the keyboard plays: peak " + juce::String (l, 3));
                           machine ([] (Machine& m) {
                               m.keyUp (12);
                               m.press (Key::Tape);
                               m.release (Key::Tape);
                               m.press (Key::Record);
                               m.release (Key::Record);
                               m.keyDown (7, 0.8f);
                           });
                       } });
    // record a take, stop, play it back
    steps->push_back ({ 1200, [=] {
                           machine ([] (Machine& m) {
                               m.keyUp (7);
                               m.press (Key::Stop);
                               m.release (Key::Stop);
                           });
                       } });
    steps->push_back ({ 1200, [=] {
                           int takes = 0;
                           machine ([&] (Machine& m) {
                               takes = (int) m.tape.reel.tracks[0].takes.size();
                               m.tape.locate (0);
                               m.press (Key::Play);
                               m.release (Key::Play);
                           });
                           takes == 0 ? fail ("record made no take") : log ("ok    recorded a take on track 1");
                       } });
    steps->push_back ({ 500, [=, this] {
                           const float l = engine.getOutputLevel();
                           l < 0.02f ? fail ("the take doesn't play back (" + juce::String (l, 4) + ")") : log ("ok    the take plays back: peak " + juce::String (l, 3));
                           machine ([] (Machine& m) {
                               m.press (Key::Stop);
                               m.release (Key::Stop);
                           });
                       } });
    // the tape is written to the folder
    steps->push_back ({ 2600, [=, this] {
                           engine.saveAll();
                           const auto f = root.getChildFile ("tapes/tape 1/track 1.wav");
                           f.existsAsFile() && root.getChildFile ("tapes/tape 1/tape.json").existsAsFile() ? log ("ok    tape 1 saved: " + juce::String (f.getSize() / 1024) + " KB")
                                                                                                           : fail ("tape 1 wasn't saved");
                           // the mixdown: com, T4 records the output, T2 stops
                           machine ([] (Machine& m) {
                               m.press (Key::Com);
                               m.release (Key::Com);
                               m.press (Key::T4);
                               m.release (Key::T4);
                               m.keyDown (14, 0.8f);
                           });
                       } });
    steps->push_back ({ 800, [=] {
                           machine ([] (Machine& m) {
                               m.keyUp (14);
                               m.press (Key::T2);
                               m.release (Key::T2);
                           });
                       } });
    steps->push_back ({ 800, [=, this] {
                           engine.getFolder().flush();
                           const auto mixes = root.getChildFile ("mixdowns").findChildFiles (juce::File::findFiles, false, "*.wav");
                           mixes.isEmpty() ? fail ("no mixdown was written") : log ("ok    mixdown written: " + mixes[0].getFileName());
                           // a snapshot: a sound key held three seconds
                           machine ([] (Machine& m) {
                               m.press (Key::Synth);
                               m.release (Key::Synth);
                               m.press (Key::K2);
                           });
                       } });
    steps->push_back ({ 3500, [=, this] {
                           machine ([] (Machine& m) { m.release (Key::K2); });
                           const auto snaps = root.getChildFile ("snapshots/synth").findChildFiles (juce::File::findFiles, false, "*.json");
                           snaps.isEmpty() ? fail ("holding a sound key didn't save a snapshot") : log ("ok    snapshot saved: " + snaps[0].getFileName());
                           // the settings file
                           engine.saveAll();
                           engine.getSettingsFile().existsAsFile() ? log ("ok    settings saved") : fail ("the settings weren't saved");
                           lines->add ("");
                           lines->add (*failures == 0 ? juce::String ("ALL PASSED") : juce::String (*failures) + " FAILED");
                           reportFile.replaceWithText (lines->joinIntoString ("\n") + "\n");
                           juce::JUCEApplicationBase::quit();
                       } });
    struct Runner
    {
        static void next (std::shared_ptr<std::vector<Step>> s, size_t i, juce::Component::SafePointer<MainComponent> safe)
        {
            if (safe == nullptr || i >= s->size())
                return;
            juce::Timer::callAfterDelay ((*s)[i].ms, [s, i, safe] {
                if (safe == nullptr)
                    return;
                (*s)[i].fn();
                next (s, i + 1, safe);
            });
        }
    };
    Runner::next (steps, 0, this);
}
} // namespace keytape::ui
