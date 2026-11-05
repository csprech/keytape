// What the display shows: the current screen as data (titles, the four encoder parameters, lists, the tape,
// the mixer, the sequencer), and the help texts.

#include "Machine.h"

namespace keytape
{
namespace
{
    juce::String pct (float v) { return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v) * 99.0f)); }
    juce::String bip (float v)
    {
        const int x = juce::roundToInt ((v - 0.5f) * 198.0f);
        return (x > 0 ? "+" : "") + juce::String (x);
    }
    ParamView pv (const juce::String& name, float value, const juce::String& text)
    {
        ParamView p;
        p.name = name;
        p.value = juce::jlimit (0.0f, 1.0f, value);
        p.text = text;
        p.visible = true;
        return p;
    }
    const char* targetName (float v)
    {
        static const char* n[] = { "engine", "envelope", "effect", "main" };
        return n[juce::jlimit (0, 3, (int) (v * 3.999f))];
    }
    juce::String seconds (double s)
    {
        const int m = (int) (s / 60.0);
        return juce::String (m) + ":" + juce::String (s - m * 60.0, 1).paddedLeft ('0', 4);
    }
} // namespace

void Machine::describeParams (DisplayState& d) const
{
    const bool shift = down[(size_t) Key::Shift];
    const bool drumMode = instrument == Mode::Drum;
    auto& P = d.params;
    for (auto& p : P)
        p = {};
    const int off = shift ? 4 : 0;
    switch (page)
    {
        case Page::Engine:
            for (int e = 0; e < 4; ++e)
            {
                if (drumMode)
                {
                    const auto& m = drum().keys[(size_t) drumHost.lastKey];
                    const int i = e + off;
                    P[(size_t) e] = pv (drumParamName (drum().engine, i), m.p[(size_t) i], drum().engine == DrumEngine::Sampler && (i == 0 || i == 5) ? bip (m.p[(size_t) i]) : pct (m.p[(size_t) i]));
                }
                else
                {
                    const auto& s = synth();
                    const bool fine = shift && engineParamName (s.engine, e + 4) == "fine";
                    const int i = fine ? e : e + off;
                    P[(size_t) e] = pv (fine ? engineParamName (s.engine, e) + " (fine)" : engineParamName (s.engine, i), s.eng.p[(size_t) i], pct (s.eng.p[(size_t) i]));
                }
            }
            return;
        case Page::Envelope:
            for (int e = 0; e < 4; ++e)
            {
                const int i = drumMode && shift ? e : e + off;
                const auto& m = drumMode ? drum().env : synth().env;
                juce::String text = pct (m.p[(size_t) i]);
                if (! drumMode && i == 4)
                    text = playModeName (synth().playMode());
                if (! drumMode && i == 6)
                    text = juce::String (synth().bendRange()) + " st";
                if (drumMode && i == 3)
                    text = m.p[3] < 0.5f ? "early" : "late";
                P[(size_t) e] = pv (envParamName (drumMode, i), m.p[(size_t) i], text);
            }
            return;
        case Page::Fx:
        case Page::MasterFxPage:
        {
            const auto& fx = page == Page::MasterFxPage ? mixer.fx : (drumMode ? drum().fx : synth().fx);
            for (int e = 0; e < 4; ++e)
                P[(size_t) e] = pv (fxParamName (fx.type, e) + (shift ? " (fine)" : ""), fx.m.p[(size_t) e], pct (fx.m.p[(size_t) e]));
            return;
        }
        case Page::Lfo:
        {
            const auto& lfo = drumMode ? drum().lfo : synth().lfo;
            for (int e = 0; e < 4; ++e)
            {
                const int i = e + off;
                const auto name = lfoParamName (lfo.type, i);
                const float v = lfo.m.p[(size_t) i];
                juce::String text = pct (v);
                if (name.startsWith ("destination"))
                    text = targetName (v);
                else if (name == "parameter")
                {
                    const float destination = lfo.type == Lfo::Midi ? 0.0f : lfo.m.p[2];
                    text = targetParamName ((Target) juce::jlimit (0, 3, (int) (destination * 3.999f)), juce::jlimit (0, 3, (int) (v * 3.999f)), synth().engine,
                                            lfo.type == Lfo::Midi ? Fx::Delay : (drumMode ? drum().fx.type : synth().fx.type), drumMode);
                }
                else if (name == "source")
                {
                    static const char* src[] = { "input", "g-force", "envelope", "sum" };
                    text = src[juce::jlimit (0, 3, (int) (v * 3.999f))];
                }
                else if (name == "lfo shape")
                {
                    static const char* sh[] = { "sine", "saw", "exp", "square", "blip" };
                    text = sh[juce::jlimit (0, 4, (int) (v * 4.999f))];
                }
                else if (name == "amount" || name == "pitch amount" || name == "volume level" || name == "dest amount")
                    text = bip (v);
                P[(size_t) e] = pv (name, v, text);
            }
            return;
        }
        case Page::TapePage:
            P[0] = pv (shift ? "slide" : "scrub", (float) (tape.position() / (double) tape.tapeLength()), seconds (tape.position() / sr));
            P[1] = pv (shift ? "loop in" : "loop out", 0.5f, tape.reel.loopIn < 0 && tape.reel.loopOut < 0 ? juce::String ("--") : seconds ((double) (shift ? tape.reel.loopIn : tape.reel.loopOut) / sr));
            P[2] = pv ("speed", (float) ((tape.speed + 2.0) / 4.0), juce::String (juce::roundToInt (tape.speed * 100.0)) + "%");
            P[3] = shift ? pv ("loop out", 0.5f, tape.reel.loopOut < 0 ? juce::String ("--") : seconds ((double) tape.reel.loopOut / sr))
                         : pv ("level", tape.recLevel, pct (tape.recLevel));
            return;
        case Page::MixerPage:
            for (int t = 0; t < 4; ++t)
                P[(size_t) t] = shift ? pv ("pan " + juce::String (t + 1), mixer.pan[(size_t) t], bip (mixer.pan[(size_t) t]))
                                      : pv ("track " + juce::String (t + 1), mixer.level[(size_t) t], mixer.mute[(size_t) t] ? juce::String ("mute") : pct (mixer.level[(size_t) t]));
            return;
        case Page::EqPage:
        {
            static const char* n[] = { "low", "mid", "high", "amount" };
            for (int e = 0; e < 4; ++e)
                P[(size_t) e] = pv (n[e], mixer.eq.p[(size_t) e], ! mixer.eqOn[(size_t) e] ? juce::String ("off") : e < 3 ? bip (mixer.eq.p[(size_t) e]) : pct (mixer.eq.p[(size_t) e]));
            return;
        }
        case Page::MasterOutPage:
        {
            static const char* n[] = { "left", "right", "drive", "release" };
            for (int e = 0; e < 4; ++e)
                P[(size_t) e] = pv (n[e], mixer.out.p[(size_t) e], pct (mixer.out.p[(size_t) e]));
            return;
        }
        case Page::TempoPage:
            P[0] = pv (shift ? "bpm (fine)" : "bpm", (float) ((tempo.bpm - 40.0) / 200.0), tempo.sync == Sync::MidiSync ? juce::String ("ext") : juce::String (tempo.bpm, 1));
            P[1] = pv (tempo.sync == Sync::PoSync && shift ? "pulses" : "sync", (float) tempo.sync / 3.0f, tempo.sync == Sync::PoSync && shift ? juce::String (tempo.sixteenth ? "1/16" : "1/8") : juce::String (syncName (tempo.sync)));
            P[2] = pv ("tape speed", (float) (tape.speed / 2.0), juce::String (juce::roundToInt (tape.speed * 100.0)) + "%");
            {
                static const char* sounds[] = { "click", "wood", "low", "high" };
                P[3] = pv ("metronome", tempo.metronome, tempo.metronome <= 0.0f ? juce::String ("off") : juce::String (sounds[tempo.metronomeSound]) + " " + pct (tempo.metronome));
            }
            return;
        case Page::InputPage:
            P[0] = pv ("source", (float) input.source / 3.0f, inputName (input.source));
            if (input.source == Input::Radio)
                P[1] = pv ("frequency", (float) ((input.fmMHz - 87.5) / 20.5), juce::String (input.fmMHz, 1) + " MHz");
            else if (input.source == Input::MicLine)
                P[1] = pv ("channels", input.stereo ? 1.0f : 0.0f, input.stereo ? "stereo" : "mono");
            P[2] = pv ("threshold", input.threshold, pct (input.threshold));
            P[3] = pv ("gain", input.gain, juce::String (juce::roundToInt (input.gain * 31.0f - 6.0f)) + " dB");
            return;
        case Page::OutputPage:
            P[0] = pv ("output", (float) outputMode, outputMode == 0 ? "mixdown" : "fm radio");
            if (outputMode == 1)
                P[1] = pv ("frequency", (float) ((input.fmMHz - 87.5) / 20.5), juce::String (input.fmMHz, 1) + " MHz");
            return;
        case Page::MidiPage:
        {
            static const char* io[] = { "off", "in", "out", "in + out" };
            P[0] = pv ("channel", settings.midiChannel / 16.0f, settings.midiChannel == 0 ? juce::String ("all") : juce::String (settings.midiChannel));
            P[1] = pv ("clock", 0.5f, io[(settings.clockIn ? 1 : 0) + (settings.clockOut ? 2 : 0)]);
            P[2] = pv ("notes", 0.5f, io[(settings.notesIn ? 1 : 0) + (settings.notesOut ? 2 : 0)]);
            P[3] = pv ("other", settings.otherIn ? 1.0f : 0.0f, settings.otherIn ? "in" : "off");
            return;
        }
        case Page::CtrlPage:
            P[0] = pv ("channel", settings.ctrlChannel / 16.0f, juce::String (settings.ctrlChannel));
            P[1] = pv ("encoders", settings.ctrlRelative ? 1.0f : 0.0f, settings.ctrlRelative ? "relative" : "absolute");
            P[2] = pv ("octave", (octave[0] + 4) / 8.0f, juce::String (octave[0]));
            return;
        case Page::SettingsPage:
        {
            static const char* items[] = { "about", "screen", "tuning" };
            P[0] = pv ("item", settingsItem / 2.0f, items[settingsItem]);
            if (settingsItem == 1)
            {
                static const char* sub[] = { "brightness", "power off" };
                P[1] = pv ("setting", (float) settingsParam, sub[std::min (1, settingsParam)]);
                P[2] = settingsParam == 0 ? pv ("value", settings.brightness, pct (settings.brightness))
                                          : pv ("value", settings.delayedPowerOff ? 1.0f : 0.0f, settings.delayedPowerOff ? "delayed" : "instant");
            }
            if (settingsItem == 2)
                P[2] = pv ("cents", (settings.tuningCents + 100.0f) / 200.0f, juce::String (juce::roundToInt (settings.tuningCents)));
            return;
        }
        case Page::SequencerPage:
            for (int e = 0; e < 4; ++e)
                P[(size_t) e] = pv (seq().paramName (e + off), seq().paramValue (e), seq().paramText (e));
            return;
        default: return;
    }
}

