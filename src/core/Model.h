#pragma once

// Keytape's state: synth sounds and drum kits (four modules each: engine, envelope, FX, LFO), the mixer, tempo,
// input, output and the settings. Every module has eight parameters (four on the encoders, four more with shift),
// stored 0..1 as the encoders turn them. The tape is in Tape.h.

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <memory>
#include <vector>

namespace keytape
{
constexpr int kSlots = 8;     // sound keys 1-8
constexpr int kKeys = 24;     // the keyboard: F to E, two octaves
constexpr int kTracks = 4;    // tape tracks
constexpr int kTapes = 8;     // tapes in memory
constexpr int kVoices = 6;    // synth polyphony
constexpr int kParams = 8;    // per module: 4, then 4 with shift
constexpr int kLowestNote = 53; // the leftmost key (F3) at octave 0

enum class Mode : uint8_t { Synth = 0, Drum, Tape, Mixer };
enum class Engine : uint8_t { Swarm = 0, Bits, Shape, Gene, Warp, Dual, Fm, Phase, Pulse, Sampler, String, Volt, Vocoder, Count };
enum class DrumEngine : uint8_t { Sampler = 0, Kit, Count };
enum class Fx : uint8_t { Comb = 0, Delay, Cube, Hall, Sweep, Lofi, Thump, Spring, Count };
enum class Lfo : uint8_t { Random = 0, Sense, Midi, Tremolo, Value, Velocity, Count };
enum class Seq : uint8_t { Arp = 0, Chain, Hands, Hold, Grid, Draw, Spin, Count };
enum class TapeStyle : uint8_t { Studio = 0, Vintage, Cassette, MoDisc, Count };
enum class PlayMode : uint8_t { Poly = 0, Mono, Legato, Unison };
enum class Sync : uint8_t { Free = 0, BeatMatch, MidiSync, PoSync };
enum class Input : uint8_t { MicLine = 0, Radio, Usb, Resample };
/** What an LFO modulates: a module and one of its four parameters. */
enum class Target : uint8_t { Engine = 0, Envelope, Fx, Main };

const char* engineName (Engine);
const char* drumEngineName (DrumEngine);
const char* fxName (Fx);
const char* lfoName (Lfo);
const char* seqName (Seq);
const char* tapeStyleName (TapeStyle);
const char* inputName (Input);
const char* playModeName (PlayMode);
const char* syncName (Sync);
/** The label of parameter i (0-7) of a module, as its screen shows it. */
juce::String engineParamName (Engine, int i);
juce::String drumParamName (DrumEngine, int i);
juce::String fxParamName (Fx, int i);
juce::String lfoParamName (Lfo, int i);
juce::String envParamName (bool drum, int i);
juce::String targetParamName (Target, int i, Engine e, Fx f, bool drum);
const char* noteName (int midiNote); // "C4"

/** A recorded sound: stereo float audio (the sampler's sample, a drum kit's sample, a lifted take). */
struct Audio
{
    juce::AudioBuffer<float> data; // 2 channels
    double sampleRate = 48000.0;
    int length() const { return data.getNumSamples(); }
};
using AudioPtr = std::shared_ptr<const Audio>;

struct Module
{
    std::array<float, kParams> p {};
    bool operator== (const Module&) const = default;
};

struct FxModule
{
    Fx type = Fx::Delay;
    Module m;
    bool on = false;
    bool operator== (const FxModule&) const = default;
};

struct LfoModule
{
    Lfo type = Lfo::Tremolo;
    Module m;
    bool on = false;
    bool operator== (const LfoModule&) const = default;
};

/** A synth sound: engine (T1), envelope with its play mode (T2), FX (T3), LFO (T4). */
struct SynthSound
{
    juce::String name { "init" };
    Engine engine = Engine::Shape;
    Module eng;
    Module env; // attack, decay, sustain, release; play mode, portamento, bend range, volume
    FxModule fx;
    LfoModule lfo;
    AudioPtr sample; // (the sampler's)
    SynthSound();
    PlayMode playMode() const;
    int bendRange() const; // semitones
};

/** A drum kit: the drum engine (T1), the drum envelope (T2), FX (T3), LFO (T4). */
struct DrumKit
{
    juce::String name { "init" };
    DrumEngine engine = DrumEngine::Kit;
    /** Per key, for the sampler: in, out, tune, play mode, direction, pan, attack, gain (0..1 each). For KIT: pitch,
        wave, envelope, cross mod; pitch 2, wave 2, envelope 2, filter. */
    std::array<Module, kKeys> keys;
    Module env; // attack, gain, release, timing
    FxModule fx;
    LfoModule lfo;
    AudioPtr sample;
    bool stacked = false; // the sampler's channels: stereo L+R, or stacked A+B
    DrumKit();
};

struct MixerState
{
    std::array<float, kTracks> level { 0.8f, 0.8f, 0.8f, 0.8f }, pan { 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<bool, kTracks> mute {};
    Module eq;                 // low, mid, high, amount (0.5 = flat for the bands)
    std::array<bool, 4> eqOn { true, true, true, true };
    FxModule fx;               // the master effect
    Module out;                // left, right, drive, release
    bool linked = true;        // left and right move together
    MixerState();
};

struct TempoState
{
    double bpm = 120.0;
    Sync sync = Sync::BeatMatch;
    bool sixteenth = false; // PO sync: 1/16 pulses
    float metronome = 0.0f; // level (0 = off)
    int metronomeSound = 0;
};

struct InputState
{
    Input source = Input::MicLine;
    bool stereo = true;
    double fmMHz = 98.0;
    float threshold = 0.0f, gain = 0.5f;
    bool on = false; // input monitoring in tape/mixer (the input key toggles it)
};

struct Settings
{
    int midiChannel = 0;             // 0 = all (omni) for input; output on channel 1
    bool clockIn = true, clockOut = true, notesIn = true, notesOut = true, otherIn = true;
    int ctrlChannel = 1;
    bool ctrlRelative = true;
    float brightness = 0.8f;
    float tuningCents = 0.0f;        // global detune
    bool delayedPowerOff = false;
    bool usbCharging = true;
    juce::var toJson() const;
    void fromJson (const juce::var&);
};

/** The 8 synth sounds, the 8 drum kits and which is selected. */
struct Sounds
{
    std::array<SynthSound, kSlots> synth;
    std::array<DrumKit, kSlots> drum;
    int synthSlot = 0, drumSlot = 0;
};

// ---- presets: the factory library the browsers show (original sounds)
struct Preset
{
    juce::String name;
    SynthSound sound;
};
const std::vector<Preset>& factorySynthPresets (Engine);
struct KitPreset
{
    juce::String name;
    DrumKit kit;
};
const std::vector<KitPreset>& factoryKits (DrumEngine);

juce::var soundToJson (const SynthSound&);
void soundFromJson (const juce::var&, SynthSound&);
juce::var kitToJson (const DrumKit&);
void kitFromJson (const juce::var&, DrumKit&);
juce::var mixerToJson (const MixerState&);
void mixerFromJson (const juce::var&, MixerState&);
} // namespace keytape
