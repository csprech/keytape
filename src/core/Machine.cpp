#include "Machine.h"

namespace keytape
{
namespace
{
    constexpr int kChunk = 32; // modulation and sequencer resolution (samples)
    // the radio's stations (MHz) and what they play: Keytape's own little broadcasts
    const double kStations[] = { 89.3, 94.7, 101.1, 105.5 };

    float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
    bool isTrick (Key k) { return k >= Key::K1 && k <= Key::K8; }
} // namespace

const char* keyName (Key k)
{
    static const char* n[] = { "help", "tempo", "(blank)", "synth", "drum", "tape", "mixer", "T1", "T2", "T3", "T4", "1", "2", "3", "4", "5", "6", "7", "8",
                               "sequencer", "input", "com", "lift", "drop", "split", "record", "play", "stop", "left", "right", "shift" };
    return n[(int) k];
}

Machine::Machine()
{
    // the factory sounds in the slots: one preset of eight engines, and the kits
    const Engine slots[kSlots] = { Engine::Shape, Engine::Fm, Engine::Swarm, Engine::String, Engine::Pulse, Engine::Dual, Engine::Bits, Engine::Sampler };
    for (int s = 0; s < kSlots; ++s)
    {
        const auto& lib = factorySynthPresets (slots[s]);
        if (! lib.empty())
            sounds.synth[(size_t) s] = lib.front().sound;
    }
    const auto& kits = factoryKits (DrumEngine::Kit);
    for (int s = 0; s < kSlots; ++s)
        sounds.drum[(size_t) s] = kits[(size_t) (s % (int) kits.size())].kit;
    for (int t = 0; t < kTapes; ++t)
        reels[(size_t) t].name = "tape " + juce::String (t + 1);
    tape.reel = reels[0];
    savedSynth = sounds.synth;
    savedDrum = sounds.drum;
    seqs[1].drum = true;
    for (int k = 0; k < kKeys; ++k)
        keyNote[(size_t) k] = -1;
}

void Machine::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = std::max (kChunk, block);
    synthHost.prepare (sr, kChunk);
    drumHost.prepare (sr, kChunk);
    masterFx.prepare (sr);
    eq.prepare (sr);
    drive.prepare (sr);
    for (auto& c : colour)
        c.prepare (sr);
    tape.prepare (sr, kChunk);
    for (auto* v : { &bufL, &bufR, &instL, &instR, &inL, &inR, &inMono, &recL, &recR, &outL, &outR })
        v->assign ((size_t) kChunk, 0.0f);
    for (auto& t : trackBuf)
        for (auto& c : t)
            c.assign ((size_t) kChunk, 0.0f);
    sampleBuf.setSize (2, (int) (20.0 * sr), false, true);
    radioFilter.set ((float) sr, 2500.0f, 0.7f);
    seqEvents.reserve (256);
    loadFactoryDrumSamples();
    // a sample for the sounds that have none (the sampler plays it until something is sampled): a short, original
    // pluck, one shared by them all
    if (defaultSample == nullptr || std::abs (defaultSample->sampleRate - sr) > 1.0)
    {
        auto a = std::make_shared<Audio>();
        a->sampleRate = sr;
        const int len = (int) (sr * 1.5);
        a->data.setSize (2, len);
        for (int i = 0; i < len; ++i)
        {
            const float t = (float) i / (float) sr;
            const float v = (std::sin (dsp::kTwoPi * 261.63f * t) + 0.4f * std::sin (dsp::kTwoPi * 523.25f * t + 0.3f) + 0.2f * std::sin (dsp::kTwoPi * 784.0f * t))
                            * std::exp (-t * 2.5f) * 0.5f;
            a->data.setSample (0, i, v);
            a->data.setSample (1, i, v * 0.95f);
        }
        defaultSample = a;
    }
    for (auto* list : { &sounds.synth, &savedSynth })
        for (auto& s : *list)
            if (s.sample == nullptr)
                s.sample = defaultSample;
}

void Machine::loadFactoryDrumSamples()
{
    // the drum sampler's kit: 24 KIT sounds rendered one after another into a single sample, sliced across the keys
    // (once for each sample rate)
    if (factoryDrumSample == nullptr || std::abs (factoryDrumSample->sampleRate - sr) > 1.0)
    {
        auto a = std::make_shared<Audio>();
        a->sampleRate = sr;
        const int slice = (int) (sr * 0.5);
        a->data.setSize (2, slice * kKeys);
        a->data.clear();
        const auto& kitPreset = factoryKits (DrumEngine::Kit).front().kit;
        for (int k = 0; k < kKeys; ++k)
        {
            dsp::DrumVoice v;
            v.prepare (sr);
            v.start (k, kitPreset, 0.9f);
            dsp::Mods mods;
            v.render (a->data.getWritePointer (0, k * slice), a->data.getWritePointer (1, k * slice), slice, kitPreset, mods);
        }
        factoryDrumSample = a;
    }
    const auto audio = factoryDrumSample;
    for (auto& d : sounds.drum)
        if (d.engine == DrumEngine::Sampler && d.sample == nullptr)
            d.sample = audio;
    for (auto& d : savedDrum)
        if (d.engine == DrumEngine::Sampler && d.sample == nullptr)
            d.sample = audio;
    // (slot 2 holds a sampled kit from the start; only the first time, so a kit put there stays)
    if (! factoryKitPlaced)
    {
        factoryKitPlaced = true;
        DrumKit sampled;
        sampled.name = "rendered";
        sampled.engine = DrumEngine::Sampler;
        sampled.sample = audio;
        sounds.drum[1] = sampled;
        savedDrum[1] = sampled;
    }
}

// ======================================================================== keys

void Machine::message (const juce::String& text, double seconds)
{
    popup = text;
    popupUntil = clock + (int64_t) (seconds * sr);
}

void Machine::press (Key k)
{
    if (down[(size_t) k])
        return;
    down[(size_t) k] = true;
    downAt[(size_t) k] = clock;
    const bool shift = down[(size_t) Key::Shift];
    // help + a key: what that key does
    if (down[(size_t) Key::Help] && k != Key::Help)
    {
        helpOverlay = helpFor (k);
        usedWhileDown = true;
        return;
    }
    if (k != Key::Help && k != Key::Shift)
        helpOverlay = {};
    usedWhileDown = false;
    switch (k)
    {
        case Key::Shift: return;
        case Key::Blank: return; // (no function, as on the original)
        case Key::Help:
            if (shift)
            {
                returnPage = page;
                page = Page::Guide;
                guidePage = 0;
            }
            else
                helpOverlay = screenHelp();
            return;
        case Key::Tempo:
            if (shift)
            {
                page = Page::SettingsPage;
                return;
            }
            if (page == Page::TempoPage && tempo.sync != Sync::MidiSync)
            {
                // tap tempo
                taps.push_back (clock);
                if (taps.size() > 4)
                    taps.erase (taps.begin());
                if (taps.size() >= 2)
                {
                    const double avg = (double) (taps.back() - taps.front()) / (double) (taps.size() - 1) / sr;
                    if (avg > 0.15 && avg < 2.0)
                        tempo.bpm = juce::jlimit (40.0, 240.0, std::round (60.0 / avg * 10.0) / 10.0);
                }
            }
            else
            {
                taps.clear();
                page = Page::TempoPage;
            }
            return;
        case Key::Synth:
        case Key::Drum:
            if (shift)
            {
                // revert preset: the active sound back to how it was saved
                if (k == Key::Synth)
                    sounds.synth[(size_t) sounds.synthSlot] = savedSynth[(size_t) sounds.synthSlot];
                else
                    sounds.drum[(size_t) sounds.drumSlot] = savedDrum[(size_t) sounds.drumSlot];
                message ("reverted");
                return;
            }
            modeKey (k == Key::Synth ? Mode::Synth : Mode::Drum);
            return;
        case Key::Tape:
            if (shift)
            {
                browseB = tapeIndex;
                browseA = (int) reels[(size_t) tapeIndex].style;
                openBrowser (Page::TapeBrowser);
                return;
            }
            modeKey (Mode::Tape);
            return;
        case Key::Mixer:
            if (shift)
            {
                page = Page::SoundPath;
                return;
            }
            modeKey (Mode::Mixer);
            return;
        case Key::T1:
        case Key::T2:
        case Key::T3:
        case Key::T4: trackKey ((int) k - (int) Key::T1); return;
        case Key::K1:
        case Key::K2:
        case Key::K3:
        case Key::K4:
        case Key::K5:
        case Key::K6:
        case Key::K7:
        case Key::K8:
        {
            const int i = (int) k - (int) Key::K1;
            if (mode == Mode::Tape || mode == Mode::Mixer)
                trick (i, true);
            else
                slotKey (i);
            return;
        }
        case Key::Seq:
            if (shift)
            {
                browseA = (int) seq().type;
                openBrowser (Page::SeqBrowser);
                return;
            }
            {
                auto& s = seq();
                s.enabled = ! s.enabled;
                if (! s.enabled)
                {
                    seqEvents.clear();
                    s.stopAll (seqEvents);
                    for (auto& e : seqEvents)
                        playNote (e.note, e.velocity, false);
                    seqEvents.clear();
                    if (page == Page::SequencerPage)
                        page = returnPage;
                }
                else
                {
                    if (page != Page::SequencerPage)
                        returnPage = page;
                    page = Page::SequencerPage;
                }
            }
            return;
        case Key::Input:
            if (shift)
            {
                page = Page::InputPage;
                return;
            }
            // in a sampler: get ready to sample (the next key held records); otherwise the input on or off
            if ((mode == Mode::Synth && synth().engine == Engine::Sampler) || (mode == Mode::Drum && drum().engine == DrumEngine::Sampler))
            {
                sampleArmed = ! sampleArmed;
                message (sampleArmed ? "hold a key to sample" : "sampling off");
            }
            else
                input.on = ! input.on;
            return;
        case Key::Com:
            if (shift)
            {
                page = Page::ComPage;
                ctrlMode = false;
            }
            else
                page = Page::OutputPage;
            return;
        case Key::Lift:
        case Key::Drop:
        case Key::Split: editKey (k); return;
        case Key::Record:
        case Key::Play:
        case Key::Stop: transport (k); return;
        case Key::Left: arrowKey (-1, true); return;
        case Key::Right: arrowKey (1, true); return;
        case Key::Count: return;
    }
}