void Machine::describe (DisplayState& d) const
{
    const bool drumMode = instrument == Mode::Drum;
    d.mode = mode;
    d.page = page;
    d.shift = down[(size_t) Key::Shift];
    d.help = helpOverlay;
    d.popup = popup;
    d.showBattery = down[(size_t) Key::Help] && clock - downAt[(size_t) Key::Help] > (int64_t) (0.6 * sr) && ! usedWhileDown;
    describeParams (d);
    // the scope, oldest first
    for (int i = 0; i < 256; ++i)
        d.scope[(size_t) i] = scope[(size_t) ((scopePos + i) % 256)];
    // ---- the sound
    const auto& s = synth();
    const auto& k = drum();
    d.engine = s.engine;
    d.drumEngine = k.engine;
    d.drumKey = drumHost.lastKey;
    d.engineParams = drumMode ? k.keys[(size_t) drumHost.lastKey].p : s.eng.p;
    d.envParams = drumMode ? k.env.p : s.env.p;
    const auto& fx = page == Page::MasterFxPage || mode == Mode::Mixer ? mixer.fx : (drumMode ? k.fx : s.fx);
    d.fxParams = fx.m.p;
    d.fxType = fx.type;
    d.fxOn = fx.on;
    const auto& lfo = drumMode ? k.lfo : s.lfo;
    d.lfoParams = lfo.m.p;
    d.lfoType = lfo.type;
    d.lfoOn = lfo.on;
    d.lfoShown = drumMode ? drumHost.lfoShown() : synthHost.lfoShown();
    d.slot = drumMode ? sounds.drumSlot : sounds.synthSlot;
    d.octave = octave[drumMode ? 1 : 0];
    d.soundName = drumMode ? k.name : s.name;
    d.keysDown = keysHeld;
    d.zoomed = zoomed;
    d.sampleOverview.clear();
    const AudioPtr sample = drumMode ? (k.engine == DrumEngine::Sampler ? k.sample : nullptr) : (s.engine == Engine::Sampler ? s.sample : nullptr);
    if (sample != nullptr && sample->length() > 0)
    {
        const int bins = 160, len = sample->length();
        d.sampleOverview.resize ((size_t) bins);
        for (int b = 0; b < bins; ++b)
        {
            const int from = (int) ((int64_t) len * b / bins), to = std::max (from + 1, (int) ((int64_t) len * (b + 1) / bins));
            float peak = 0.0f;
            for (int i = from; i < to; i += std::max (1, (to - from) / 32))
                peak = std::max (peak, std::abs (sample->data.getSample (0, i)));
            d.sampleOverview[(size_t) b] = peak;
        }
        if (drumMode)
        {
            const auto& m = k.keys[(size_t) drumHost.lastKey].p;
            d.samplePoints = { m[1], m[2], 0.0f, 0.0f };
        }
        else
            d.samplePoints = { s.eng.p[0], s.eng.p[1], s.eng.p[2], s.eng.p[3] };
    }
    // ---- the tape
    d.takes.clear();
    for (int t = 0; t < kTracks; ++t)
        for (const auto& take : tape.reel.tracks[(size_t) t].takes)
        {
            DisplayState::TakeView v;
            v.track = t;
            v.start = (double) take.start / sr;
            v.end = (double) take.end() / sr;
            v.active = t == tape.track && (int64_t) tape.position() >= take.start && (int64_t) tape.position() < take.end();
            v.sound = take.soundData.isObject();
            d.takes.push_back (v);
        }
    d.head = tape.position() / sr;
    d.length = (double) tape.tapeLength() / sr;
    d.loopIn = tape.reel.loopIn < 0 ? -1.0 : (double) tape.reel.loopIn / sr;
    d.loopOut = tape.reel.loopOut < 0 ? -1.0 : (double) tape.reel.loopOut / sr;
    d.barSeconds = (double) tape.barLength() / sr;
    d.loopOn = tape.reel.loopOn;
    d.playing = tape.isPlaying();
    d.recording = tape.isRecording();
    d.armed = tape.isArmed();
    d.reverse = tape.getReversePlay();
    d.breaking = tape.isBreaking();
    d.chopping = tape.isChopping();
    d.speed = tape.speed;
    d.recLevel = tape.recLevel;
    d.tapeLevel = tape.meter();
    d.track = tape.track;
    d.tapeIndex = tapeIndex;
    d.grid = tape.gridResolution;
    d.style = tape.reel.style;
    d.tapeName = tape.reel.name;
    // ---- the mixer
    d.levels = mixer.level;
    d.pans = mixer.pan;
    d.mutes = mixer.mute;
    d.trackMeters = trackMeters;
    d.eq = { mixer.eq.p[0], mixer.eq.p[1], mixer.eq.p[2], mixer.eq.p[3] };
    d.eqOn = mixer.eqOn;
    d.masterOut = { mixer.out.p[0], mixer.out.p[1], mixer.out.p[2], mixer.out.p[3] };
    d.gainReduction = drive.gainReduction();
    d.linked = mixer.linked;
    // ---- lists
    d.list.clear();
    d.list2.clear();
    d.listIndex = d.list2Index = 0;
    d.listTitle = {};
    switch (page)
    {
        case Page::PresetBrowser:
        {
            const int engines = drumMode ? (int) DrumEngine::Count : (int) Engine::Count;
            for (int e = 0; e < engines; ++e)
                d.list.add (drumMode ? drumEngineName ((DrumEngine) e) : engineName ((Engine) e));
            d.list.add ("snapshots");
            d.listIndex = browseA;
            if (browseA == engines)
            {
                if (library != nullptr)
                    d.list2 = library->snapshots (drumMode);
            }
            else if (drumMode)
                for (const auto& p : factoryKits ((DrumEngine) browseA))
                    d.list2.add (p.name);
            else
                for (const auto& p : factorySynthPresets ((Engine) browseA))
                    d.list2.add (p.name);
            d.list2Index = browseB;
            d.listTitle = "slot " + juce::String (browseSlot + 1);
            break;
        }
        case Page::EngineBrowser:
            for (int e = 0; e < (drumMode ? (int) DrumEngine::Count : (int) Engine::Count); ++e)
                d.list.add (drumMode ? drumEngineName ((DrumEngine) e) : engineName ((Engine) e));
            d.listIndex = browseA;
            d.listTitle = drumMode ? "drum engine" : "synth engine";
            break;
        case Page::FxBrowser:
            for (int e = 0; e < (int) Fx::Count; ++e)
                d.list.add (fxName ((Fx) e));
            d.listIndex = browseA;
            d.listTitle = "effect";
            break;
        case Page::LfoBrowser:
            for (int e = 0; e < (int) Lfo::Count; ++e)
                d.list.add (lfoName ((Lfo) e));
            d.listIndex = browseA;
            d.listTitle = "lfo";
            break;
        case Page::SeqBrowser:
            for (int e = 0; e < (int) Seq::Count; ++e)
                d.list.add (seqName ((Seq) e));
            d.listIndex = browseA;
            d.listTitle = "sequencer";
            break;
        case Page::TapeBrowser:
            for (int t = 0; t < kTapes; ++t)
            {
                const auto& r = t == tapeIndex ? tape.reel : reels[(size_t) t];
                d.list.add (r.name + (r.empty() ? " (empty)" : ""));
            }
            d.listIndex = browseB;
            for (int st = 0; st < (int) TapeStyle::Count; ++st)
                d.list2.add (tapeStyleName ((TapeStyle) st));
            d.list2Index = browseA;
            d.listTitle = "tapes";
            break;
        case Page::Guide:
        {
            static const char* pages[] = { "the keys", "synth", "drum", "tape", "mixer", "sequencers", "tempo and sync", "com and midi" };
            for (auto* p : pages)
                d.list.add (p);
            d.listIndex = guidePage;
            d.listTitle = "guide";
            break;
        }
        case Page::ComPage:
            d.list.add ("midi");
            d.list.add ("ctrl");
            d.list.add ("list");
            d.list.add ("disk");
            d.listTitle = "connections";
            break;
        case Page::ListPage:
            if (library != nullptr)
                d.list = library->midiDevices();
            d.listTitle = "midi devices";
            break;
        case Page::SoundPath:
            d.list.add (drumMode ? drumEngineName (k.engine) : engineName (s.engine));
            d.list.add ("envelope");
            d.list.add ((drumMode ? k.fx.on : s.fx.on) ? fxName ((drumMode ? k.fx : s.fx).type) : "(no effect)");
            d.list.add ("track " + juce::String (tape.track + 1));
            d.list.add ("eq");
            d.list.add (mixer.fx.on ? fxName (mixer.fx.type) : "(no master effect)");
            d.list.add ("master out");
            d.listTitle = "sound path";
            break;
        default: break;
    }
    // ---- the sequencer
    const auto& sq = seq();
    d.seqType = sq.type;
    d.seqOn = sq.enabled;
    d.seqStep = sq.currentStep();
    d.seqNotes = sq.type == Seq::Chain ? sq.chain : sq.held;
    d.seqGrid = sq.grid;
    d.seqDraw = sq.draw;
    d.seqHands = sq.hands[(size_t) juce::jlimit (0, 13, sq.handsKey)];
    d.balls.clear();
    for (const auto& b : sq.balls)
        d.balls.emplace_back (b.x, b.y);
    d.spinAngle = sq.spinAngle;
    d.spinSides = 3 + juce::roundToInt (sq.spinShape * 5.0f);
    d.seqCursor = sq.type == Seq::Grid ? sq.gridCursor : sq.type == Seq::Hands ? sq.handsCursor : sq.drawX;
    d.seqLength = sq.type == Seq::Grid ? sq.gridLength : sq.handsLength;
    d.drones = sq.drones;
    d.holdBreak = sq.holdBreak;
    // ---- tempo, input, output, com
    d.bpm = tempo.bpm;
    d.sync = tempo.sync;
    d.beatPos = beatPhase;
    d.beat = std::fmod (beatPhase, 1.0) < 0.12;
    d.downbeat = d.beat && ((int64_t) std::floor (beatPhase)) % 4 == 0;
    d.externalClock = externalClock;
    d.sixteenth = tempo.sixteenth;
    d.metronome = tempo.metronome;
    d.inputSource = input.source;
    d.inputOn = input.on;
    d.inputStereo = input.stereo;
    d.fmMHz = input.fmMHz;
    d.stations.clear();
    d.stationName = {};
    if (stations.empty())
    {
        static const double builtIn[] = { 89.3, 94.7, 101.1, 105.5 };
        static const char* names[] = { "chords", "beat", "talk", "test tone" };
        for (int st = 0; st < 4; ++st)
        {
            d.stations.push_back (builtIn[st]);
            if (std::abs (builtIn[st] - input.fmMHz) < 0.15)
                d.stationName = names[st];
        }
    }
    else
        for (const auto& st : stations)
        {
            d.stations.push_back (st.mhz);
            if (std::abs (st.mhz - input.fmMHz) < 0.15)
                d.stationName = st.name;
        }
    d.eraseKeys = eraseKeys;
    d.inputLevel = inputLevel;
    d.threshold = input.threshold;
    d.gain = input.gain;
    d.outputMode = outputMode;
    d.side = side;
    d.mixRecording = mixRecording;
    d.fmLive = fmLive;
    d.mixSeconds = (double) mixLength / sr;
    d.settings = settings;
    d.bleAdvertising = bleAdvertising;
    d.meter = meterLevel;
    // ---- titles
    switch (page)
    {
        case Page::Engine: d.title = drumMode ? drumEngineName (k.engine) : engineName (s.engine); break;
        case Page::Envelope: d.title = drumMode ? "drum envelope" : (d.shift ? "play mode" : "envelope"); break;
        case Page::Fx: d.title = fxName (fx.type); break;
        case Page::Lfo: d.title = lfoName (lfo.type); break;
        case Page::TapePage: d.title = tape.reel.name; break;
        case Page::MixerPage: d.title = "mixer"; break;
        case Page::EqPage: d.title = "eq"; break;
        case Page::MasterFxPage: d.title = fxName (mixer.fx.type); break;
        case Page::MasterOutPage: d.title = "master out"; break;
        case Page::TempoPage: d.title = "tempo"; break;
        case Page::SettingsPage: d.title = "settings"; break;
        case Page::InputPage: d.title = "input"; break;
        case Page::OutputPage: d.title = "output"; break;
        case Page::MidiPage: d.title = "midi"; break;
        case Page::CtrlPage: d.title = "ctrl"; break;
        case Page::DiskPage: d.title = "disk"; break;
        case Page::SequencerPage: d.title = seqName (sq.type); break;
        case Page::EraseConfirm: d.title = "erase tape?"; break;
        default: d.title = d.listTitle; break;
    }
    d.subtitle = page == Page::EraseConfirm ? juce::String ("press T1, T2, T3 and T4") : juce::String();
}

