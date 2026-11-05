#pragma once

// Signal processing: building blocks, the synth voice (all 13 engines), the drum voices (KIT and the drum sampler),
// the 8 effects, the master EQ and drive, and the tape styles' coloring. All Keytape's own designs.

#include "Model.h"
#include <cmath>

namespace keytape::dsp
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

struct Noise
{
    uint32_t s = 0x12345678u;
    float next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

/** A state-variable filter (Simper's): low, band and high outputs. */
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set (float sr, float hz, float q)
    {
        hz = juce::jlimit (10.0f, sr * 0.45f, hz);
        g = std::tan (kPi * hz / sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    float process (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
        return lp;
    }
    void reset() { ic1 = ic2 = 0; }
};

/** A one-pole lowpass (smoothing, tone). */
struct OnePole
{
    float y = 0, a = 0.5f;
    void set (float sr, float hz) { a = 1.0f - std::exp (-kTwoPi * juce::jlimit (1.0f, sr * 0.49f, hz) / sr); }
    float process (float x) { return y += a * (x - y); }
};

/** A delay line with fractional (linear) reads. */
struct DelayLine
{
    std::vector<float> buf;
    int pos = 0;
    void prepare (int size)
    {
        buf.assign ((size_t) std::max (4, size), 0.0f);
        pos = 0;
    }
    void write (float x)
    {
        buf[(size_t) pos] = x;
        if (++pos >= (int) buf.size())
            pos = 0;
    }
    float read (float delay) const
    {
        const int size = (int) buf.size();
        delay = juce::jlimit (1.0f, (float) size - 2.0f, delay);
        float rp = (float) pos - delay;
        while (rp < 0)
            rp += (float) size;
        const int i0 = (int) rp, i1 = (i0 + 1) % size;
        const float f = rp - (float) i0;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
};

struct Adsr
{
    enum class Stage { Idle, Attack, Decay, Sustain, Release } stage = Stage::Idle;
    float level = 0, a = 0.01f, d = 0.2f, s = 0.7f, r = 0.2f; // seconds, level
    void noteOn() { stage = Stage::Attack; }
    void noteOff()
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }
    bool active() const { return stage != Stage::Idle; }
    float next (float sr);
};

/** What the LFO and the play mode do to a voice this block. */
struct Mods
{
    std::array<float, kParams> eng {};  // added to the engine's parameters
    std::array<float, 4> env {};        // added to attack, decay, sustain, release
    float pitchSemis = 0.0f;            // bend + LFO + glide offsets
    float volume = 1.0f;                // LFO tremolo, velocity
    float pan = 0.0f;                   // -1..1 offset
};

/** One synth voice: any of the 13 engines, with its amplitude envelope. */
class SynthVoice
{
public:
    void prepare (double sampleRate);
    void start (int note, float velocity, const SynthSound& s, bool retrigger, float glideFromNote, float glideSeconds);
    void release();
    void kill();
    bool active() const { return env.active(); }
    bool held() const { return gate; }
    int note = -1;
    float velocity = 1.0f;
    uint64_t age = 0;
    /** Adds the voice into L, R. `vocoderIn` is the modulator for the vocoder (mono, may be null). */
    void render (float* L, float* R, int n, const SynthSound& s, const Mods& mods, const float* vocoderIn, float unisonSpread);

private:
    float renderEngine (const SynthSound& s, const std::array<float, kParams>& p, float freq, float& right);
    double sr = 48000.0;
    Adsr env, env2;
    bool gate = false;
    float glideNote = 0, glideRate = 0, currentNote = 60;
    // oscillator state (shared by engines)
    std::array<float, 8> ph {};
    float subPh = 0, lastOut = 0, held1 = 0, holdCount = 0;
    Svf f1, f2, f3, f4;
    Noise noise;
    DelayLine str1, str2;  // string
    float strLp1 = 0, strLp2 = 0;
    double samplePhase = 0.0;
    bool sampleForward = true, sampleReleased = false, sampleStarted = false;
    std::array<Svf, 32> vocCarrier, vocMod;
    float drift = 0, driftTarget = 0;
    int driftCount = 0;
    OnePole hum;
};

/** One drum voice: KIT (a dual-oscillator drum synth) or a slice of the drum sampler. */
class DrumVoice
{
public:
    void prepare (double sampleRate);
    void start (int key, const DrumKit& k, float velocity);
    void release();
    bool active() const { return on; }
    int key = -1;
    void render (float* L, float* R, int n, const DrumKit& k, const Mods& mods);

private:
    double sr = 48000.0;
    bool on = false, gate = false;
    float t = 0, ph1 = 0, ph2 = 0, amp1 = 0, amp2 = 0, velocity = 1;
    double pos = 0;
    float attackEnv = 0;
    Svf filt;
    Noise noise;
};

/** The 8 effects: stereo in, stereo out (in place). */
class Effect
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (Fx type, const Module& m, float* L, float* R, int n, double bpm);

private:
    double sr = 48000.0;
    Fx last = Fx::Count;
    DelayLine dl[8];
    Svf sv[4];
    OnePole op[4];
    float fb[4] {}, env = 0, phase = 0, hold[2] {}, holdCount = 0, crushPh = 0;
    Noise noise;
    // a frequency shifter (for comb's sideband): two allpass chains in quadrature
    struct Hilbert
    {
        std::array<float, 4> c {}, x1 {}, y1 {};
        float process (float x)
        {
            for (int i = 0; i < 4; ++i)
            {
                const float y = c[(size_t) i] * (x - y1[(size_t) i]) + x1[(size_t) i];
                x1[(size_t) i] = x;
                y1[(size_t) i] = y;
                x = y;
            }
            return x;
        }
    } hilA, hilB;
    float hilDelay = 0, shiftPh = 0;
};

/** The master EQ: low shelf, mid peak, high shelf, and the amount. */
class MasterEq
{
public:
    void prepare (double sr);
    void process (const Module& m, const std::array<bool, 4>& on, float* L, float* R, int n);

private:
    double sr = 48000.0;
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    std::array<Biquad, 6> bq;
    std::array<float, 3> lastDb { -99, -99, -99 };
    void design (int band, float db);
};

/** The master out's drive: a compressor that gets louder and dirtier, with its release. */
class Drive
{
public:
    void prepare (double sr);
    void process (float drive, float release, float* L, float* R, int n);
    float gainReduction() const { return lastGr; }

private:
    double sr = 48000.0;
    float env = 0, lastGr = 0;
};

/** A tape style's coloring, on playback: saturation, wow and flutter, hiss, bandwidth, data compression. */
class TapeColour
{
public:
    void prepare (double sr);
    void process (TapeStyle s, float* L, float* R, int n, float speed);

private:
    double sr = 48000.0;
    DelayLine wowL, wowR;
    float wowPh = 0, flutPh = 0;
    Svf toneL, toneR;
    Noise noise;
    std::array<float, 64> codecL {}, codecR {};
    int codecPos = 0;
};
} // namespace keytape::dsp