void Machine::release (Key k)
{
    if (! down[(size_t) k])
        return;
    down[(size_t) k] = false;
    if (isTrick (k) && (mode == Mode::Tape || mode == Mode::Mixer))
        trick ((int) k - (int) Key::K1, false);
    if (k == Key::Left || k == Key::Right)
        arrowKey (k == Key::Left ? -1 : 1, false);
}

void Machine::modeKey (Mode m)
{
    mode = m;
    if (m == Mode::Synth || m == Mode::Drum)
    {
        instrument = m;
        page = Page::Engine;
    }
    else if (m == Mode::Tape)
        page = Page::TapePage;
    else
        page = Page::MixerPage;
    zoomed = false;
}

void Machine::trackKey (int t)
{
    const bool shift = down[(size_t) Key::Shift];
    switch (page)
    {
        case Page::Guide: guidePage = juce::jlimit (0, 7, guidePage + (t < 2 ? -1 : 1)); return;
        case Page::EraseConfirm:
            eraseKeys[(size_t) t] = true;
            if (std::all_of (eraseKeys.begin(), eraseKeys.end(), [] (bool b) { return b; }))
            {
                // all four pressed: the tape is wiped for good
                if (browseB == tapeIndex)
                    tape.eraseAll();
                else
                    reels[(size_t) browseB] = Reel();
                reels[(size_t) browseB].name = "tape " + juce::String (browseB + 1);
                eraseKeys = {};
                page = Page::TapePage;
                mode = Mode::Tape;
                message ("tape erased");
            }
            return;
        case Page::ComPage:
        case Page::MidiPage:
        case Page::CtrlPage:
        case Page::ListPage:
        case Page::DiskPage:
        {
            static const Page pages[] = { Page::MidiPage, Page::CtrlPage, Page::ListPage, Page::DiskPage };
            // the same key again goes back to the com screen
            page = page == pages[t] ? Page::ComPage : pages[t];
            ctrlMode = page == Page::CtrlPage;
            if (page == Page::DiskPage && library != nullptr)
                library->reveal();
            return;
        }
        case Page::OutputPage:
            if (outputMode == 0)
            {
                // mixdown: T1 play, T2 stop, T3 the next side, T4 record
                if (t == 3)
                {
                    // (six minutes of stereo for the side, made when first needed)
                    if (mixBuffer.getNumSamples() < (int) (6.0 * 60.0 * sr))
                        mixBuffer.setSize (2, (int) (6.0 * 60.0 * sr), false, true);
                    mixRecording = true;
                    mixLength = 0;
                }
                if (t == 1 && mixRecording)
                {
                    mixRecording = false;
                    mixPending = true;
                }
                if (t == 2)
                    side = 1 - side;
                if (t == 0)
                    message ("side " + juce::String (side == 0 ? "a" : "b"));
            }
            else
            {
                if (t == 0)
                    fmLive = ! fmLive;
                if (t == 2)
                    message ("no receiver found");
            }
            return;
        case Page::EngineBrowser:
            if (t == 0)
                confirmBrowser();
            return;
        case Page::FxBrowser:
            if (t == 2)
                confirmBrowser();
            return;
        case Page::LfoBrowser:
            if (t == 3)
                confirmBrowser();
            return;
        default: break;
    }
    if (mode == Mode::Tape)
    {
        tape.track = t;
        page = Page::TapePage;
        return;
    }
    if (mode == Mode::Mixer)
    {
        if (shift)
        {
            if (t == 2 && page == Page::MasterFxPage)
            {
                browseA = (int) mixer.fx.type;
                openBrowser (Page::FxBrowser);
                return;
            }
            mixer.mute[(size_t) t] = ! mixer.mute[(size_t) t];
            return;
        }
        static const Page pages[] = { Page::MixerPage, Page::EqPage, Page::MasterFxPage, Page::MasterOutPage };
        if (t == 2 && page == Page::MasterFxPage)
            mixer.fx.on = ! mixer.fx.on; // T3 again toggles the master effect
        else if (t == 2 && page != Page::MasterFxPage)
        {
            page = Page::MasterFxPage;
            mixer.fx.on = true;
        }
        else
            page = pages[t];
        return;
    }
    // synth and drum: the four modules (shift: their browsers)
    auto& fx = instrument == Mode::Drum ? drum().fx : synth().fx;
    auto& lfo = instrument == Mode::Drum ? drum().lfo : synth().lfo;
    if (shift)
    {
        if (t == 0)
        {
            browseA = instrument == Mode::Drum ? (int) drum().engine : (int) synth().engine;
            openBrowser (Page::EngineBrowser);
        }
        else if (t == 1)
            page = Page::Envelope; // (the play mode settings show while shift is held)
        else if (t == 2)
        {
            browseA = (int) fx.type;
            openBrowser (Page::FxBrowser);
        }
        else
        {
            browseA = (int) lfo.type;
            openBrowser (Page::LfoBrowser);
        }
        return;
    }
    static const Page pages[] = { Page::Engine, Page::Envelope, Page::Fx, Page::Lfo };
    if (t == 2)
    {
        if (page == Page::Fx)
            fx.on = ! fx.on; // pressed again: the effect on or off
        else
        {
            page = Page::Fx;
            fx.on = true;
        }
        return;
    }
    if (t == 3)
    {
        if (page == Page::Lfo)
            lfo.on = ! lfo.on;
        else
        {
            page = Page::Lfo;
            lfo.on = true;
        }
        return;
    }
    page = pages[t];
    zoomed = false;
}

void Machine::slotKey (int s)
{
    const bool shift = down[(size_t) Key::Shift];
    const bool drumMode = instrument == Mode::Drum;
    if (page == Page::PresetBrowser && s == browseSlot)
    {
        confirmBrowser(); // (the same key again loads the preset)
        return;
    }
    if (shift)
    {
        browseSlot = s;
        if (drumMode)
            sounds.drumSlot = s;
        else
            sounds.synthSlot = s;
        browseA = drumMode ? (int) drum().engine : (int) synth().engine;
        browseB = 0;
        openBrowser (Page::PresetBrowser);
        return;
    }
    synthHost.allOff();
    drumHost.allOff();
    if (drumMode)
        sounds.drumSlot = s;
    else
        sounds.synthSlot = s;
    if (page != Page::SequencerPage)
        page = Page::Engine;
}