juce::String Machine::helpFor (Key k) const
{
    switch (k)
    {
        case Key::Help: return "help: hold for the battery; shift: the guide";
        case Key::Tempo: return "tempo: bpm, sync, metronome; tap to set; shift: settings";
        case Key::Blank: return "(this key has no function)";
        case Key::Synth: return "synth mode; shift: revert the preset";
        case Key::Drum: return "drum mode; shift: revert the kit";
        case Key::Tape: return "tape mode; shift: the tapes";
        case Key::Mixer: return "mixer mode; shift: the sound path";
        case Key::T1: return mode == Mode::Tape ? "track 1" : mode == Mode::Mixer ? "mixer levels; shift: mute 1" : "the engine; shift: engines";
        case Key::T2: return mode == Mode::Tape ? "track 2" : mode == Mode::Mixer ? "eq; shift: mute 2" : "the envelope; shift: play mode";
        case Key::T3: return mode == Mode::Tape ? "track 3" : mode == Mode::Mixer ? "master effect; shift: mute 3" : "the effect (again: on/off); shift: effects";
        case Key::T4: return mode == Mode::Tape ? "track 4" : mode == Mode::Mixer ? "master out; shift: mute 4" : "the lfo (again: on/off); shift: lfos";
        case Key::K1: return mode == Mode::Tape || mode == Mode::Mixer ? "loop in" : "sound 1; shift: presets; hold: save a snapshot";
        case Key::K2: return mode == Mode::Tape || mode == Mode::Mixer ? "loop out" : "sound 2; shift: presets; hold: save a snapshot";
        case Key::K3: return mode == Mode::Tape || mode == Mode::Mixer ? "loop on/off; shift: loop the take" : "sound 3; shift: presets";
        case Key::K4: return mode == Mode::Tape || mode == Mode::Mixer ? "break: a tape stop (hold)" : "sound 4; shift: presets";
        case Key::K5: return mode == Mode::Tape || mode == Mode::Mixer ? "reverse (hold)" : "sound 5; shift: presets";
        case Key::K6: return mode == Mode::Tape || mode == Mode::Mixer ? "chop (hold)" : "sound 6; shift: presets";
        case Key::K7: return mode == Mode::Tape || mode == Mode::Mixer ? "memo 1: hold and turn to store, press to recall" : "sound 7; shift: presets";
        case Key::K8: return mode == Mode::Tape || mode == Mode::Mixer ? "memo 2: hold and turn to store, press to recall" : "sound 8; shift: presets";
        case Key::Seq: return "sequencer on/off; shift: sequencers";
        case Key::Input: return "input on/off (in a sampler: sample); shift: input settings";
        case Key::Com: return "output: mixdown, fm; shift: midi, ctrl, list, disk";
        case Key::Lift: return mode == Mode::Tape || mode == Mode::Mixer ? "lift the take (or loop); shift: lift all tracks" : "lift the sound";
        case Key::Drop: return "drop what was lifted";
        case Key::Split: return "split the take; shift: join";
        case Key::Record: return "record; shift: arm (record with the reels still)";
        case Key::Play: return "play; shift: reverse";
        case Key::Stop: return "stop; shift: the grid; with arrows: start or end";
        case Key::Left:
        case Key::Right: return mode == Mode::Tape || mode == Mode::Mixer ? "wind the tape; shift: bars, or move the loop" : "octave; shift: bars, or move the loop";
        case Key::Shift: return "shift: the second function of each key and encoder";
        case Key::Count: break;
    }
    return {};
}

