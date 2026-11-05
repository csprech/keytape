#include "Model.h"

namespace keytape
{
// ======================================================================== names

const char* engineName (Engine e)
{
    static const char* n[] = { "swarm", "bits", "shape", "gene", "warp", "dual", "fm", "phase", "pulse", "sampler", "string", "volt", "vocoder" };
    return n[juce::jlimit (0, (int) Engine::Count - 1, (int) e)];
}
const char* drumEngineName (DrumEngine e) { return e == DrumEngine::Sampler ? "drum sampler" : "kit"; }
const char* fxName (Fx f)
{
    static const char* n[] = { "comb", "delay", "cube", "hall", "sweep", "lofi", "thump", "spring" };
    return n[juce::jlimit (0, (int) Fx::Count - 1, (int) f)];
}
const char* lfoName (Lfo l)
{
    static const char* n[] = { "random", "sense", "midi", "tremolo", "value", "velocity" };
    return n[juce::jlimit (0, (int) Lfo::Count - 1, (int) l)];
}
const char* seqName (Seq s)
{
    static const char* n[] = { "arp", "chain", "hands", "hold", "grid", "draw", "spin" };
    return n[juce::jlimit (0, (int) Seq::Count - 1, (int) s)];
}
const char* tapeStyleName (TapeStyle t)
{
    static const char* n[] = { "studio 4-track", "vintage 4-track", "cassette 4-track", "mo disc" };
    return n[juce::jlimit (0, (int) TapeStyle::Count - 1, (int) t)];
}
const char* inputName (Input i)
{
    static const char* n[] = { "mic / line in", "fm radio", "usb audio", "resample" };
    return n[(int) i];
}
const char* playModeName (PlayMode m)
{
    static const char* n[] = { "poly", "mono", "legato", "unison" };
    return n[(int) m];
}
const char* syncName (Sync s)
{
    static const char* n[] = { "free", "beat match", "midi sync", "sync out" };
    return n[(int) s];
}

juce::String engineParamName (Engine e, int i)
{
    // blue, ochre, gray, orange; then their shifted functions ("fine" where shift fine-tunes the same parameter)
    static const char* t[][kParams] = {
        { "wave number", "wave envelope", "spread", "unitor", "fine", "fine", "fine", "fine" },                // swarm
        { "wave shaper", "octave", "detune / ring", "digitalness", "fine", "fine", "fine", "fine" },            // bits
        { "waveform", "width", "cutoff", "resonance", "fine", "fine", "fine", "fine" },                         // shape
        { "filter", "wave number", "wave modifier", "noise", "fine", "fine", "fine", "fine" },                  // gene
        { "wave type", "filter", "phase", "chorus", "fine", "fine", "fine", "fine" },                           // warp
        { "frequency", "waveform", "envelope", "cross mod", "env crossfader", "waveform 2", "envelope 2", "cutoff" }, // dual
        { "fm amount", "frequency", "topology", "detune", "fine", "fine", "fine", "fine" },                     // fm
        { "phase shift", "distortion", "phase filter", "phase tilt", "fine", "fine", "fine", "fine" },          // phase
        { "filter", "amplitude", "second pulse", "modulation", "fine", "fine", "fine", "fine" },                // pulse
        { "start", "loop in", "loop out", "end", "direction", "fine tune", "loop fade", "gain" },               // sampler
        { "tension", "decay", "detune", "impulse", "fine", "fine", "fine", "fine" },                            // string
        { "modulation", "ground noise", "phase filter", "detune", "fine", "fine", "fine", "fine" },             // volt
        { "waveform", "formant", "bands", "mix", "fine", "fine", "fine", "fine" },                              // vocoder
    };
    return t[juce::jlimit (0, (int) Engine::Count - 1, (int) e)][juce::jlimit (0, kParams - 1, i)];
}

juce::String drumParamName (DrumEngine e, int i)
{
    static const char* sampler[] = { "tuning", "in point", "out point", "play mode", "direction", "pan", "attack", "gain" };
    static const char* kit[] = { "pitch", "waveform", "envelope", "cross mod", "pitch 2", "waveform 2", "envelope 2", "cutoff" };
    return (e == DrumEngine::Sampler ? sampler : kit)[juce::jlimit (0, kParams - 1, i)];
}

juce::String fxParamName (Fx f, int i)
{
    static const char* t[][4] = {
        { "frequency", "delay", "feedback", "sideband" }, { "range", "speed", "feedback", "level" }, { "x size", "y size", "z feedback", "mix" },
        { "distance", "gate", "color", "mix" },           { "frequency", "filter follow", "feedback", "frequency 2" },
        { "tone", "gsm", "baud", "telemetry" },           { "frequency", "punch", "rounds", "power" }, { "tone", "turns", "damping", "mix" },
    };
    return i < 4 ? juce::String (t[juce::jlimit (0, (int) Fx::Count - 1, (int) f)][i]) : juce::String ("fine");
}

juce::String lfoParamName (Lfo l, int i)
{
    static const char* t[][kParams] = {
        { "speed", "amount", "destination", "envelope", "fine", "fine", "parameter", "fine" },                                  // random
        { "source", "amount", "destination", "parameter", "fine", "fine", "fine", "fine" },                                     // sense
        { "parameter 1", "parameter 2", "parameter 3", "parameter 4", "destination 1", "destination 2", "destination 3", "destination 4" }, // midi
        { "speed", "pitch amount", "volume level", "envelope", "speed fine", "fine", "fine", "lfo shape" },                     // tremolo
        { "speed", "amount", "destination", "parameter", "speed fine", "fine", "fine", "fine" },                                // value
        { "dest amount", "volume amount", "destination", "parameter", "fine", "fine", "fine", "fine" },                         // velocity
    };
    return t[juce::jlimit (0, (int) Lfo::Count - 1, (int) l)][juce::jlimit (0, kParams - 1, i)];
}

juce::String envParamName (bool drum, int i)
{
    static const char* synth[] = { "attack", "decay", "sustain", "release", "play mode", "portamento", "bend range", "volume" };
    static const char* drums[] = { "attack", "gain", "release", "timing", "fine", "fine", "fine", "fine" };
    return (drum ? drums : synth)[juce::jlimit (0, kParams - 1, i)];
}

juce::String targetParamName (Target t, int i, Engine e, Fx f, bool drum)
{
    switch (t)
    {
        case Target::Engine: return drum ? drumParamName (DrumEngine::Kit, i) : engineParamName (e, i);
        case Target::Envelope: return envParamName (drum, i);
        case Target::Fx: return fxParamName (f, i);
        case Target::Main:
        {
            static const char* m[] = { "pitch", "volume", "pan", "tape speed" };
            return m[juce::jlimit (0, 3, i)];
        }
    }
    return {};
}

const char* noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static thread_local char buf[8];
    std::snprintf (buf, sizeof (buf), "%s%d", names[((n % 12) + 12) % 12], n / 12 - 1);
    return buf;
}