void Machine::openBrowser (Page p)
{
    if (page != p)
        returnPage = page;
    page = p;
}

void Machine::confirmBrowser()
{
    const bool drumMode = instrument == Mode::Drum;
    switch (page)
    {
        case Page::PresetBrowser:
        {
            if (drumMode)
            {
                const int nEngines = (int) DrumEngine::Count;
                if (browseA >= nEngines)
                {
                    // a snapshot
                    if (library != nullptr)
                    {
                        const auto names = library->snapshots (true);
                        if (browseB < names.size())
                            library->loadSnapshot (true, names[browseB], nullptr, &drum());
                    }
                }
                else
                {
                    const auto& kits = factoryKits ((DrumEngine) browseA);
                    if (browseB < (int) kits.size())
                    {
                        auto sample = drum().sample;
                        drum() = kits[(size_t) browseB].kit;
                        if (drum().engine == DrumEngine::Sampler && drum().sample == nullptr)
                            drum().sample = sample != nullptr ? sample : savedDrum[1].sample;
                    }
                }
                savedDrum[(size_t) sounds.drumSlot] = drum();
            }
            else
            {
                if (browseA >= (int) Engine::Count)
                {
                    if (library != nullptr)
                    {
                        const auto names = library->snapshots (false);
                        if (browseB < names.size())
                            library->loadSnapshot (false, names[browseB], &synth(), nullptr);
                    }
                }
                else
                {
                    const auto& lib = factorySynthPresets ((Engine) browseA);
                    if (browseB < (int) lib.size())
                    {
                        auto sample = synth().sample;
                        synth() = lib[(size_t) browseB].sound;
                        synth().sample = sample;
                    }
                }
                savedSynth[(size_t) sounds.synthSlot] = synth();
            }
            page = Page::Engine;
            return;
        }
        case Page::EngineBrowser:
            if (drumMode)
            {
                const auto e = (DrumEngine) juce::jlimit (0, (int) DrumEngine::Count - 1, browseA);
                if (e != drum().engine)
                {
                    auto keep = drum();
                    drum() = factoryKits (e).front().kit;
                    drum().env = keep.env;
                    drum().fx = keep.fx;
                    drum().lfo = keep.lfo;
                    if (e == DrumEngine::Sampler)
                        drum().sample = keep.sample != nullptr ? keep.sample : savedDrum[1].sample;
                }
            }
            else
            {
                const auto e = (Engine) juce::jlimit (0, (int) Engine::Count - 1, browseA);
                if (e != synth().engine)
                {
                    // only the engine changes (envelope, FX and LFO stay); it starts from its first preset's settings
                    synth().engine = e;
                    const auto& lib = factorySynthPresets (e);
                    if (! lib.empty())
                        synth().eng = lib.front().sound.eng;
                }
            }
            page = Page::Engine;
            return;
        case Page::FxBrowser:
        {
            auto& fx = mode == Mode::Mixer ? mixer.fx : (drumMode ? drum().fx : synth().fx);
            fx.type = (Fx) juce::jlimit (0, (int) Fx::Count - 1, browseA);
            fx.on = true;
            page = mode == Mode::Mixer ? Page::MasterFxPage : Page::Fx;
            return;
        }
        case Page::LfoBrowser:
        {
            auto& lfo = drumMode ? drum().lfo : synth().lfo;
            lfo.type = (Lfo) juce::jlimit (0, (int) Lfo::Count - 1, browseA);
            lfo.on = true;
            page = Page::Lfo;
            return;
        }
        case Page::SeqBrowser:
        {
            auto& s = seq();
            seqEvents.clear();
            s.stopAll (seqEvents);
            seqEvents.clear();
            s.type = (Seq) juce::jlimit (0, (int) Seq::Count - 1, browseA);
            s.enabled = true;
            page = Page::SequencerPage;
            return;
        }
        default: return;
    }
}

void Machine::trick (int k, bool on)
{
    const bool shift = down[(size_t) Key::Shift];
    switch (k)
    {
        case 0: if (on) tape.setLoopIn(); break;
        case 1: if (on) tape.setLoopOut(); break;
        case 2:
            if (on)
            {
                if (shift)
                    tape.loopCurrentTake();
                else
                    tape.toggleLoop();
            }
            break;
        case 3: tape.setBreak (on); break;        // (the key shows the reels: a tape stop)
        case 4: tape.setTrickReverse (on); break; // (the key shows a mirrored letter)
        case 5: tape.setChop (on, tempo.bpm); break;
        case 6:
        case 7:
        {
            // memo: hold and turn an encoder to remember a value; press to recall it
            auto& m = memos[(size_t) (k - 6)];
            if (! on && ! usedWhileDown && m.set)
            {
                const Page keep = page;
                page = m.page;
                // recall by setting the parameter straight to the stored value
                auto set = [&] (float v) {
                    switch (m.page)
                    {
                        case Page::TapePage:
                            if (m.encoder == 3 && ! m.shifted)
                                tape.recLevel = v;
                            if (m.encoder == 2)
                                tape.speed = v;
                            break;
                        case Page::MixerPage:
                            (m.shifted ? mixer.pan : mixer.level)[(size_t) m.encoder] = v;
                            break;
                        case Page::EqPage: mixer.eq.p[(size_t) m.encoder] = v; break;
                        case Page::MasterFxPage: mixer.fx.m.p[(size_t) m.encoder] = v; break;
                        case Page::MasterOutPage: mixer.out.p[(size_t) m.encoder] = v; break;
                        default: break;
                    }
                };
                set (m.value);
                page = keep;
                message ("memo " + juce::String (k - 5));
            }
            break;
        }
        default: break;
    }
}

void Machine::transport (Key k)
{
    const bool shift = down[(size_t) Key::Shift];
    if (k == Key::Record)
    {
        if (shift)
            tape.arm();
        else
            tape.record();
        return;
    }
    if (k == Key::Play)
    {
        if (shift)
            tape.setReversePlay (! tape.getReversePlay());
        tape.play();
        for (auto& s : seqs)
            s.restart();
        return;
    }
    // stop
    if (shift)
    {
        tape.gridResolution = (tape.gridResolution + 1) % 4;
        static const char* names[] = { "grid off", "grid: bar", "grid: beat", "grid: half beat" };
        message (names[tape.gridResolution]);
        return;
    }
    tape.stop();
}

void Machine::editKey (Key k)
{
    const bool shift = down[(size_t) Key::Shift];
    if (mode == Mode::Tape || mode == Mode::Mixer)
    {
        if (k == Key::Lift)
        {
            if (shift ? tape.liftAll() : tape.lift())
            {
                lastLift = 2;
                message (shift ? "lifted all" : "lifted");
            }
        }
        else if (k == Key::Drop)
        {
            if (lastLift == 1 && soundClipboard.isObject())
            {
                tape.dropSoundData (soundClipboard, (int64_t) (sr * 0.5));
                message ("sound dropped");
            }
            else if (tape.drop())
                message ("dropped");
        }
        else if (shift ? tape.join() : tape.split())
            message (shift ? "joined" : "split");
        return;
    }
    // synth and drum: lift the sound, or drop a sound or what was lifted off the tape
    const bool drumMode = instrument == Mode::Drum;
    if (k == Key::Lift)
    {
        soundClipboard = drumMode ? kitToJson (drum()) : soundToJson (synth());
        soundClipboardDrum = drumMode;
        lastLift = 1;
        message ("sound lifted");
    }
    else if (k == Key::Drop)
    {
        if (lastLift == 2)
        {
            // a sound saved on tape comes back; tape audio goes into the sampler
            const auto& clip = tape.clipboard[(size_t) tape.track];
            if (clip && clip->soundData.isObject())
            {
                const bool isKit = clip->soundData.getProperty ("format", {}).toString() == "keytape-kit";
                if (isKit == drumMode)
                {
                    if (drumMode)
                        kitFromJson (clip->soundData, drum());
                    else
                        soundFromJson (clip->soundData, synth());
                    message ("sound recalled");
                }
                return;
            }
            auto mix = tape.clipboardMix();
            if (mix == nullptr)
                return;
            if (drumMode)
            {
                drum().engine = DrumEngine::Sampler;
                drum().sample = mix;
                for (int key = 0; key < kKeys; ++key)
                {
                    drum().keys[(size_t) key].p[1] = (float) key / kKeys;
                    drum().keys[(size_t) key].p[2] = (float) (key + 1) / kKeys;
                }
            }
            else
            {
                synth().engine = Engine::Sampler;
                synth().sample = mix;
                synth().eng.p = { 0.0f, 0.3f, 0.7f, 1.0f, 0.0f, 0.5f, 0.2f, 0.75f };
            }
            message ("dropped into the sampler");
        }
        else if (lastLift == 1 && soundClipboard.isObject() && soundClipboardDrum == drumMode)
        {
            if (drumMode)
                kitFromJson (soundClipboard, drum());
            else
                soundFromJson (soundClipboard, synth());
            message ("sound dropped");
        }
    }
}

