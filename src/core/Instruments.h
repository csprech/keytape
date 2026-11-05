#pragma once

// The two instruments: the synth (six voices, its play mode and glide, its LFO and FX) and the drums (KIT or the
// drum sampler, the drum envelope, LFO and FX). Each takes notes and renders stereo.

#include "Dsp.h"

namespace keytape
{
/** What the LFOs can read from outside the sound. */
struct LfoInputs
{
    float inputLevel = 0.0f; // the input source's envelope
    float gforce = 0.0f;     // the tilt (-1..1)
    float sum = 0.0f;        // the main output's level
    std::array<float, 4> cc {}; // MIDI CC 1-4 (0..1)
    double bpm = 120.0;
};

/** One LFO's modulation of a sound this block. */
struct LfoOutput
{
    dsp::Mods mods;
    Module fxMod;     // added to the FX parameters
    float tapeSpeed = 0.0f;
    float shown = 0.0f; // the LFO's value, for the display (-1..1)
};

class LfoRunner
{
public:
    /** Advances `samples` and works out what the LFO does. `noteOn` = a note started this block; `envLevel` the
        loudest voice's envelope; `velocity` the last note's. */
    LfoOutput run (const LfoModule& lfo, int samples, double sr, bool drum, const LfoInputs& in, bool noteOn, float envLevel, float velocity);

private:
    double phase = 0.0;
    float randomValue = 0.0f, randomTarget = 0.0f, fade = 1.0f;
    juce::Random rng { 17 };
};

class SynthHost
{
public:
    void prepare (double sr, int maxBlock);
    void noteOn (int note, float velocity, const SynthSound& s);
    void noteOff (int note, const SynthSound& s);
    void allOff();
    void pitchBend (float semitones) { bend = semitones; }
    void sustain (bool on);
    /** Renders into L, R (cleared first). `input` is the input source (mono) for the vocoder and SENSE. */
    void process (int n, float* L, float* R, const SynthSound& s, const float* input, const LfoInputs& in);
    float lfoShown() const { return lastLfo; }
    int activeVoices() const;
    float envelopeLevel() const;
    float tapeSpeedMod() const { return tapeMod; }

private:
    double sr = 48000.0;
    std::array<dsp::SynthVoice, kVoices> voices;
    dsp::Effect fx;
    LfoRunner lfo;
    std::vector<int> heldNotes; // (mono and legato: the last held note plays)
    float bend = 0.0f, lastLfo = 0.0f, tapeMod = 0.0f, lastVelocity = 0.8f;
    float lastNote = -1.0f;
    bool sustained = false, noteStarted = false;
    std::vector<int> sustainedNotes;
    uint64_t counter = 0;
    std::array<dsp::Svf, 32> vocBands;
    std::array<float, 32> vocEnv {};
};

class DrumHost
{
public:
    void prepare (double sr, int maxBlock);
    void noteOn (int key, float velocity, const DrumKit& k);
    void noteOff (int key);
    void allOff();
    void process (int n, float* L, float* R, const DrumKit& k, const LfoInputs& in);
    float lfoShown() const { return lastLfo; }
    int lastKey = 0; // (the key the drum screens show)

private:
    double sr = 48000.0;
    std::array<dsp::DrumVoice, 12> voices;
    dsp::Effect fx;
    LfoRunner lfo;
    float lastLfo = 0.0f, lastVelocity = 0.8f;
    bool noteStarted = false;
};
} // namespace keytape