// ======================================================================== defaults

SynthSound::SynthSound()
{
    eng.p = { 0.3f, 0.5f, 0.6f, 0.3f, 0.5f, 0.5f, 0.5f, 0.5f };
    env.p = { 0.02f, 0.3f, 0.7f, 0.25f, 0.0f, 0.0f, 0.17f, 0.8f }; // play mode poly, no portamento, bend 2, volume
    fx.type = Fx::Delay;
    fx.m.p = { 0.4f, 0.5f, 0.4f, 0.3f, 0, 0, 0, 0 };
    lfo.type = Lfo::Tremolo;
    lfo.m.p = { 0.3f, 0.5f, 0.5f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f };
}

PlayMode SynthSound::playMode() const { return (PlayMode) juce::jlimit (0, 3, (int) (env.p[4] * 3.999f)); }

int SynthSound::bendRange() const { return juce::jlimit (0, 12, (int) std::lround (env.p[6] * 12.0f)); }

DrumKit::DrumKit()
{
    for (int k = 0; k < kKeys; ++k)
    {
        keys[(size_t) k].p = { 0.5f, (float) k / kKeys, (float) (k + 1) / kKeys, 0.0f, 0.0f, 0.5f, 0.0f, 0.75f }; // tuning, in, out, ...
    }
    env.p = { 0.5f, 0.75f, 0.5f, 0.5f, 0, 0, 0, 0 };
    fx.type = Fx::Hall;
    fx.m.p = { 0.3f, 0.2f, 0.5f, 0.2f, 0, 0, 0, 0 };
    lfo.type = Lfo::Value;
    lfo.m.p = { 0.3f, 0.0f, 0.0f, 0.0f, 0.5f, 0, 0, 0 };
}

MixerState::MixerState()
{
    eq.p = { 0.5f, 0.5f, 0.5f, 0.5f, 0, 0, 0, 0 };
    fx.type = Fx::Hall;
    fx.m.p = { 0.4f, 0.3f, 0.5f, 0.25f, 0, 0, 0, 0 };
    out.p = { 0.8f, 0.8f, 0.0f, 0.4f, 0, 0, 0, 0 };
}