void Machine::arrowKey (int dir, bool on)
{
    const bool shift = down[(size_t) Key::Shift];
    if (! on)
    {
        tape.shuttle (0);
        return;
    }
    if (down[(size_t) Key::Stop])
    {
        // stop + left / right: the start, or the end of the last take
        tape.locate (dir < 0 ? 0 : tape.reel.lastEnd());
        return;
    }
    if (page == Page::TempoPage)
    {
        tempo.bpm = juce::jlimit (40.0, 240.0, tempo.bpm + dir * 0.5); // nudge
        return;
    }
    if (shift)
    {
        if (seqActive() && seq().arrow (dir, true))
            return;
        if (tape.reel.loopOn)
            tape.moveLoopByBar (dir);
        else
            tape.jumpBar (dir);
        return;
    }
    if (mode == Mode::Tape || mode == Mode::Mixer)
    {
        tape.shuttle (dir);
        return;
    }
    auto& o = octave[instrument == Mode::Drum ? 1 : 0];
    o = juce::jlimit (-4, 4, o + dir);
    message ("octave " + juce::String (o > 0 ? "+" : "") + juce::String (o));
}

// ======================================================================== the keyboard

void Machine::playNote (int note, float velocity, bool on)
{
    if (instrument == Mode::Drum)
    {
        const int key = juce::jlimit (0, kKeys - 1, note - kLowestNote);
        if (on)
            drumHost.noteOn (key, velocity, drum());
        else
            drumHost.noteOff (key);
    }
    else
    {
        if (on)
            synthHost.noteOn (note, velocity, synth());
        else
            synthHost.noteOff (note, synth());
    }
}

void Machine::keyDown (int key, float velocity)
{
    key = juce::jlimit (0, kKeys - 1, key);
    const bool shift = down[(size_t) Key::Shift];
    const int note = instrument == Mode::Drum ? kLowestNote + key : kLowestNote + key + 12 * octave[0];
    keysHeld[(size_t) key] = true;
    keyNote[(size_t) key] = note;
    if (down[(size_t) Key::Help])
    {
        helpOverlay = instrument == Mode::Drum ? juce::String ("key ") + juce::String (key + 1) : juce::String (noteName (note));
        usedWhileDown = true;
    }
    if (ctrlMode)
    {
        pendingNotes.push_back ({ note, { 1, velocity } }); // (sent as MIDI, not played)
        return;
    }
    if (instrument == Mode::Drum)
        drumHost.lastKey = key;
    if (sampleArmed)
    {
        // sampling: this key records the input while it's held
        sampling = true;
        samplingKey = key;
        sampleLen = 0;
        sampleTriggered = false;
        return;
    }
    if (seqActive() && seq().keyDown (note, shift))
        return;
    playNote (note, velocity, true);
}

void Machine::keyUp (int key)
{
    key = juce::jlimit (0, kKeys - 1, key);
    const int note = keyNote[(size_t) key];
    keysHeld[(size_t) key] = false;
    keyNote[(size_t) key] = -1;
    if (note < 0)
        return;
    if (ctrlMode)
    {
        pendingNotes.push_back ({ note, { 0, 0.0f } });
        return;
    }
    if (sampling && key == samplingKey)
    {
        // the sample is taken: into the synth sampler, or this key's slice of the drum sampler
        sampling = false;
        sampleArmed = false;
        if (sampleLen > 16)
        {
            auto a = std::make_shared<Audio>();
            a->sampleRate = sr;
            a->data.setSize (2, sampleLen);
            for (int ch = 0; ch < 2; ++ch)
                a->data.copyFrom (ch, 0, sampleBuf, ch, 0, sampleLen);
            if (instrument == Mode::Drum)
            {
                drum().engine = DrumEngine::Sampler;
                drum().sample = a;
                auto& p = drum().keys[(size_t) key].p;
                p[1] = 0.0f;
                p[2] = 1.0f;
            }
            else
            {
                synth().engine = Engine::Sampler;
                synth().sample = a;
                synth().eng.p[0] = 0.0f;
                synth().eng.p[3] = 1.0f;
            }
            message ("sampled " + juce::String ((double) sampleLen / sr, 1) + " s");
        }
        return;
    }
    if (seqActive() && seq().keyUp (note, down[(size_t) Key::Shift]))
        return;
    playNote (note, 0.0f, false);
}

void Machine::shake()
{
    seqs[0].shake();
    seqs[1].shake();
}

void Machine::midiNote (int note, float velocity, bool on)
{
    // notes from outside play the instrument (and its sequencer), with their velocity
    if (on && seqActive() && seq().keyDown (note, false))
        return;
    if (! on && seqActive() && seq().keyUp (note, false))
        return;
    playNote (note, velocity, on);
}

void Machine::midiMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOnOrOff())
    {
        if (settings.notesIn && (settings.midiChannel == 0 || m.getChannel() == settings.midiChannel))
            midiNote (m.getNoteNumber(), m.getFloatVelocity(), m.isNoteOn());
        return;
    }
    if (m.isPitchWheel())
    {
        synthHost.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * (float) synth().bendRange());
        return;
    }
    if (m.isProgramChange())
    {
        const int p = m.getProgramChangeNumber();
        if (p < 8)
            sounds.synthSlot = p, instrument = Mode::Synth;
        else if (p < 16)
            sounds.drumSlot = p - 8, instrument = Mode::Drum;
        return;
    }
    if (m.isMidiStart() || m.isMidiContinue())
    {
        if (settings.clockIn)
        {
            if (m.isMidiStart())
                for (auto& s : seqs)
                    s.restart();
            tape.play();
        }
        return;
    }
    if (m.isMidiStop())
    {
        if (settings.clockIn)
            tape.stop();
        return;
    }
    if (m.isMidiClock())
    {
        if (settings.clockIn && tempo.sync == Sync::MidiSync)
        {
            if (lastClockAt >= 0)
            {
                const double dt = (double) (clock - lastClockAt) / sr;
                if (dt > 0.001 && dt < 0.5)
                    externalBpm = externalBpm <= 0.0 ? 60.0 / (dt * 24.0) : externalBpm * 0.9 + 0.1 * 60.0 / (dt * 24.0);
            }
            lastClockAt = clock;
            externalClock = true;
        }
        return;
    }
    if (! m.isController() || ! settings.otherIn)
        return;
    // the CC map (the guide's MIDI reference)
    const int c = m.getControllerNumber(), v = m.getControllerValue();
    const float f = (float) v / 127.0f;
    const int ch = m.getChannel();
    auto& s = synth();
    if ((c == 7 || c == 9 || c == 10) && ch >= 1 && ch <= 4)
    {
        if (c == 7)
            mixer.level[(size_t) ch - 1] = f;
        if (c == 9)
            mixer.mute[(size_t) ch - 1] = v >= 64;
        if (c == 10)
            mixer.pan[(size_t) ch - 1] = f;
        return;
    }
    if (c >= 1 && c <= 4) // (the MIDI LFO's CC 1-4)
        cc[(size_t) c - 1] = f;
    if (c >= 46 && c <= 49)
    {
        if (instrument == Mode::Drum)
            drum().keys[(size_t) drumHost.lastKey].p[(size_t) (c - 46)] = f;
        else
            s.eng.p[(size_t) (c - 46)] = f;
    }
    if (c >= 50 && c <= 53)
        (instrument == Mode::Drum ? drum().env : s.env).p[(size_t) (c - 50)] = f;
    if (c >= 54 && c <= 57)
        (instrument == Mode::Drum ? drum().fx : s.fx).m.p[(size_t) (c - 54)] = f;
    if (c >= 58 && c <= 61)
        (instrument == Mode::Drum ? drum().lfo : s.lfo).m.p[(size_t) (c - 58)] = f;
    if (c == 62 && v >= 64)
        for (auto& p : s.eng.p)
            p = juce::Random::getSystemRandom().nextFloat();
    if (c == 63 && v >= 64)
        s = savedSynth[(size_t) sounds.synthSlot];
    if (c == 64)
        synthHost.sustain (v >= 64);
    if (c >= 70 && c <= 73)
        mixer.fx.m.p[(size_t) (c - 70)] = f;
    if (c >= 74 && c <= 77)
        mixer.out.p[(size_t) std::min (3, c - 74)] = f;
    if (c == 78)
        tape.recLevel = f;
    if (c == 79)
        arrowKey (v < 64 ? -1 : 1, true);
    if (c == 80)
        tempo.bpm = v <= 5 ? 40.0 + v * 2.0 : v <= 120 ? 52.0 + (v - 6) : 168.0 + (v - 121) * 2.0;
    if (c == 81)
        tempo.metronome = f;
    if (c >= 82 && c <= 88 && v >= 64)
    {
        switch (c)
        {
            case 82: tape.jumpBar (-1); break;
            case 83: tape.jumpBar (1); break;
            case 84: tape.locate (0); break;
            case 85: tape.locate (tape.reel.lastEnd()); break;
            case 86: tape.setLoopIn(); break;
            case 87: tape.setLoopOut(); break;
            case 88: tape.toggleLoop(); break;
            default: break;
        }
    }
    if (c >= 90 && c <= 92)
        mixer.eq.p[(size_t) (c - 90)] = f;
    if (c == 93)
        instrument = v < 64 ? Mode::Synth : Mode::Drum;
    if (c == 102 && v >= 64 && ch >= 1 && ch <= 8)
        (instrument == Mode::Drum ? sounds.drumSlot : sounds.synthSlot) = ch - 1;
    if (c == 104 && v >= 64)
        tape.stop();
    if (c == 105 && v >= 64)
        tape.play();
    if (c == 123)
    {
        synthHost.allOff();
        drumHost.allOff();
    }
}

