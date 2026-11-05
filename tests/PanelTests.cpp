// Every key, the keyboard and the four encoders, as the panel uses them: the modes and their screens, the
// browsers, the tape's transport and tricks, the mixer, tempo, input, output, COM, MIDI.

#include "Rig.h"

namespace keytape::test
{
class PanelTests : public juce::UnitTest
{
public:
    PanelTests() : juce::UnitTest ("panel", "keytape") {}

    void runTest() override
    {
        beginTest ("the keyboard plays the synth; every sound key selects its sound");
        {
            Rig r;
            expect (r.run (0.2) < 1.0e-4f, "silent at rest");
            expect (r.play (12, 0.4, 1.0) > 0.02f, "a key sounds");
            for (int s = 0; s < kSlots; ++s)
            {
                r.tapKey ((Key) ((int) Key::K1 + s));
                expectEquals (r.m.sounds.synthSlot, s);
                const float p = r.play (12, 0.4, 0.8);
                expect (p > 0.01f, "sound " + juce::String (s + 1) + " plays: " + juce::String (p));
                expect (p < 2.0f);
            }
            expect (r.screen().title == juce::String (engineName (r.m.synth().engine)));
        }

        beginTest ("mode keys");
        {
            Rig r;
            r.tapKey (Key::Drum);
            expect (r.m.getMode() == Mode::Drum && r.m.getPage() == Page::Engine && r.m.instrument == Mode::Drum);
            expect (r.play (0, 0.2, 0.5) > 0.01f, "the drums play");
            r.tapKey (Key::Tape);
            expect (r.m.getMode() == Mode::Tape && r.m.getPage() == Page::TapePage);
            expect (r.play (0, 0.2, 0.5) > 0.01f, "the keyboard plays the last instrument in tape mode");
            r.tapKey (Key::Mixer);
            expect (r.m.getMode() == Mode::Mixer && r.m.getPage() == Page::MixerPage);
            r.tapKey (Key::Synth);
            expect (r.m.getMode() == Mode::Synth && r.m.instrument == Mode::Synth);
        }

        beginTest ("T1-T4 in synth mode: engine, envelope, effect and lfo (again: on and off); shift: the browsers");
        {
            Rig r;
            r.tapKey (Key::T2);
            expect (r.m.getPage() == Page::Envelope);
            r.m.synth().fx.on = false;
            r.tapKey (Key::T3);
            expect (r.m.getPage() == Page::Fx && r.m.synth().fx.on);
            r.tapKey (Key::T3);
            expect (! r.m.synth().fx.on, "T3 again: the effect off");
            r.tapKey (Key::T4);
            expect (r.m.getPage() == Page::Lfo && r.m.synth().lfo.on);
            r.tapKey (Key::T4);
            expect (! r.m.synth().lfo.on);
            r.tapKey (Key::T1);
            expect (r.m.getPage() == Page::Engine);
            // the engine browser
            r.shifted (Key::T1);
            expect (r.m.getPage() == Page::EngineBrowser);
            const auto before = r.m.synth().engine;
            r.m.turn (0, 1);
            r.m.tap (0);
            expect (r.m.getPage() == Page::Engine && r.m.synth().engine != before, "a new engine");
            // the effect browser
            r.shifted (Key::T3);
            expect (r.m.getPage() == Page::FxBrowser);
            r.m.turn (0, -10);
            r.m.tap (0);
            expect (r.m.synth().fx.type == Fx::Comb && r.m.synth().fx.on && r.m.getPage() == Page::Fx);
            r.shifted (Key::T4);
            expect (r.m.getPage() == Page::LfoBrowser);
            r.m.turn (0, 10);
            r.m.tap (0);
            expect (r.m.synth().lfo.type == Lfo::Velocity && r.m.getPage() == Page::Lfo);
            // shift + T2 shows the play mode settings
            r.shifted (Key::T2);
            expect (r.m.getPage() == Page::Envelope);
        }

        beginTest ("encoders turn the four parameters, shift the other four");
        {
            Rig r;
            const auto p = r.m.synth().eng.p;
            r.m.turn (0, 10);
            r.m.turn (1, -10);
            expectWithinAbsoluteError (r.m.synth().eng.p[0], juce::jlimit (0.0f, 1.0f, p[0] + 0.1f), 1.0e-5f);
            expectWithinAbsoluteError (r.m.synth().eng.p[1], juce::jlimit (0.0f, 1.0f, p[1] - 0.1f), 1.0e-5f);
            // envelope: shift turns play mode, portamento, bend range, volume
            r.tapKey (Key::T2);
            r.m.synth().env.p[4] = 0.0f;
            r.shiftTurn (0, 30);
            expect (r.m.synth().playMode() == PlayMode::Mono);
            auto d = r.screen();
            expect (d.params[0].visible && d.params[0].name == "attack");
            r.m.press (Key::Shift);
            d = r.screen();
            r.m.release (Key::Shift);
            expect (d.params[0].name == "play mode" && d.params[0].text == "mono");
            // each screen shows four parameters
            for (auto k : { Key::T1, Key::T2, Key::T3, Key::T4 })
            {
                r.tapKey (Key::T1); // (so T3 / T4 open their page rather than toggle)
                r.tapKey (k);
                d = r.screen();
                for (auto& pv : d.params)
                    expect (pv.visible && pv.name.isNotEmpty(), keyName (k));
            }
        }

        beginTest ("shift + a sound key: the preset browser; the same key again loads the preset");
        {
            Rig r;
            r.shifted (Key::K3);
            expect (r.m.getPage() == Page::PresetBrowser && r.m.sounds.synthSlot == 2);
            r.m.turn (0, -20);
            r.m.turn (0, (int) Engine::Fm);
            r.m.turn (1, 1);
            auto d = r.screen();
            expect (d.list.size() == (int) Engine::Count + 1 && d.list2.size() >= 2);
            r.tapKey (Key::K3);
            expect (r.m.getPage() == Page::Engine);
            expect (r.m.synth().engine == Engine::Fm && r.m.synth().name == factorySynthPresets (Engine::Fm)[1].name);
            // revert: shift + synth puts back what was loaded
            r.m.turn (0, 40);
            r.shifted (Key::Synth);
            expect (r.m.synth().eng == factorySynthPresets (Engine::Fm)[1].sound.eng, "reverted");
        }

        beginTest ("a sound key held three seconds saves a snapshot; the browser loads it");
        {
            Rig r;
            MemoryLibrary lib;
            r.m.setLibrary (&lib);
            r.m.synth().eng.p[0] = 0.321f;
            r.press (Key::K1);
            r.run (3.3);
            r.release (Key::K1);
            expectEquals (lib.synthNames.size(), 1);
            r.m.synth().eng.p[0] = 0.9f;
            r.shifted (Key::K1);
            r.m.turn (0, 40); // (past the engines: snapshots)
            r.m.tap (0);
            expectWithinAbsoluteError (r.m.synth().eng.p[0], 0.321f, 1.0e-6f);
            r.m.setLibrary (nullptr);
        }

        beginTest ("drum mode: each key its own sound; the encoders edit the last key played");
        {
            Rig r;
            r.tapKey (Key::Drum);
            r.m.sounds.drumSlot = 0;
            r.play (5, 0.05, 0.1);
            const auto before = r.m.drum().keys[5].p[0];
            r.m.turn (0, 5);
            expectWithinAbsoluteError (r.m.drum().keys[5].p[0], juce::jlimit (0.0f, 1.0f, before + 0.05f), 1.0e-5f);
            expectEquals (r.screen().drumKey, 5);
            // the sampled kit in slot 2
            r.tapKey (Key::K2);
            expect (r.m.drum().engine == DrumEngine::Sampler && r.m.drum().sample != nullptr);
            for (int k = 0; k < kKeys; k += 5)
                expect (r.play (k, 0.1, 0.2) > 0.01f, "sampled key " + juce::String (k));
        }

        beginTest ("arrows: octave in synth mode");
        {
            Rig r;
            r.tapKey (Key::Right);
            expectEquals (r.m.octave[0], 1);
            r.tapKey (Key::Left);
            r.tapKey (Key::Left);
            expectEquals (r.m.octave[0], -1);
            expect (r.m.popupText().contains ("octave"));
        }

        beginTest ("record, play, stop; shift + record arms; shift + play reverses; shift + stop the grid");
        {
            Rig r;
            r.tapKey (Key::Tape);
            r.tapKey (Key::Record);
            expect (r.m.tape.isRecording() && r.m.tape.isPlaying());
            r.m.keyDown (12);
            r.run (0.8);
            r.m.keyUp (12);
            r.run (0.4);
            r.tapKey (Key::Stop);
            expect (! r.m.tape.isPlaying());
            expectEquals ((int) r.m.tape.reel.tracks[0].takes.size(), 1);
            // back to the start (stop + left) and play: the take sounds
            r.press (Key::Stop);
            r.tapKey (Key::Left);
            r.release (Key::Stop);
            expectEquals (r.m.tape.position(), 0.0);
            r.tapKey (Key::Play);
            expect (r.run (0.6) > 0.01f, "the take plays back");
            r.tapKey (Key::Stop);
            r.shifted (Key::Play);
            expect (r.m.tape.getReversePlay() && r.m.tape.isPlaying());
            r.shifted (Key::Play);
            expect (! r.m.tape.getReversePlay());
            r.tapKey (Key::Stop);
            r.shifted (Key::Record);
            expect (r.m.tape.isArmed());
            r.tapKey (Key::Stop);
            expect (! r.m.tape.isArmed());
            const int g = r.m.tape.gridResolution;
            r.shifted (Key::Stop);
            expectEquals (r.m.tape.gridResolution, (g + 1) % 4);
            // stop + right: the end of the last take
            r.press (Key::Stop);
            r.tapKey (Key::Right);
            r.release (Key::Stop);
            expectEquals ((int64_t) r.m.tape.position(), r.m.tape.reel.lastEnd());
        }

        beginTest ("tape mode: T1-T4 select the track; the arrows wind the tape");
        {
            Rig r;
            r.tapKey (Key::Tape);
            r.tapKey (Key::T3);
            expectEquals (r.m.tape.track, 2);
            r.press (Key::Right);
            r.run (0.5);
            r.release (Key::Right);
            expect (r.m.tape.position() > kSr * 2.0, "fast forward");
            r.press (Key::Left);
            r.run (0.5);
            r.release (Key::Left);
            expect (r.m.tape.position() < kSr * 1.0, "rewind");
            // shift + arrows: bars
            r.m.tape.locate (0);
            r.m.press (Key::Shift);
            r.tapKey (Key::Right);
            r.m.release (Key::Shift);
            expectEquals ((int64_t) r.m.tape.position(), r.m.tape.barLength());
            // the encoders: speed, level
            r.m.turn (2, -50);
            expectWithinAbsoluteError (r.m.tape.speed, 0.5, 1.0e-9);
            r.shiftTurn (2, 1);
            expectWithinAbsoluteError (r.m.tape.speed, 0.75, 1.0e-9);
            r.m.turn (3, -10);
            expectWithinAbsoluteError (r.m.tape.recLevel, 0.7f, 1.0e-5f);
            r.m.tape.speed = 1.0;
            const auto d = r.screen();
            expect (d.params[2].name == "speed" && d.params[3].name == "level");
        }

        beginTest ("tape tricks on the sound keys: loop in, out, on; break, reverse, chop held; memos");
        {
            Rig r;
            r.tapKey (Key::Tape);
            r.m.tape.gridResolution = 0;
            r.m.tape.locate ((int64_t) kSr);
            r.tapKey (Key::K1);
            r.m.tape.locate ((int64_t) (kSr * 2));
            r.tapKey (Key::K2);
            expectEquals (r.m.tape.reel.loopIn, (int64_t) kSr);
            expectEquals (r.m.tape.reel.loopOut, (int64_t) kSr * 2);
            r.tapKey (Key::K3);
            expect (r.m.tape.reel.loopOn);
            r.tapKey (Key::K3);
            expect (! r.m.tape.reel.loopOn);
            r.tapKey (Key::Play);
            r.run (0.3);
            const double p0 = r.m.tape.position();
            r.press (Key::K5);
            r.run (0.2);
            expect (r.m.tape.position() < p0, "reverse while held");
            r.release (Key::K5);
            r.press (Key::K4);
            expect (r.screen().breaking);
            r.run (0.2);
            r.release (Key::K4);
            r.press (Key::K6);
            expect (r.screen().chopping);
            r.run (0.2);
            r.release (Key::K6);
            expect (! r.m.tape.isChopping() && ! r.m.tape.isBreaking());
            r.tapKey (Key::Stop);
            // memo: hold 7 and turn the level; later a press of 7 brings it back
            r.press (Key::K7);
            r.m.turn (3, -30);
            r.release (Key::K7);
            const float stored = r.m.tape.recLevel;
            r.m.turn (3, 20);
            r.tapKey (Key::K7);
            expectWithinAbsoluteError (r.m.tape.recLevel, stored, 1.0e-6f);
            // shift + 3: loop the take under the head
            r.tapKey (Key::Record);
            r.m.keyDown (12);
            r.run (0.5);
            r.m.keyUp (12);
            r.tapKey (Key::Stop);
            r.m.tape.locate ((int64_t) (r.m.tape.reel.tracks[0].takes[0].start + 100));
            r.shifted (Key::K3);
            expect (r.m.tape.reel.loopOn && r.m.tape.reel.loopIn == r.m.tape.reel.tracks[0].takes[0].start);
        }

        beginTest ("lift, drop, split (shift: join) in tape mode");
        {
            Rig r;
            r.tapKey (Key::Tape);
            r.m.tape.locate ((int64_t) (kSr * 10));
            r.tapKey (Key::Record);
            r.m.keyDown (12);
            r.run (1.0);
            r.m.keyUp (12);
            r.tapKey (Key::Stop);
            r.m.tape.locate ((int64_t) (kSr * 10.5));
            r.tapKey (Key::Split);
            expectEquals ((int) r.m.tape.reel.tracks[0].takes.size(), 2);
            r.shifted (Key::Split);
            expectEquals ((int) r.m.tape.reel.tracks[0].takes.size(), 1);
            r.tapKey (Key::Lift);
            expect (r.m.tape.reel.tracks[0].takes.empty());
            r.m.tape.locate ((int64_t) (kSr * 20));
            r.tapKey (Key::Drop);
            expectEquals (r.m.tape.reel.tracks[0].takes[0].start, (int64_t) (kSr * 20));
            // lift in tape, drop in the synth: the audio becomes the sampler's sample
            r.m.tape.locate ((int64_t) (kSr * 20.2));
            r.tapKey (Key::Lift);
            r.tapKey (Key::Synth);
            r.tapKey (Key::Drop);
            expect (r.m.synth().engine == Engine::Sampler && r.m.synth().sample != nullptr);
            expect (r.play (12, 0.3, 0.3) > 0.005f, "the lifted audio plays from the keyboard");
            // lift a sound in synth mode, drop it on the tape and back
            const auto name = r.m.synth().name;
            r.tapKey (Key::Lift);
            r.tapKey (Key::Tape);
            r.m.tape.locate ((int64_t) (kSr * 40));
            r.tapKey (Key::Drop);
            r.m.tape.locate ((int64_t) (kSr * 40) + 10);
            expect (r.m.tape.activeTake() != nullptr && r.m.tape.activeTake()->soundData.isObject(), "a sound on tape");
        }

        beginTest ("tape browser: shift + tape; the style, a new tape, load, erase with all four T keys");
        {
            Rig r;
            r.tapKey (Key::Tape);
            r.tapKey (Key::Record);
            r.m.keyDown (12);
            r.run (0.3);
            r.m.keyUp (12);
            r.tapKey (Key::Stop);
            r.shifted (Key::Tape);
            expect (r.m.getPage() == Page::TapeBrowser);
            r.m.turn (0, 1);
            expect (r.m.tape.reel.style == TapeStyle::Vintage);
            r.m.turn (1, 1);
            r.m.tap (1); // load tape 2
            expectEquals (r.m.tapeIndex, 1);
            expect (r.m.tape.reel.empty() && ! r.m.reels[0].empty());
            r.shifted (Key::Tape);
            r.m.turn (1, -1);
            r.m.tap (1);
            expectEquals (r.m.tapeIndex, 0);
            expect (! r.m.tape.reel.empty(), "tape 1 came back with its take");
            r.shifted (Key::Tape);
            r.m.tap (3);
            expect (r.m.getPage() == Page::EraseConfirm);
            r.tapKey (Key::T1);
            r.tapKey (Key::T2);
            r.tapKey (Key::T3);
            expect (! r.m.tape.reel.empty(), "not until all four");
            r.tapKey (Key::T4);
            expect (r.m.tape.reel.empty() && r.m.getPage() == Page::TapePage);
        }

        beginTest ("mixer: T1-T4 the four screens; shift + T mutes; the levels, eq, master effect and out");
        {
            Rig r;
            r.tapKey (Key::Mixer);
            r.m.turn (1, -20);
            expectWithinAbsoluteError (r.m.mixer.level[1], 0.6f, 1.0e-5f);
            r.shiftTurn (1, 10);
            expectWithinAbsoluteError (r.m.mixer.pan[1], 0.6f, 1.0e-5f);
            r.shifted (Key::T2);
            expect (r.m.mixer.mute[1]);
            r.m.tap (1);
            expect (! r.m.mixer.mute[1], "tap an encoder: mute");
            r.tapKey (Key::T2);
            expect (r.m.getPage() == Page::EqPage);
            r.m.turn (0, 10);
            expectWithinAbsoluteError (r.m.mixer.eq.p[0], 0.6f, 1.0e-5f);
            r.tapKey (Key::T3);
            expect (r.m.getPage() == Page::MasterFxPage && r.m.mixer.fx.on);
            r.tapKey (Key::T3);
            expect (! r.m.mixer.fx.on);
            r.tapKey (Key::T3);
            r.shifted (Key::T3);
            expect (r.m.getPage() == Page::FxBrowser);
            r.m.turn (0, 2);
            r.m.tap (0);
            expect (r.m.getPage() == Page::MasterFxPage);
            r.tapKey (Key::T4);
            expect (r.m.getPage() == Page::MasterOutPage);
            r.m.turn (0, -10);
            expectWithinAbsoluteError (r.m.mixer.out.p[1], r.m.mixer.out.p[0], 1.0e-6f); // (linked)
            // a muted track is silent
            r.tapKey (Key::Tape);
            r.tapKey (Key::Record);
            r.m.keyDown (12);
            r.run (0.5);
            r.m.keyUp (12);
            r.tapKey (Key::Stop);
            r.m.tape.locate (0);
            r.m.mixer.fx.on = false; // (no master effect tail)
            r.run (1.0);
            r.m.mixer.mute[0] = true;
            r.tapKey (Key::Play);
            expect (r.run (0.3) < 0.003f, "muted");
            r.m.mixer.mute[0] = false;
            r.m.tape.locate (0);
            expect (r.run (0.3) > 0.01f, "unmuted");
            // the sound path
            r.shifted (Key::Mixer);
            expect (r.m.getPage() == Page::SoundPath && r.screen().list.size() == 7);
        }

        beginTest ("tempo: the bpm, tap tempo, sync, metronome; beat match runs the tape at the tempo");
        {
            Rig r;
            r.tapKey (Key::Tempo);
            expect (r.m.getPage() == Page::TempoPage);
            r.m.turn (0, 10);
            expectWithinAbsoluteError (r.m.tempo.bpm, 130.0, 1.0e-9);
            expectWithinAbsoluteError (r.m.tape.speed, 130.0 / 120.0, 1.0e-9);
            r.tapKey (Key::Tempo);
            r.run (0.5);
            r.tapKey (Key::Tempo);
            r.run (0.5);
            r.tapKey (Key::Tempo);
            expectWithinAbsoluteError (r.m.tempo.bpm, 120.0, 1.5);
            r.m.turn (1, -1);
            expect (r.m.tempo.sync == Sync::Free);
            r.m.turn (2, 10);
            expectWithinAbsoluteError (r.m.tape.speed, 1.1, 0.02);
            r.m.turn (1, 1);
            r.m.turn (3, 50);
            expect (r.m.tempo.metronome > 0.4f);
            r.m.tape.speed = 1.0;
            r.tapKey (Key::Play);
            expect (r.run (1.0) > 0.05f, "the metronome clicks while the tape rolls");
            r.tapKey (Key::Stop);
            expect (r.run (0.5) < 1.0e-3f, "and stops with it");
            r.shifted (Key::Tempo);
            expect (r.m.getPage() == Page::SettingsPage);
        }

        beginTest ("help: help + key explains it; shift + help opens the guide; the blank key does nothing");
        {
            Rig r;
            r.press (Key::Help);
            r.tapKey (Key::Lift);
            r.release (Key::Help);
            expect (r.m.helpText().contains ("lift"));
            r.tapKey (Key::Help);
            expect (r.m.helpText().isNotEmpty());
            r.shifted (Key::Help);
            expect (r.m.getPage() == Page::Guide);
            r.tapKey (Key::T4);
            expectEquals (r.screen().listIndex, 1);
            r.tapKey (Key::Synth);
            const auto before = r.screen();
            r.tapKey (Key::Blank);
            const auto after = r.screen();
            expect (before.page == after.page && before.mode == after.mode);
            // help held: the battery
            r.press (Key::Help);
            r.run (1.0);
            expect (r.screen().showBattery);
            r.release (Key::Help);
        }

        beginTest ("input: the key turns it on; shift opens its screen; it's heard and recorded");
        {
            Rig r;
            r.input = [] (int64_t i) { return sine (i, 440.0f, 0.3f); };
            r.tapKey (Key::Tape);
            expect (r.run (0.2) < 1.0e-4f, "off: not heard");
            r.tapKey (Key::Input);
            expect (r.m.input.on);
            expect (r.run (0.2) > 0.05f, "on: heard");
            r.shifted (Key::Input);
            expect (r.m.getPage() == Page::InputPage);
            r.m.turn (0, 1);
            expect (r.m.input.source == Input::Radio);
            r.m.tap (1);
            expectWithinAbsoluteError (r.m.input.fmMHz, 101.1, 1.0e-6);
            expect (r.run (0.3) > 0.05f, "the radio plays its station");
            r.m.turn (0, -1);
            r.tapKey (Key::Tape);
            r.tapKey (Key::Record);
            r.run (0.5);
            r.tapKey (Key::Stop);
            r.tapKey (Key::Input);
            r.m.tape.locate (0);
            r.tapKey (Key::Play);
            expect (r.run (0.4) > 0.05f, "the input was recorded");
        }

        beginTest ("sampling: input in a sampler, then hold a key");
        {
            Rig r;
            r.input = [] (int64_t i) { return sine (i, 300.0f, 0.4f); };
            r.tapKey (Key::K8); // (the sampler sound)
            expect (r.m.synth().engine == Engine::Sampler);
            r.tapKey (Key::Input);
            r.m.keyDown (3);
            r.run (0.7);
            r.m.keyUp (3);
            expect (r.m.popupText().startsWith ("sampled"));
            expectWithinAbsoluteError ((double) r.m.synth().sample->length(), kSr * 0.7, kSr * 0.05);
            r.input = {};
            expect (r.play (12, 0.3, 0.2) > 0.05f, "the new sample plays");
        }

        beginTest ("com: the output screen (mixdown); shift: midi, ctrl, list, disk");
        {
            Rig r;
            MemoryLibrary lib;
            r.m.setLibrary (&lib);
            r.tapKey (Key::Com);
            expect (r.m.getPage() == Page::OutputPage);
            r.tapKey (Key::T4); // record the side
            r.m.keyDown (12);
            r.run (0.5);
            r.m.keyUp (12);
            r.tapKey (Key::T2);
            r.run (0.05);
            expectEquals (lib.mixdowns, 1);
            expectWithinAbsoluteError ((double) lib.mixLength, kSr * 0.5, (double) kBlock * 2);
            r.shifted (Key::Com);
            expect (r.m.getPage() == Page::ComPage);
            r.tapKey (Key::T1);
            expect (r.m.getPage() == Page::MidiPage);
            r.m.turn (0, 3);
            expectEquals (r.m.settings.midiChannel, 3);
            r.tapKey (Key::T1);
            expect (r.m.getPage() == Page::ComPage);
            r.tapKey (Key::T3);
            expect (r.screen().list.contains ("test device"));
            r.tapKey (Key::T4);
            expectEquals (lib.reveals, 1);
            // ctrl: the keyboard sends MIDI instead of playing
            r.tapKey (Key::T2);
            expect (r.m.getPage() == Page::CtrlPage);
            r.m.keyDown (12);
            r.run (0.05);
            r.m.keyUp (12);
            r.tapKey (Key::T2);
            r.m.setLibrary (nullptr);
        }

        beginTest ("the sequencer key; shift + sequencer opens the browser");
        {
            Rig r;
            r.tapKey (Key::Seq);
            expect (r.m.seqs[0].enabled && r.m.getPage() == Page::SequencerPage);
            r.m.keyDown (12);
            r.m.keyDown (16);
            const float p = r.run (1.0);
            r.m.keyUp (12);
            r.m.keyUp (16);
            expect (p > 0.01f, "the arpeggio plays");
            r.shifted (Key::Seq);
            expect (r.m.getPage() == Page::SeqBrowser);
            r.m.turn (0, (int) Seq::Grid);
            r.m.tap (0);
            expect (r.m.seqs[0].type == Seq::Grid);
            r.tapKey (Key::Seq);
            expect (! r.m.seqs[0].enabled);
        }

        beginTest ("MIDI in: notes, CCs, program change, clock");
        {
            Rig r;
            r.midiIn.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
            expect (r.run (0.3) > 0.01f, "a note from outside plays");
            r.midiIn.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
            r.run (1.0);
            r.midiIn.addEvent (juce::MidiMessage::controllerEvent (2, 7, 32), 0);
            r.midiIn.addEvent (juce::MidiMessage::controllerEvent (1, 46, 127), 0);
            r.midiIn.addEvent (juce::MidiMessage::programChange (1, 10), 0);
            r.run (0.01);
            expectWithinAbsoluteError (r.m.mixer.level[1], 32.0f / 127.0f, 1.0e-4f);
            expectEquals (r.m.sounds.drumSlot, 2);
            expect (r.m.instrument == Mode::Drum);
            r.m.tempo.sync = Sync::MidiSync;
            for (int i = 0; i < 96; ++i)
            {
                r.midiIn.addEvent (juce::MidiMessage::midiClock(), 0);
                r.run (60.0 / 100.0 / 24.0); // 100 bpm
            }
            expectWithinAbsoluteError (r.m.tempo.bpm, 100.0, 3.0);
            // MIDI out: the clock in beat match while the tape plays
            r.m.tempo.sync = Sync::BeatMatch;
            r.m.tempo.bpm = 120.0;
            r.clocksOut = 0;
            r.tapKey (Key::Play);
            r.run (1.0);
            r.tapKey (Key::Stop);
            expectWithinAbsoluteError (r.clocksOut, 48, 2);
        }

        beginTest ("sync out: pulses on the left, the mix on the right");
        {
            Rig r;
            r.m.tempo.sync = Sync::PoSync;
            r.tapKey (Key::Play);
            int pulses = 0;
            float last = 0;
            r.capture = true;
            r.run (1.0);
            for (float v : r.captured)
            {
                if (v > 0.5f && last <= 0.5f)
                    ++pulses;
                last = v;
            }
            expectWithinAbsoluteError (pulses, 4, 1); // eighths at 120
        }

        beginTest ("the display: every page describes itself without trouble");
        {
            Rig r;
            MemoryLibrary lib;
            r.m.setLibrary (&lib);
            for (auto k : { Key::Synth, Key::Drum, Key::Tape, Key::Mixer, Key::Tempo, Key::Com, Key::Input, Key::Seq })
            {
                r.tapKey (k);
                auto d = r.screen();
                expect (d.title.isNotEmpty() || d.list.size() > 0, keyName (k));
                r.shifted (k);
                d = r.screen();
                expect (d.title.isNotEmpty() || d.list.size() > 0, juce::String ("shift + ") + keyName (k));
                r.tapKey (Key::Synth);
            }
            r.m.setLibrary (nullptr);
        }
    }
};

static PanelTests panelTests;
} // namespace keytape::test