// ======================================================================== factory presets (Keytape's own)

namespace
{
    SynthSound make (Engine e, std::initializer_list<float> eng, std::initializer_list<float> env, Fx fx, std::initializer_list<float> fxp, bool fxOn,
                     Lfo lfo, std::initializer_list<float> lfop, bool lfoOn, const char* name)
    {
        SynthSound s;
        s.name = name;
        s.engine = e;
        int i = 0;
        for (float v : eng)
            s.eng.p[(size_t) i++] = v;
        i = 0;
        for (float v : env)
            s.env.p[(size_t) i++] = v;
        s.fx.type = fx;
        i = 0;
        for (float v : fxp)
            s.fx.m.p[(size_t) i++] = v;
        s.fx.on = fxOn;
        s.lfo.type = lfo;
        i = 0;
        for (float v : lfop)
            s.lfo.m.p[(size_t) i++] = v;
        s.lfo.on = lfoOn;
        return s;
    }
} // namespace

const std::vector<Preset>& factorySynthPresets (Engine e)
{
    static std::array<std::vector<Preset>, (size_t) Engine::Count> lib = [] {
        std::array<std::vector<Preset>, (size_t) Engine::Count> l;
        auto add = [&l] (SynthSound s) { l[(size_t) s.engine].push_back ({ s.name, s }); };
        const std::initializer_list<float> pad { 0.45f, 0.4f, 0.75f, 0.6f, 0.0f, 0.0f, 0.17f, 0.8f };
        const std::initializer_list<float> pluck { 0.0f, 0.25f, 0.0f, 0.2f, 0.0f, 0.0f, 0.17f, 0.8f };
        const std::initializer_list<float> keys { 0.01f, 0.35f, 0.6f, 0.3f, 0.0f, 0.0f, 0.17f, 0.8f };
        const std::initializer_list<float> lead { 0.01f, 0.3f, 0.8f, 0.2f, 0.4f, 0.15f, 0.17f, 0.8f };
        add (make (Engine::Swarm, { 0.5f, 0.4f, 0.6f, 0.3f }, pad, Fx::Hall, { 0.5f, 0.2f, 0.5f, 0.35f }, true, Lfo::Tremolo, { 0.2f, 0.52f, 0.6f, 0.0f }, true, "cloud"));
        add (make (Engine::Swarm, { 0.2f, 0.7f, 0.3f, 0.8f }, keys, Fx::Delay, { 0.4f, 0.5f, 0.4f, 0.3f }, true, Lfo::Random, { 0.3f, 0.2f, 0.0f, 0.0f }, false, "bees"));
        add (make (Engine::Bits, { 0.4f, 0.5f, 0.3f, 0.5f }, keys, Fx::Lofi, { 0.5f, 0.3f, 0.4f, 0.2f }, true, Lfo::Value, { 0.4f, 0.2f, 0.0f, 0.0f }, false, "console"));
        add (make (Engine::Bits, { 0.7f, 0.25f, 0.6f, 0.8f }, lead, Fx::Delay, { 0.3f, 0.6f, 0.5f, 0.3f }, true, Lfo::Tremolo, { 0.5f, 0.55f, 0.5f, 0.0f }, true, "chip lead"));
        add (make (Engine::Shape, { 0.6f, 0.3f, 0.45f, 0.35f }, lead, Fx::Delay, { 0.4f, 0.45f, 0.3f, 0.25f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "analog lead"));
        add (make (Engine::Shape, { 0.75f, 0.8f, 0.25f, 0.55f }, { 0.0f, 0.2f, 0.4f, 0.15f, 0.25f, 0.0f, 0.17f, 0.85f }, Fx::Thump, { 0.4f, 0.4f, 0.3f, 0.3f }, true, Lfo::Value, { 0.5f, 0.3f, 0.0f, 0.75f }, false, "sub bass"));
        add (make (Engine::Shape, { 0.5f, 0.9f, 0.6f, 0.2f }, pad, Fx::Hall, { 0.6f, 0.1f, 0.6f, 0.4f }, true, Lfo::Tremolo, { 0.15f, 0.5f, 0.55f, 0.0f }, true, "wide pad"));
        add (make (Engine::Gene, { 0.5f, 0.4f, 0.5f, 0.1f }, keys, Fx::Spring, { 0.5f, 0.4f, 0.5f, 0.3f }, true, Lfo::Random, { 0.2f, 0.3f, 0.0f, 0.0f }, true, "organic"));
        add (make (Engine::Warp, { 0.3f, 0.6f, 0.4f, 0.5f }, pad, Fx::Cube, { 0.5f, 0.5f, 0.4f, 0.3f }, true, Lfo::Tremolo, { 0.25f, 0.5f, 0.6f, 0.0f }, false, "warped"));
        add (make (Engine::Dual, { 0.5f, 0.3f, 0.4f, 0.2f, 0.5f, 0.5f, 0.6f, 0.6f }, lead, Fx::Delay, { 0.4f, 0.5f, 0.3f, 0.25f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "duo"));
        add (make (Engine::Fm, { 0.35f, 0.5f, 0.3f, 0.2f }, keys, Fx::Hall, { 0.4f, 0.2f, 0.5f, 0.25f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.55f, 0.0f }, false, "e-piano"));
        add (make (Engine::Fm, { 0.7f, 0.75f, 0.6f, 0.1f }, pluck, Fx::Delay, { 0.3f, 0.55f, 0.4f, 0.3f }, true, Lfo::Random, { 0.2f, 0.2f, 0.0f, 0.0f }, false, "bell"));
        add (make (Engine::Phase, { 0.4f, 0.3f, 0.5f, 0.5f }, keys, Fx::Comb, { 0.4f, 0.4f, 0.3f, 0.5f }, false, Lfo::Value, { 0.3f, 0.3f, 0.0f, 0.0f }, true, "phaser"));
        add (make (Engine::Pulse, { 0.5f, 0.6f, 0.3f, 0.4f }, lead, Fx::Delay, { 0.4f, 0.5f, 0.35f, 0.25f }, true, Lfo::Tremolo, { 0.35f, 0.5f, 0.5f, 0.0f }, false, "square"));
        add (make (Engine::Sampler, { 0.0f, 0.3f, 0.7f, 1.0f, 0.0f, 0.5f, 0.2f, 0.75f }, keys, Fx::Hall, { 0.4f, 0.2f, 0.5f, 0.2f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "sampler"));
        add (make (Engine::String, { 0.5f, 0.6f, 0.3f, 0.4f }, pluck, Fx::Spring, { 0.5f, 0.3f, 0.5f, 0.25f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "guitar"));
        add (make (Engine::String, { 0.8f, 0.85f, 0.15f, 0.7f }, pluck, Fx::Hall, { 0.5f, 0.2f, 0.6f, 0.3f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "harp"));
        add (make (Engine::Volt, { 0.3f, 0.2f, 0.5f, 0.3f }, lead, Fx::Thump, { 0.5f, 0.5f, 0.3f, 0.3f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "volts"));
        add (make (Engine::Vocoder, { 0.4f, 0.5f, 0.6f, 0.8f }, pad, Fx::Hall, { 0.4f, 0.2f, 0.5f, 0.25f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "robot"));
        // ---- more of each (four or more an engine, as the browser shows them)
        const std::initializer_list<float> bass { 0.0f, 0.3f, 0.5f, 0.15f, 0.3f, 0.1f, 0.17f, 0.85f };
        const std::initializer_list<float> swell { 0.6f, 0.5f, 0.8f, 0.7f, 0.0f, 0.0f, 0.17f, 0.75f };
        const std::initializer_list<float> stab { 0.0f, 0.15f, 0.1f, 0.15f, 0.0f, 0.0f, 0.17f, 0.8f };
        const std::initializer_list<float> wide { 0.2f, 0.4f, 0.7f, 0.4f, 0.8f, 0.0f, 0.17f, 0.7f }; // (unison)
        add (make (Engine::Swarm, { 0.8f, 0.2f, 0.85f, 0.6f }, swell, Fx::Hall, { 0.7f, 0.1f, 0.6f, 0.45f }, true, Lfo::Tremolo, { 0.1f, 0.5f, 0.55f, 0.4f }, true, "choir"));
        add (make (Engine::Swarm, { 0.35f, 0.5f, 0.15f, 0.1f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, true, Lfo::Random, { 0.2f, 0.2f, 0.0f, 0.0f }, false, "hive bass"));
        add (make (Engine::Bits, { 0.2f, 0.75f, 0.1f, 0.3f }, bass, Fx::Lofi, { 0.3f, 0.2f, 0.6f, 0.1f }, true, Lfo::Value, { 0.5f, 0.3f, 0.0f, 0.0f }, false, "8-bit bass"));
        add (make (Engine::Bits, { 0.55f, 0.5f, 0.85f, 0.65f }, stab, Fx::Delay, { 0.5f, 0.4f, 0.5f, 0.35f }, true, Lfo::Random, { 0.6f, 0.4f, 0.0f, 0.0f }, true, "glitch"));
        add (make (Engine::Shape, { 0.85f, 0.45f, 0.7f, 0.15f }, wide, Fx::Cube, { 0.5f, 0.5f, 0.4f, 0.3f }, true, Lfo::Tremolo, { 0.2f, 0.5f, 0.5f, 0.0f }, false, "supersaw"));
        add (make (Engine::Gene, { 0.7f, 0.2f, 0.6f, 0.0f }, pad, Fx::Hall, { 0.6f, 0.1f, 0.55f, 0.35f }, true, Lfo::Tremolo, { 0.2f, 0.5f, 0.55f, 0.0f }, true, "seedling"));
        add (make (Engine::Gene, { 0.35f, 0.8f, 0.3f, 0.25f }, stab, Fx::Delay, { 0.4f, 0.5f, 0.4f, 0.3f }, true, Lfo::Random, { 0.3f, 0.2f, 0.0f, 0.0f }, false, "mutant"));
        add (make (Engine::Gene, { 0.25f, 0.5f, 0.8f, 0.6f }, swell, Fx::Sweep, { 0.4f, 0.5f, 0.4f, 0.4f }, true, Lfo::Value, { 0.2f, 0.4f, 0.0f, 0.0f }, true, "breath"));
        add (make (Engine::Warp, { 0.7f, 0.4f, 0.8f, 0.1f }, keys, Fx::Delay, { 0.4f, 0.45f, 0.4f, 0.3f }, true, Lfo::Random, { 0.3f, 0.2f, 0.0f, 0.0f }, false, "tines"));
        add (make (Engine::Warp, { 0.1f, 0.3f, 0.2f, 0.9f }, swell, Fx::Hall, { 0.7f, 0.1f, 0.6f, 0.4f }, true, Lfo::Tremolo, { 0.15f, 0.5f, 0.55f, 0.0f }, true, "ensemble"));
        add (make (Engine::Warp, { 0.45f, 0.75f, 0.55f, 0.3f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, false, Lfo::Value, { 0.4f, 0.3f, 0.0f, 0.0f }, false, "rubber"));
        add (make (Engine::Dual, { 0.25f, 0.0f, 0.3f, 0.6f, 0.2f, 0.4f, 0.4f, 0.5f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "growl"));
        add (make (Engine::Dual, { 0.75f, 0.6f, 0.7f, 0.1f, 0.6f, 0.2f, 0.8f, 0.7f }, pad, Fx::Hall, { 0.6f, 0.1f, 0.55f, 0.35f }, true, Lfo::Tremolo, { 0.2f, 0.5f, 0.55f, 0.0f }, true, "twins"));
        add (make (Engine::Dual, { 0.6f, 0.9f, 0.15f, 0.4f, 0.5f, 0.9f, 0.2f, 0.8f }, stab, Fx::Delay, { 0.4f, 0.5f, 0.4f, 0.3f }, true, Lfo::Random, { 0.3f, 0.2f, 0.0f, 0.0f }, false, "ping"));
        add (make (Engine::Fm, { 0.5f, 0.12f, 0.0f, 0.15f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "fm bass"));
        add (make (Engine::Fm, { 0.25f, 0.4f, 0.9f, 0.5f }, swell, Fx::Hall, { 0.6f, 0.1f, 0.6f, 0.35f }, true, Lfo::Tremolo, { 0.15f, 0.5f, 0.55f, 0.0f }, true, "glass pad"));
        add (make (Engine::Phase, { 0.7f, 0.6f, 0.2f, 0.4f }, lead, Fx::Delay, { 0.4f, 0.5f, 0.35f, 0.25f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "bent lead"));
        add (make (Engine::Phase, { 0.3f, 0.2f, 0.7f, 0.3f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "resobass"));
        add (make (Engine::Phase, { 0.55f, 0.4f, 0.45f, 0.65f }, pad, Fx::Hall, { 0.6f, 0.1f, 0.55f, 0.35f }, true, Lfo::Value, { 0.15f, 0.3f, 0.0f, 0.0f }, true, "cascade"));
        add (make (Engine::Pulse, { 0.3f, 0.4f, 0.0f, 0.6f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "pwm bass"));
        add (make (Engine::Pulse, { 0.7f, 0.3f, 0.7f, 0.8f }, swell, Fx::Hall, { 0.6f, 0.1f, 0.6f, 0.35f }, true, Lfo::Tremolo, { 0.2f, 0.5f, 0.55f, 0.0f }, true, "strings"));
        add (make (Engine::Pulse, { 0.85f, 0.7f, 0.95f, 0.2f }, stab, Fx::Delay, { 0.4f, 0.55f, 0.4f, 0.3f }, true, Lfo::Random, { 0.3f, 0.2f, 0.0f, 0.0f }, false, "hoover"));
        add (make (Engine::Sampler, { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.75f }, stab, Fx::Delay, { 0.4f, 0.5f, 0.4f, 0.3f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "one shot"));
        add (make (Engine::Sampler, { 0.0f, 0.2f, 0.8f, 1.0f, 0.5f, 0.5f, 0.4f, 0.75f }, pad, Fx::Hall, { 0.6f, 0.1f, 0.6f, 0.35f }, true, Lfo::Tremolo, { 0.2f, 0.5f, 0.55f, 0.0f }, true, "reversed"));
        add (make (Engine::Sampler, { 0.1f, 0.3f, 0.5f, 0.9f, 0.9f, 0.5f, 0.3f, 0.75f }, keys, Fx::Lofi, { 0.4f, 0.3f, 0.5f, 0.2f }, true, Lfo::Random, { 0.3f, 0.2f, 0.0f, 0.0f }, false, "ping pong"));
        add (make (Engine::String, { 0.3f, 0.3f, 0.6f, 0.2f }, pluck, Fx::Delay, { 0.4f, 0.5f, 0.4f, 0.3f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "koto"));
        add (make (Engine::String, { 0.15f, 0.2f, 0.1f, 0.1f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, false, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "upright"));
        add (make (Engine::Volt, { 0.6f, 0.05f, 0.3f, 0.6f }, bass, Fx::Thump, { 0.4f, 0.5f, 0.3f, 0.3f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "modular bass"));
        add (make (Engine::Volt, { 0.4f, 0.4f, 0.8f, 0.3f }, swell, Fx::Hall, { 0.6f, 0.1f, 0.6f, 0.35f }, true, Lfo::Tremolo, { 0.15f, 0.5f, 0.55f, 0.0f }, true, "hum"));
        add (make (Engine::Volt, { 0.8f, 0.15f, 0.6f, 0.9f }, lead, Fx::Delay, { 0.4f, 0.5f, 0.35f, 0.25f }, true, Lfo::Random, { 0.2f, 0.2f, 0.0f, 0.0f }, false, "drift lead"));
        add (make (Engine::Vocoder, { 0.8f, 0.3f, 1.0f, 1.0f }, keys, Fx::Delay, { 0.4f, 0.5f, 0.4f, 0.3f }, true, Lfo::Tremolo, { 0.3f, 0.5f, 0.5f, 0.0f }, false, "talker"));
        add (make (Engine::Vocoder, { 0.1f, 0.7f, 0.3f, 0.6f }, swell, Fx::Hall, { 0.6f, 0.1f, 0.6f, 0.35f }, true, Lfo::Tremolo, { 0.2f, 0.5f, 0.55f, 0.0f }, true, "whisper"));
        add (make (Engine::Vocoder, { 0.5f, 0.5f, 0.5f, 0.4f }, lead, Fx::Comb, { 0.4f, 0.4f, 0.3f, 0.5f }, true, Lfo::Value, { 0.3f, 0.3f, 0.0f, 0.0f }, false, "radio voice"));
        return l;
    }();
    return lib[(size_t) juce::jlimit (0, (int) Engine::Count - 1, (int) e)];
}

const std::vector<KitPreset>& factoryKits (DrumEngine e)
{
    static std::array<std::vector<KitPreset>, 2> lib = [] {
        std::array<std::vector<KitPreset>, 2> l;
        // KIT: each key a drum voice: pitch, wave, envelope, cross mod; pitch 2, wave 2, envelope 2, cutoff
        auto kit = [] (const char* name, float tone, float snap, Fx fx) {
            DrumKit k;
            k.name = name;
            k.engine = DrumEngine::Kit;
            for (int i = 0; i < kKeys; ++i)
            {
                // the keyboard laid out like a drum machine: kicks low, snares, claps, toms, hats and cymbals up
                const float f = (float) i / (float) (kKeys - 1);
                const int family = i < 4 ? 0 : i < 8 ? 1 : i < 11 ? 2 : i < 16 ? 3 : i < 21 ? 4 : 5;
                auto& p = k.keys[(size_t) i].p;
                switch (family)
                {
                    case 0: p = { 0.15f + 0.04f * (float) i, 0.0f, 0.45f + 0.1f * tone, 0.1f * snap, 0.08f, 0.0f, 0.15f, 0.3f }; break;           // kick
                    case 1: p = { 0.45f + 0.02f * (float) i, 0.0f, 0.3f, 0.2f, 0.6f, 0.9f, 0.3f + 0.1f * snap, 0.6f + 0.2f * tone }; break;      // snare
                    case 2: p = { 0.6f, 0.9f, 0.2f, 0.0f, 0.65f, 0.9f, 0.25f, 0.55f }; break;                                                     // clap
                    case 3: p = { 0.25f + 0.07f * (float) (i - 11), 0.1f, 0.4f, 0.05f, 0.2f, 0.0f, 0.1f, 0.4f }; break;                            // tom
                    case 4: p = { 0.9f, 0.95f, 0.08f + 0.06f * (float) (i - 16), 0.5f, 0.95f, 0.95f, 0.1f, 0.9f }; break;                          // hat
                    default: p = { 0.85f, 0.95f, 0.6f, 0.7f, 0.9f, 0.95f, 0.5f, 0.8f }; break;                                                     // cymbal
                }
                juce::ignoreUnused (f);
            }
            k.fx.type = fx;
            return k;
        };
        l[1].push_back ({ "classic", kit ("classic", 0.5f, 0.5f, Fx::Hall) });
        l[1].push_back ({ "punchy", kit ("punchy", 0.2f, 1.0f, Fx::Thump) });
        l[1].push_back ({ "soft", kit ("soft", 0.9f, 0.1f, Fx::Spring) });
        // the drum sampler's kits are made at start-up from KIT sounds rendered into one sample (see Machine)
        DrumKit s;
        s.name = "rendered";
        s.engine = DrumEngine::Sampler;
        l[0].push_back ({ "rendered", s });
        return l;
    }();
    return lib[(size_t) e];
}

// ======================================================================== JSON

namespace
{
    juce::var moduleToJson (const Module& m)
    {
        juce::Array<juce::var> a;
        for (auto v : m.p)
            a.add (v);
        return a;
    }
    void moduleFromJson (const juce::var& v, Module& m)
    {
        if (auto* a = v.getArray())
            for (int i = 0; i < std::min (kParams, a->size()); ++i)
                m.p[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) (double) (*a)[i]);
    }
    juce::var fxToJson (const FxModule& f)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", (int) f.type);
        o->setProperty ("p", moduleToJson (f.m));
        o->setProperty ("on", f.on);
        return juce::var (o);
    }
    void fxFromJson (const juce::var& v, FxModule& f)
    {
        f.type = (Fx) juce::jlimit (0, (int) Fx::Count - 1, (int) v.getProperty ("type", (int) f.type));
        moduleFromJson (v["p"], f.m);
        f.on = (bool) v.getProperty ("on", f.on);
    }
    juce::var lfoToJson (const LfoModule& f)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("type", (int) f.type);
        o->setProperty ("p", moduleToJson (f.m));
        o->setProperty ("on", f.on);
        return juce::var (o);
    }
    void lfoFromJson (const juce::var& v, LfoModule& f)
    {
        f.type = (Lfo) juce::jlimit (0, (int) Lfo::Count - 1, (int) v.getProperty ("type", (int) f.type));
        moduleFromJson (v["p"], f.m);
        f.on = (bool) v.getProperty ("on", f.on);
    }
} // namespace

juce::var soundToJson (const SynthSound& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "keytape-sound");
    o->setProperty ("name", s.name);
    o->setProperty ("engine", (int) s.engine);
    o->setProperty ("eng", moduleToJson (s.eng));
    o->setProperty ("env", moduleToJson (s.env));
    o->setProperty ("fx", fxToJson (s.fx));
    o->setProperty ("lfo", lfoToJson (s.lfo));
    return juce::var (o);
}

void soundFromJson (const juce::var& v, SynthSound& s)
{
    s.name = v.getProperty ("name", s.name).toString();
    s.engine = (Engine) juce::jlimit (0, (int) Engine::Count - 1, (int) v.getProperty ("engine", (int) s.engine));
    moduleFromJson (v["eng"], s.eng);
    moduleFromJson (v["env"], s.env);
    fxFromJson (v["fx"], s.fx);
    lfoFromJson (v["lfo"], s.lfo);
}

juce::var kitToJson (const DrumKit& k)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "keytape-kit");
    o->setProperty ("name", k.name);
    o->setProperty ("engine", (int) k.engine);
    juce::Array<juce::var> keys;
    for (auto& m : k.keys)
        keys.add (moduleToJson (m));
    o->setProperty ("keys", keys);
    o->setProperty ("env", moduleToJson (k.env));
    o->setProperty ("fx", fxToJson (k.fx));
    o->setProperty ("lfo", lfoToJson (k.lfo));
    o->setProperty ("stacked", k.stacked);
    return juce::var (o);
}

void kitFromJson (const juce::var& v, DrumKit& k)
{
    k.name = v.getProperty ("name", k.name).toString();
    k.engine = (DrumEngine) juce::jlimit (0, 1, (int) v.getProperty ("engine", (int) k.engine));
    if (auto* a = v["keys"].getArray())
        for (int i = 0; i < std::min (kKeys, a->size()); ++i)
            moduleFromJson ((*a)[i], k.keys[(size_t) i]);
    moduleFromJson (v["env"], k.env);
    fxFromJson (v["fx"], k.fx);
    lfoFromJson (v["lfo"], k.lfo);
    k.stacked = (bool) v.getProperty ("stacked", false);
}

juce::var mixerToJson (const MixerState& m)
{
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> lv, pn, mu, eqOn;
    for (int t = 0; t < kTracks; ++t)
    {
        lv.add (m.level[(size_t) t]);
        pn.add (m.pan[(size_t) t]);
        mu.add (m.mute[(size_t) t]);
    }
    for (auto b : m.eqOn)
        eqOn.add (b);
    o->setProperty ("level", lv);
    o->setProperty ("pan", pn);
    o->setProperty ("mute", mu);
    o->setProperty ("eq", moduleToJson (m.eq));
    o->setProperty ("eqOn", eqOn);
    o->setProperty ("fx", fxToJson (m.fx));
    o->setProperty ("out", moduleToJson (m.out));
    o->setProperty ("linked", m.linked);
    return juce::var (o);
}

void mixerFromJson (const juce::var& v, MixerState& m)
{
    for (int t = 0; t < kTracks; ++t)
    {
        if (auto* a = v["level"].getArray(); a != nullptr && t < a->size())
            m.level[(size_t) t] = juce::jlimit (0.0f, 1.0f, (float) (double) (*a)[t]);
        if (auto* a = v["pan"].getArray(); a != nullptr && t < a->size())
            m.pan[(size_t) t] = juce::jlimit (0.0f, 1.0f, (float) (double) (*a)[t]);
        if (auto* a = v["mute"].getArray(); a != nullptr && t < a->size())
            m.mute[(size_t) t] = (bool) (*a)[t];
    }
    moduleFromJson (v["eq"], m.eq);
    if (auto* a = v["eqOn"].getArray())
        for (int i = 0; i < std::min (4, a->size()); ++i)
            m.eqOn[(size_t) i] = (bool) (*a)[i];
    fxFromJson (v["fx"], m.fx);
    moduleFromJson (v["out"], m.out);
    m.linked = (bool) v.getProperty ("linked", true);
}

juce::var Settings::toJson() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "keytape-settings");
    o->setProperty ("midiChannel", midiChannel);
    o->setProperty ("clockIn", clockIn);
    o->setProperty ("clockOut", clockOut);
    o->setProperty ("notesIn", notesIn);
    o->setProperty ("notesOut", notesOut);
    o->setProperty ("otherIn", otherIn);
    o->setProperty ("ctrlChannel", ctrlChannel);
    o->setProperty ("ctrlRelative", ctrlRelative);
    o->setProperty ("brightness", brightness);
    o->setProperty ("tuningCents", tuningCents);
    o->setProperty ("delayedPowerOff", delayedPowerOff);
    o->setProperty ("usbCharging", usbCharging);
    return juce::var (o);
}

void Settings::fromJson (const juce::var& v)
{
    midiChannel = juce::jlimit (0, 16, (int) v.getProperty ("midiChannel", 0));
    clockIn = (bool) v.getProperty ("clockIn", true);
    clockOut = (bool) v.getProperty ("clockOut", true);
    notesIn = (bool) v.getProperty ("notesIn", true);
    notesOut = (bool) v.getProperty ("notesOut", true);
    otherIn = (bool) v.getProperty ("otherIn", true);
    ctrlChannel = juce::jlimit (1, 16, (int) v.getProperty ("ctrlChannel", 1));
    ctrlRelative = (bool) v.getProperty ("ctrlRelative", true);
    brightness = juce::jlimit (0.1f, 1.0f, (float) (double) v.getProperty ("brightness", 0.8));
    tuningCents = juce::jlimit (-100.0f, 100.0f, (float) (double) v.getProperty ("tuningCents", 0.0));
    delayedPowerOff = (bool) v.getProperty ("delayedPowerOff", false);
    usbCharging = (bool) v.getProperty ("usbCharging", true);
}
} // namespace keytape