// ======================================================================== the encoders

Module* Machine::activeModule (bool& isFx, bool& isLfo)
{
    isFx = isLfo = false;
    const bool drumMode = instrument == Mode::Drum;
    switch (page)
    {
        case Page::Engine: return drumMode ? &drum().keys[(size_t) drumHost.lastKey] : &synth().eng;
        case Page::Envelope: return drumMode ? &drum().env : &synth().env;
        case Page::Fx: isFx = true; return drumMode ? &drum().fx.m : &synth().fx.m;
        case Page::Lfo: isLfo = true; return drumMode ? &drum().lfo.m : &synth().lfo.m;
        case Page::MasterFxPage: isFx = true; return &mixer.fx.m;
        default: return nullptr;
    }
}

void Machine::turn (int e, int delta)
{
    if (delta == 0)
        return;
    e = juce::jlimit (0, 3, e);
    const bool shift = down[(size_t) Key::Shift];
    helpOverlay = {};
    const float step = (float) delta * 0.01f;
    // a memo key held while turning: remember what this encoder ends up at
    auto memo = [&] (float value) {
        for (int k = 0; k < 2; ++k)
            if (down[(size_t) Key::K7 + (size_t) k] && (mode == Mode::Tape || mode == Mode::Mixer))
            {
                memos[(size_t) k] = { true, page, e, shift, value };
                usedWhileDown = true;
            }
    };
    switch (page)
    {
        case Page::Engine:
        case Page::Envelope:
        case Page::Fx:
        case Page::Lfo:
        {
            bool isFx = false, isLfo = false;
            auto* m = activeModule (isFx, isLfo);
            if (m == nullptr)
                return;
            const bool drumMode = instrument == Mode::Drum;
            // shift: the second four parameters, or fine tuning where the screen has none
            const bool fineOnly = page == Page::Engine && ! drumMode && engineParamName (synth().engine, e + 4) == "fine";
            const bool envFine = page == Page::Envelope && drumMode;
            const bool fxFine = isFx;
            const bool lfoFine = isLfo && lfoParamName ((drumMode ? drum().lfo : synth().lfo).type, e + 4) == "fine";
            if (shift && (fineOnly || envFine || fxFine || lfoFine))
            {
                m->p[(size_t) e] = clamp01 (m->p[(size_t) e] + step * 0.1f);
                return;
            }
            const int i = e + (shift ? 4 : 0);
            m->p[(size_t) i] = clamp01 (m->p[(size_t) i] + step);
            return;
        }
        case Page::PresetBrowser:
        {
            const bool drumMode = instrument == Mode::Drum;
            const int engines = (drumMode ? (int) DrumEngine::Count : (int) Engine::Count) + 1; // (+ snapshots)
            if (e == 0)
            {
                browseA = juce::jlimit (0, engines - 1, browseA + delta);
                browseB = 0;
            }
            if (e == 1)
            {
                int count = 0;
                if (browseA == engines - 1)
                    count = library != nullptr ? library->snapshots (drumMode).size() : 0;
                else
                    count = drumMode ? (int) factoryKits ((DrumEngine) browseA).size() : (int) factorySynthPresets ((Engine) browseA).size();
                browseB = juce::jlimit (0, std::max (0, count - 1), browseB + delta);
            }
            return;
        }
        case Page::EngineBrowser:
            if (e == 0)
                browseA = juce::jlimit (0, (instrument == Mode::Drum ? (int) DrumEngine::Count : (int) Engine::Count) - 1, browseA + delta);
            return;
        case Page::FxBrowser:
            if (e == 0)
                browseA = juce::jlimit (0, (int) Fx::Count - 1, browseA + delta);
            return;
        case Page::LfoBrowser:
            if (e == 0)
                browseA = juce::jlimit (0, (int) Lfo::Count - 1, browseA + delta);
            return;
        case Page::SeqBrowser:
            if (e == 0)
                browseA = juce::jlimit (0, (int) Seq::Count - 1, browseA + delta);
            return;
        case Page::TapeBrowser:
            if (e == 0)
            {
                // the selected tape's style
                auto& r = browseB == tapeIndex ? tape.reel : reels[(size_t) browseB];
                r.style = (TapeStyle) juce::jlimit (0, (int) TapeStyle::Count - 1, (int) r.style + delta);
                browseA = (int) r.style;
            }
            if (e == 1)
            {
                browseB = juce::jlimit (0, kTapes - 1, browseB + delta);
                browseA = (int) (browseB == tapeIndex ? tape.reel.style : reels[(size_t) browseB].style);
            }
            return;
        case Page::TapePage:
            if (e == 0)
            {
                if (shift)
                    tape.slide (delta);
                else
                    tape.scrub (delta);
            }
            if (e == 1)
            {
                // loop out (shift: loop in), moved a tenth of a second a click
                auto& pnt = shift ? tape.reel.loopIn : tape.reel.loopOut;
                if (pnt < 0)
                    pnt = (int64_t) tape.position();
                pnt = juce::jlimit<int64_t> (0, tape.tapeLength(), pnt + (int64_t) (delta * 0.1 * sr));
                tape.reel.dirty = true;
            }
            if (e == 2)
            {
                if (shift)
                {
                    // speed in steps of 25%
                    const double steps[] = { -2.0, -1.5, -1.0, -0.75, -0.5, -0.25, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0 };
                    int idx = 0;
                    for (int k = 0; k < 13; ++k)
                        if (std::abs (steps[k] - tape.speed) < std::abs (steps[idx] - tape.speed))
                            idx = k;
                    tape.speed = steps[juce::jlimit (0, 12, idx + (delta > 0 ? 1 : -1))];
                }
                else
                    tape.speed = juce::jlimit (0.1, 2.0, tape.speed + delta * 0.01);
                if (tempo.sync == Sync::BeatMatch)
                    tempo.bpm = juce::jlimit (40.0, 240.0, tape.speed * tape.reel.bpmAtNormal);
                memo ((float) tape.speed);
            }
            if (e == 3)
            {
                if (shift)
                {
                    if (tape.reel.loopOut < 0)
                        tape.reel.loopOut = (int64_t) tape.position();
                    tape.reel.loopOut = juce::jlimit<int64_t> (0, tape.tapeLength(), tape.reel.loopOut + (int64_t) (delta * 0.1 * sr));
                    tape.reel.dirty = true;
                }
                else
                {
                    tape.recLevel = clamp01 (tape.recLevel + step);
                    memo (tape.recLevel);
                }
            }
            return;
        case Page::MixerPage:
        {
            auto& v = shift ? mixer.pan[(size_t) e] : mixer.level[(size_t) e];
            v = clamp01 (v + step);
            memo (v);
            return;
        }
        case Page::EqPage:
            mixer.eq.p[(size_t) e] = clamp01 (mixer.eq.p[(size_t) e] + step);
            memo (mixer.eq.p[(size_t) e]);
            return;
        case Page::MasterFxPage:
            if (shift)
                mixer.fx.m.p[(size_t) e] = clamp01 (mixer.fx.m.p[(size_t) e] + step * 0.1f);
            else
                mixer.fx.m.p[(size_t) e] = clamp01 (mixer.fx.m.p[(size_t) e] + step);
            memo (mixer.fx.m.p[(size_t) e]);
            return;
        case Page::MasterOutPage:
            if (e <= 1)
            {
                // left and right: shift (or the link) moves both
                const bool both = shift || mixer.linked;
                mixer.out.p[(size_t) e] = clamp01 (mixer.out.p[(size_t) e] + step);
                if (both)
                    mixer.out.p[(size_t) (1 - e)] = mixer.out.p[(size_t) e];
            }
            else
                mixer.out.p[(size_t) e] = clamp01 (mixer.out.p[(size_t) e] + step);
            memo (mixer.out.p[(size_t) e]);
            return;
        case Page::TempoPage:
            if (e == 0 && tempo.sync != Sync::MidiSync)
            {
                tempo.bpm = juce::jlimit (40.0, 240.0, tempo.bpm + delta * (shift ? 0.1 : 1.0));
                if (tempo.sync == Sync::BeatMatch)
                    tape.speed = tempo.bpm / tape.reel.bpmAtNormal; // (the tape follows the tempo)
            }
            if (e == 1)
            {
                if (shift && tempo.sync == Sync::PoSync)
                    tempo.sixteenth = delta > 0;
                else
                    tempo.sync = (Sync) juce::jlimit (0, 3, (int) tempo.sync + delta);
            }
            if (e == 2)
            {
                tape.speed = juce::jlimit (0.1, 2.0, tape.speed + delta * 0.01);
                if (tempo.sync == Sync::BeatMatch)
                    tempo.bpm = juce::jlimit (40.0, 240.0, tape.speed * tape.reel.bpmAtNormal);
            }
            if (e == 3)
                tempo.metronome = clamp01 (tempo.metronome + step);
            return;
        case Page::SettingsPage:
            if (e == 0)
                settingsItem = juce::jlimit (0, 2, settingsItem + delta);
            if (e == 1)
                settingsParam = juce::jlimit (0, settingsItem == 1 ? 2 : 0, settingsParam + delta);
            if (e == 2)
            {
                if (settingsItem == 1 && settingsParam == 0)
                    settings.brightness = juce::jlimit (0.1f, 1.0f, settings.brightness + step);
                if (settingsItem == 1 && settingsParam == 1)
                    settings.delayedPowerOff = delta > 0;
                if (settingsItem == 2)
                    settings.tuningCents = juce::jlimit (-100.0f, 100.0f, settings.tuningCents + (float) delta);
                settingsDirty = true;
            }
            return;
        case Page::InputPage:
            if (e == 0)
                input.source = (Input) juce::jlimit (0, 3, (int) input.source + delta);
            if (e == 1)
            {
                if (input.source == Input::Radio)
                    input.fmMHz = juce::jlimit (87.5, 108.0, std::round ((input.fmMHz + delta * 0.1) * 10.0) / 10.0);
                else if (input.source == Input::MicLine)
                    input.stereo = delta > 0;
            }
            if (e == 2)
                input.threshold = clamp01 (input.threshold + step);
            if (e == 3)
                input.gain = clamp01 (input.gain + step);
            return;
        case Page::OutputPage:
            if (e == 0)
                outputMode = juce::jlimit (0, 1, outputMode + delta);
            if (e == 1 && outputMode == 1)
                input.fmMHz = juce::jlimit (87.5, 108.0, std::round ((input.fmMHz + delta * 0.1) * 10.0) / 10.0);
            return;
        case Page::MidiPage:
            if (e == 0)
                settings.midiChannel = juce::jlimit (0, 16, settings.midiChannel + delta);
            if (e == 1)
            {
                const int v = juce::jlimit (0, 3, (settings.clockIn ? 1 : 0) + (settings.clockOut ? 2 : 0) + delta);
                settings.clockIn = (v & 1) != 0;
                settings.clockOut = (v & 2) != 0;
            }
            if (e == 2)
            {
                const int v = juce::jlimit (0, 3, (settings.notesIn ? 1 : 0) + (settings.notesOut ? 2 : 0) + delta);
                settings.notesIn = (v & 1) != 0;
                settings.notesOut = (v & 2) != 0;
            }
            if (e == 3)
                settings.otherIn = delta > 0;
            settingsDirty = true;
            return;
        case Page::CtrlPage:
            if (e == 0)
                settings.ctrlChannel = juce::jlimit (1, 16, settings.ctrlChannel + delta);
            if (e == 1)
                settings.ctrlRelative = delta > 0;
            if (e == 2)
                octave[0] = juce::jlimit (-4, 4, octave[0] + delta);
            settingsDirty = true;
            return;
        case Page::SequencerPage: seq().turn (e, delta, shift); return;
        case Page::Guide: guidePage = juce::jlimit (0, 7, guidePage + delta); return;
        default: return;
    }
}