juce::String Machine::screenHelp() const
{
    switch (page)
    {
        case Page::Engine: return "turn the four encoders; shift for the second four";
        case Page::Envelope: return instrument == Mode::Drum ? "attack, gain, release, timing" : "attack, decay, sustain, release; shift: play mode, glide, bend, volume";
        case Page::Fx: return "T3 again turns the effect off; shift: fine";
        case Page::Lfo: return "T4 again turns the lfo off";
        case Page::PresetBrowser: return "blue: engine, ochre: preset; press the key again or tap blue to load";
        case Page::EngineBrowser:
        case Page::FxBrowser:
        case Page::LfoBrowser:
        case Page::SeqBrowser: return "blue to choose, tap blue to load";
        case Page::TapeBrowser: return "blue: style, ochre: tape; tap: rename, load, new, erase";
        case Page::TapePage: return "blue scrub, ochre loop, gray speed, orange level";
        case Page::MixerPage: return "levels; shift: pan; tap: mute";
        case Page::EqPage: return "low, mid, high, amount; tap: off";
        case Page::MasterFxPage: return "T3 again: the master effect off; shift + T3: effects";
        case Page::MasterOutPage: return "left, right, drive, release; tap: link";
        case Page::TempoPage: return "blue bpm, ochre sync, gray tape speed, orange metronome; press tempo to tap";
        case Page::InputPage: return "blue source, ochre radio / channels, gray threshold, orange gain";
        case Page::OutputPage: return "T4 record, T2 stop, T3 side";
        case Page::ComPage: return "T1 midi, T2 ctrl, T3 list, T4 disk";
        case Page::SequencerPage: return "play keys; shift + keys to enter notes";
        case Page::EraseConfirm: return "press T1, T2, T3 and T4 to erase";
        default: return "help: press a key to see what it does";
    }
}
} // namespace keytape