void Machine::tap (int e)
{
    e = juce::jlimit (0, 3, e);
    const bool shift = down[(size_t) Key::Shift];
    switch (page)
    {
        case Page::Engine:
            // the sampler: zoom in around the encoder's point; tap again for the whole view
            if ((instrument == Mode::Synth && synth().engine == Engine::Sampler) || (instrument == Mode::Drum && drum().engine == DrumEngine::Sampler))
                zoomed = ! zoomed;
            if (instrument == Mode::Drum && drum().engine == DrumEngine::Sampler && e == 3 && ! shift)
                drum().stacked = ! drum().stacked;
            return;
        case Page::Lfo:
        {
            auto& lfo = instrument == Mode::Drum ? drum().lfo : synth().lfo;
            if (lfo.type == Lfo::Value && e == 0)
                lfo.m.p[6] = std::fmod (lfo.m.p[6] + 0.25f + 0.001f, 1.0f); // the shape
            if (lfo.type == Lfo::Random && e == 2)
                lfo.m.p[6] = std::fmod (lfo.m.p[6] + 0.25f + 0.001f, 1.0f); // the parameter
            return;
        }
        case Page::PresetBrowser:
        case Page::EngineBrowser:
        case Page::SeqBrowser:
            if (e == 0)
                confirmBrowser();
            return;
        case Page::FxBrowser:
        case Page::LfoBrowser:
            if (e == 0)
                confirmBrowser();
            return;
        case Page::TapeBrowser:
            if (e == 0)
            {
                // rename: the next name from a short list of words
                static const char* words[] = { "sketch", "demo", "jam", "loop", "song", "idea", "take", "tape" };
                auto& r = browseB == tapeIndex ? tape.reel : reels[(size_t) browseB];
                int idx = 0;
                for (int k = 0; k < 8; ++k)
                    if (r.name.startsWith (words[k]))
                        idx = k + 1;
                r.name = juce::String (words[idx % 8]) + " " + juce::String (browseB + 1);
            }
            if (e == 1)
                selectTape (browseB);
            if (e == 2)
            {
                // new: an empty tape in this place
                if (browseB == tapeIndex)
                {
                    tape.eraseAll();
                    tape.reel = Reel();
                }
                else
                    reels[(size_t) browseB] = Reel();
                (browseB == tapeIndex ? tape.reel : reels[(size_t) browseB]).name = "tape " + juce::String (browseB + 1);
                selectTape (browseB);
            }
            if (e == 3)
            {
                eraseKeys = {};
                page = Page::EraseConfirm;
            }
            return;
        case Page::MixerPage: mixer.mute[(size_t) e] = ! mixer.mute[(size_t) e]; return;
        case Page::EqPage: mixer.eqOn[(size_t) e] = ! mixer.eqOn[(size_t) e]; return;
        case Page::MasterOutPage:
            if (e <= 1)
                mixer.linked = ! mixer.linked;
            return;
        case Page::TempoPage:
            if (e == 3)
                tempo.metronomeSound = (tempo.metronomeSound + 1) % 4;
            return;
        case Page::InputPage:
            if (e == 1 && input.source == Input::Radio)
            {
                // scan: on to the next station
                std::vector<double> freqs;
                if (stations.empty())
                    freqs.assign (std::begin (kStations), std::end (kStations));
                else
                    for (auto& s : stations)
                        freqs.push_back (s.mhz);
                std::sort (freqs.begin(), freqs.end());
                double next = freqs.front();
                for (double s : freqs)
                    if (s > input.fmMHz + 0.05)
                    {
                        next = s;
                        break;
                    }
                input.fmMHz = next;
            }
            return;
        case Page::ComPage:
            if (e == 0)
                bleAdvertising = ! bleAdvertising;
            if (e == 3)
                settings.usbCharging = ! settings.usbCharging;
            return;
        case Page::OutputPage:
            if (e == 0)
                outputMode = 1 - outputMode;
            return;
        case Page::SequencerPage: seq().tap (e, shift); return;
        default: return;
    }
}

void Machine::selectTape (int index)
{
    index = juce::jlimit (0, kTapes - 1, index);
    if (index == tapeIndex)
    {
        page = Page::TapePage;
        mode = Mode::Tape;
        return;
    }
    tape.stop();
    reels[(size_t) tapeIndex] = tape.reel;
    tapeIndex = index;
    tape.reel = reels[(size_t) index];
    tape.locate (0);
    page = Page::TapePage;
    mode = Mode::Tape;
    message (tape.reel.name);
}

// ======================================================================== audio

void Machine::renderMetronome (int n, float* L, float* R)
{
    // a click on each beat while the tape rolls (louder on the first of the bar), not recorded
    const double beatsPerSample = tempo.bpm / 60.0 / sr;
    static const float pitches[4] = { 1000.0f, 1600.0f, 600.0f, 2400.0f };
    for (int i = 0; i < n; ++i)
    {
        const double before = beatPhase;
        beatPhase += beatsPerSample;
        if (std::floor (beatPhase) != std::floor (before))
        {
            const bool down1 = ((int64_t) std::floor (beatPhase)) % 4 == 0;
            clickEnv = 1.0f;
            clickPitch = pitches[tempo.metronomeSound] * (down1 ? 1.5f : 1.0f);
            beatFlash = true;
            downbeatFlash = down1;
        }
        if (clickEnv > 0.0001f && tempo.metronome > 0.0f && tape.isPlaying())
        {
            clickPhase += clickPitch / (float) sr;
            const float v = std::sin (dsp::kTwoPi * clickPhase) * clickEnv * tempo.metronome * 0.5f;
            L[i] += v;
            R[i] += v;
        }
        clickEnv *= 0.9985f;
    }
}

void Machine::process (int n, const AudioIO& io, juce::MidiBuffer* midiIn, juce::MidiBuffer* midiOut)
{
    // MIDI arriving
    if (midiIn != nullptr)
        for (const auto meta : *midiIn)
            midiMessage (meta.getMessage());
    if (tempo.sync == Sync::MidiSync && externalClock && externalBpm > 0.0)
    {
        tempo.bpm = juce::jlimit (20.0, 300.0, externalBpm);
        if (clock - lastClockAt > (int64_t) sr) // (the clock went quiet)
            externalClock = false;
    }
    // notes the panel sent (ctrl mode) go out as MIDI
    if (midiOut != nullptr)
        for (auto& p : pendingNotes)
            midiOut->addEvent (p.second.first ? juce::MidiMessage::noteOn (settings.ctrlChannel, p.first, p.second.second)
                                              : juce::MidiMessage::noteOff (settings.ctrlChannel, p.first),
                               0);
    pendingNotes.clear();
    int done = 0;
    while (done < n)
    {
        const int m = std::min (kChunk, n - done);
        // ---- the input source
        for (int i = 0; i < m; ++i)
        {
            float l = 0, r = 0;
            switch (input.source)
            {
                case Input::MicLine:
                    l = io.micL != nullptr ? io.micL[done + i] : 0.0f;
                    r = io.micR != nullptr ? io.micR[done + i] : l;
                    if (! input.stereo)
                        l = r = 0.5f * (l + r);
                    break;
                case Input::Usb:
                    l = io.usbL != nullptr ? io.usbL[done + i] : 0.0f;
                    r = io.usbR != nullptr ? io.usbR[done + i] : l;
                    break;
                case Input::Resample:
                    l = outL[(size_t) i];
                    r = outR[(size_t) i];
                    break;
                case Input::Radio:
                {
                    // between stations, static; near one, its broadcast comes through (the files in the radio
                    // folder, or Keytape's own four little stations when there are none)
                    double nearest = 999.0;
                    int which = 0;
                    const int count = stations.empty() ? 4 : (int) stations.size();
                    for (int s = 0; s < count; ++s)
                    {
                        const double f = stations.empty() ? kStations[s] : stations[(size_t) s].mhz;
                        if (std::abs (f - input.fmMHz) < nearest)
                        {
                            nearest = std::abs (f - input.fmMHz);
                            which = s;
                        }
                    }
                    const float clarity = (float) juce::jlimit (0.0, 1.0, 1.0 - nearest / 0.25);
                    radioPh += 1.0 / sr;
                    const double t = radioPh;
                    float prog = 0.0f;
                    if (! stations.empty())
                    {
                        const auto& a = stations[(size_t) which].audio;
                        if (a != nullptr && a->length() > 1)
                        {
                            const double at = std::fmod (t * a->sampleRate, (double) a->length() - 1.0);
                            const int i0 = (int) at;
                            const float fr = (float) (at - i0);
                            const float* d0 = a->data.getReadPointer (0);
                            const float* d1 = a->data.getReadPointer (std::min (1, a->data.getNumChannels() - 1));
                            const float pl = d0[i0] + (d0[i0 + 1] - d0[i0]) * fr, pr = d1[i0] + (d1[i0 + 1] - d1[i0]) * fr;
                            const float hiss = radioNoise.next() * 0.25f;
                            l = pl * clarity + hiss * (1.0f - clarity * 0.9f);
                            r = pr * clarity + hiss * (1.0f - clarity * 0.9f);
                        }
                    }
                    else
                    switch (which)
                    {
                        case 0: prog = 0.3f * (float) (std::sin (6.2831853 * 220.0 * t) + std::sin (6.2831853 * 277.18 * t) + std::sin (6.2831853 * 329.63 * t)) * (0.6f + 0.4f * (float) std::sin (t * 0.7)); break;
                        case 1: prog = (std::fmod (t, 0.5) < 0.03 ? 0.8f * radioNoise.next() : 0.0f) + 0.5f * (float) std::sin (6.2831853 * 55.0 * t) * (float) std::exp (-std::fmod (t, 0.5) * 8.0); break;
                        case 2: prog = 0.4f * radioFilter.process (radioNoise.next()) * (0.5f + 0.5f * (float) std::sin (t * 9.0) * (float) std::sin (t * 2.3)); break;
                        default: prog = 0.3f * (float) std::sin (6.2831853 * 1000.0 * t) * (std::fmod (t, 1.0) < 0.5 ? 1.0f : 0.0f); break;
                    }
                    if (stations.empty())
                    {
                        const float hiss = radioNoise.next() * 0.25f;
                        l = r = prog * clarity + hiss * (1.0f - clarity * 0.9f);
                    }
                    break;
                }
            }
            const float g = juce::Decibels::decibelsToGain (input.gain * 31.0f - 6.0f);
            inL[(size_t) i] = l * g;
            inR[(size_t) i] = r * g;
            inMono[(size_t) i] = 0.5f * (inL[(size_t) i] + inR[(size_t) i]);
            inputLevel = std::max (std::abs (inMono[(size_t) i]), inputLevel * 0.9995f);
        }
        // ---- sampling from the input (above the threshold)
        if (sampling)
            for (int i = 0; i < m; ++i)
            {
                if (! sampleTriggered && std::abs (inMono[(size_t) i]) > input.threshold * 0.5f)
                    sampleTriggered = true;
                if (sampleTriggered && sampleLen < sampleBuf.getNumSamples())
                {
                    sampleBuf.setSample (0, sampleLen, inL[(size_t) i]);
                    sampleBuf.setSample (1, sampleLen, inR[(size_t) i]);
                    ++sampleLen;
                }
            }
        // ---- the sequencers
        seqEvents.clear();
        auto& sq = seqs[instrument == Mode::Drum ? 1 : 0];
        sq.process (m, sr, tempo.bpm, seqEvents);
        for (auto& ev : seqEvents)
            playNote (ev.note, ev.velocity, ev.on);
        // ---- the instruments
        LfoInputs lin;
        lin.inputLevel = inputLevel;
        lin.gforce = gforce;
        lin.sum = sumLevel;
        lin.cc = cc;
        lin.bpm = tempo.bpm;
        synthHost.process (m, instL.data(), instR.data(), synth(), inMono.data(), lin);
        drumHost.process (m, bufL.data(), bufR.data(), drum(), lin);
        for (int i = 0; i < m; ++i)
        {
            instL[(size_t) i] += bufL[(size_t) i];
            instR[(size_t) i] += bufR[(size_t) i];
            // what's recorded: the instruments, and the input when it's on
            recL[(size_t) i] = instL[(size_t) i] + (input.on ? inL[(size_t) i] : 0.0f);
            recR[(size_t) i] = instR[(size_t) i] + (input.on ? inR[(size_t) i] : 0.0f);
        }
        // ---- the tape (its speed follows the tempo in beat match, and the LFO)
        if (tempo.sync == Sync::BeatMatch)
            tape.speed = tempo.bpm / tape.reel.bpmAtNormal;
        const double keepSpeed = tape.speed;
        tape.speed = juce::jlimit (-2.0, 2.0, tape.speed + synthHost.tapeSpeedMod());
        std::array<std::array<float*, 2>, kTracks> outs;
        for (int t = 0; t < kTracks; ++t)
            outs[(size_t) t] = { trackBuf[(size_t) t][0].data(), trackBuf[(size_t) t][1].data() };
        tape.process (m, recL.data(), recR.data(), outs);
        tape.speed = keepSpeed;
        // ---- the mixer: each track's level, pan and mute; the selected track also carries what's played live
        std::fill (outL.begin(), outL.begin() + m, 0.0f);
        std::fill (outR.begin(), outR.begin() + m, 0.0f);
        for (int t = 0; t < kTracks; ++t)
        {
            float* L = trackBuf[(size_t) t][0].data();
            float* R = trackBuf[(size_t) t][1].data();
            colour[(size_t) t].process (tape.reel.style, L, R, m, (float) tape.speed);
            const bool live = t == tape.track && ! tape.isRecording() && ! tape.isArmed();
            const float lv = mixer.mute[(size_t) t] ? 0.0f : mixer.level[(size_t) t] * mixer.level[(size_t) t] * 1.4f;
            const float pan = mixer.pan[(size_t) t];
            const float gl = std::sqrt (1.0f - pan) * 1.41f * 0.707f, gr = std::sqrt (pan) * 1.41f * 0.707f;
            float peak = 0.0f;
            for (int i = 0; i < m; ++i)
            {
                float l = L[i], r = R[i];
                if (live || ((tape.isRecording() || tape.isArmed()) && t == tape.track))
                {
                    // (recording: the tape's output already has the take's old content; add what's played)
                    l += recL[(size_t) i] * (live ? tape.recLevel : tape.recLevel);
                    r += recR[(size_t) i] * (live ? tape.recLevel : tape.recLevel);
                }
                outL[(size_t) i] += l * lv * gl;
                outR[(size_t) i] += r * lv * gr;
                peak = std::max (peak, std::max (std::abs (l), std::abs (r)) * lv);
            }
            trackMeters[(size_t) t] = std::max (peak, trackMeters[(size_t) t] * 0.95f);
        }
        // ---- the master: EQ, the master effect, drive, left and right
        eq.process (mixer.eq, mixer.eqOn, outL.data(), outR.data(), m);
        if (mixer.fx.on)
            masterFx.process (mixer.fx.type, mixer.fx.m, outL.data(), outR.data(), m, tempo.bpm);
        drive.process (mixer.out.p[2], mixer.out.p[3], outL.data(), outR.data(), m);
        const float gL = mixer.out.p[0] * 1.25f, gR = mixer.out.p[1] * 1.25f;
        float peak = 0.0f;
        for (int i = 0; i < m; ++i)
        {
            outL[(size_t) i] *= gL;
            outR[(size_t) i] *= gR;
            peak = std::max (peak, std::max (std::abs (outL[(size_t) i]), std::abs (outR[(size_t) i])));
        }
        renderMetronome (m, outL.data(), outR.data());
        // ---- the mixdown records the output (side a or b)
        if (mixRecording)
            for (int i = 0; i < m && mixLength < mixBuffer.getNumSamples(); ++i, ++mixLength)
            {
                mixBuffer.setSample (0, (int) mixLength, outL[(size_t) i]);
                mixBuffer.setSample (1, (int) mixLength, outR[(size_t) i]);
            }
        // ---- sync out: the left channel carries the pulses, the right the mix in mono
        if (tempo.sync == Sync::PoSync)
        {
            const double pulsesPerSample = tempo.bpm / 60.0 * (tempo.sixteenth ? 4.0 : 2.0) / sr;
            for (int i = 0; i < m; ++i)
            {
                const double before = syncPulsePhase;
                syncPulsePhase += tape.isPlaying() ? pulsesPerSample : 0.0;
                const bool pulse = syncPulsePhase - std::floor (syncPulsePhase) < (double) (0.003 * sr) * pulsesPerSample && std::floor (syncPulsePhase) != std::floor (before - 1.0e-9);
                outR[(size_t) i] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
                outL[(size_t) i] = (syncPulsePhase - std::floor (syncPulsePhase)) < 0.003 * sr * pulsesPerSample && tape.isPlaying() ? 0.8f : 0.0f;
                juce::ignoreUnused (pulse);
            }
        }
        for (int i = 0; i < m; ++i)
        {
            if (io.outL != nullptr)
                io.outL[done + i] = outL[(size_t) i];
            if (io.outR != nullptr)
                io.outR[done + i] = outR[(size_t) i];
            // the scope keeps the last stretch of the output, for the screens
            if ((i & 3) == 0)
            {
                scope[(size_t) scopePos] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
                scopePos = (scopePos + 1) % (int) scope.size();
            }
        }
        meterLevel = std::max (peak, meterLevel * 0.97f);
        sumLevel = peak;
        // ---- MIDI out: the clock in beat match while the tape rolls
        if (midiOut != nullptr && settings.clockOut && tempo.sync == Sync::BeatMatch && tape.isPlaying())
        {
            const double ticksPerSample = tempo.bpm / 60.0 * 24.0 / sr;
            for (int i = 0; i < m; ++i)
            {
                const double before = clockTicks;
                clockTicks += ticksPerSample;
                if (std::floor (clockTicks) != std::floor (before))
                    midiOut->addEvent (juce::MidiMessage::midiClock(), done + i);
            }
        }
        done += m;
        clock += m;
    }
}

void Machine::idle()
{
    if (tape.takePendingTake())
        message ("tape full");
    if (popup.isNotEmpty() && clock > popupUntil)
        popup = {};
    // a sound key held for three seconds saves a snapshot
    if (mode == Mode::Synth || mode == Mode::Drum)
        for (int k = 0; k < kSlots; ++k)
        {
            const auto key = (size_t) Key::K1 + (size_t) k;
            if (down[key] && clock - downAt[key] > (int64_t) (3.0 * sr) && downAt[key] >= 0)
            {
                downAt[key] = std::numeric_limits<int64_t>::max() / 2; // (once)
                if (library != nullptr)
                    library->saveSnapshot (instrument == Mode::Drum, instrument == Mode::Drum ? nullptr : &synth(), instrument == Mode::Drum ? &drum() : nullptr);
                message ("snapshot saved");
            }
        }
    if (mixPending)
    {
        mixPending = false;
        if (library != nullptr && mixLength > 0)
            library->saveMixdown (side, mixBuffer, (int) mixLength, sr);
        message ("side " + juce::String (side == 0 ? "a" : "b") + " saved");
    }
}
} // namespace keytape
