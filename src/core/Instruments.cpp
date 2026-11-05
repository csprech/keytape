#include "Instruments.h"

namespace keytape
{
namespace
{
    Target targetOf (float v) { return (Target) juce::jlimit (0, 3, (int) (v * 3.999f)); }
    int paramOf (float v) { return juce::jlimit (0, 3, (int) (v * 3.999f)); }

    /** Adds `amount` to one target parameter. */
    void apply (LfoOutput& o, Target t, int param, float amount)
    {
        switch (t)
        {
            case Target::Engine: o.mods.eng[(size_t) param] += amount; break;
            case Target::Envelope: o.mods.env[(size_t) param] += amount; break;
            case Target::Fx: o.fxMod.p[(size_t) param] += amount; break;
            case Target::Main:
                if (param == 0)
                    o.mods.pitchSemis += amount * 12.0f;
                else if (param == 1)
                    o.mods.volume *= juce::jlimit (0.0f, 2.0f, 1.0f + amount);
                else if (param == 2)
                    o.mods.pan += amount;
                else
                    o.tapeSpeed += amount;
                break;
        }
    }

    float shapeAt (int shape, double phase)
    {
        const float p = (float) (phase - std::floor (phase));
        switch (shape)
        {
            case 0: return std::sin (dsp::kTwoPi * p);               // sine
            case 1: return 1.0f - 2.0f * p;                          // saw (down)
            case 2: return 2.0f * std::pow (1.0f - p, 3.0f) - 1.0f;  // exp
            case 3: return p < 0.5f ? 1.0f : -1.0f;                  // square
            default: return p < 0.08f ? 1.0f : -0.2f;                // blip
        }
    }
} // namespace

LfoOutput LfoRunner::run (const LfoModule& lfo, int samples, double sr, bool drum, const LfoInputs& in, bool noteOn, float envLevel, float velocity)
{
    juce::ignoreUnused (drum);
    LfoOutput o;
    if (! lfo.on)
        return o;
    const auto& p = lfo.m.p;
    const double dt = (double) samples / sr;
    auto rate = [&] (float coarse, float fine) { return 0.05 * std::pow (400.0, (double) coarse) * (1.0 + (fine - 0.5) * 0.2); };
    switch (lfo.type)
    {
        case Lfo::Random:
        {
            // new random values at speed; the envelope smooths the steps into glides
            phase += rate (p[0], 0.5f) * dt;
            if (phase >= 1.0)
            {
                phase -= std::floor (phase);
                randomTarget = rng.nextFloat() * 2.0f - 1.0f;
            }
            const float glide = p[3] < 0.02f ? 1.0f : (float) (dt / (0.01 + p[3] * 0.5));
            randomValue += (randomTarget - randomValue) * juce::jlimit (0.0f, 1.0f, glide);
            o.shown = randomValue;
            apply (o, targetOf (p[2]), paramOf (p[6]), randomValue * p[1] * 0.5f);
            break;
        }
        case Lfo::Sense:
        {
            // an outside source: the input, g-force, the envelope, or the sum of the output
            const int source = juce::jlimit (0, 3, (int) (p[0] * 3.999f));
            const float v = source == 0 ? in.inputLevel * 2.0f : source == 1 ? in.gforce : source == 2 ? envLevel : in.sum * 2.0f;
            o.shown = juce::jlimit (-1.0f, 1.0f, v);
            apply (o, targetOf (p[2]), paramOf (p[3]), o.shown * (p[1] - 0.5f) * 2.0f);
            break;
        }
        case Lfo::Midi:
            // CC 1-4 each to a destination (shift) and parameter
            for (int c = 0; c < 4; ++c)
                apply (o, targetOf (p[(size_t) c + 4]), paramOf (p[(size_t) c]), in.cc[(size_t) c] - 0.5f);
            o.shown = in.cc[0] * 2.0f - 1.0f;
            break;
        case Lfo::Tremolo:
        {
            // pitch and volume wobble, fading in over the envelope after each note
            phase += rate (p[0], p[4]) * dt;
            if (noteOn)
                fade = 0.0f;
            fade = std::min (1.0f, fade + (float) (dt / (0.001 + p[3] * 2.0)));
            const float v = shapeAt (juce::jlimit (0, 4, (int) (p[7] * 4.999f)), phase) * fade;
            o.shown = v;
            o.mods.pitchSemis += v * (p[1] - 0.5f) * 2.0f;          // up to a semitone, either way
            o.mods.volume *= 1.0f + v * (p[2] - 0.5f) * 1.6f;
            break;
        }
        case Lfo::Value:
        {
            // one parameter, swept by a shape (tap blue picks it); synced to notes or running free
            if (noteOn && p[5] > 0.5f)
                phase = 0.0;
            phase += rate (p[0], p[4]) * dt * (in.bpm / 120.0);
            static const int shapes[4] = { 3, 1, 1, 0 }; // square, ramp (up), saw (down), sine
            const int shape = shapes[juce::jlimit (0, 3, (int) (p[6] * 3.999f))];
            float v = shapeAt (shape, phase);
            if (juce::jlimit (0, 3, (int) (p[6] * 3.999f)) == 1)
                v = -v; // ramp up
            o.shown = v;
            apply (o, targetOf (p[2]), paramOf (p[3]), v * (p[1] - 0.5f));
            break;
        }
        case Lfo::Velocity:
            // how hard the key was played (over MIDI) moves a parameter and the volume
            o.shown = velocity * 2.0f - 1.0f;
            apply (o, targetOf (p[2]), paramOf (p[3]), (velocity - 0.5f) * (p[0] - 0.5f) * 2.0f);
            o.mods.volume *= 1.0f - p[1] + p[1] * velocity * 1.4f;
            break;
        case Lfo::Count: break;
    }
    return o;
}

// ======================================================================== the synth

void SynthHost::prepare (double s, int)
{
    sr = s;
    for (auto& v : voices)
        v.prepare (sr);
    fx.prepare (sr);
}

void SynthHost::noteOn (int note, float velocity, const SynthSound& s)
{
    lastVelocity = velocity;
    noteStarted = true;
    const auto mode = s.playMode();
    const float glide = std::pow (s.env.p[5], 2.0f) * 1.5f;
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    heldNotes.push_back (note);
    if (mode == PlayMode::Mono || mode == PlayMode::Legato)
    {
        // one voice: legato glides without restarting the envelope while a key is still held
        auto& v = voices[0];
        const bool legato = mode == PlayMode::Legato && heldNotes.size() > 1 && v.active();
        v.start (note, velocity, s, ! legato, lastNote, glide > 0.001f ? glide : (legato ? 0.03f : 0.0f));
        for (size_t i = 1; i < voices.size(); ++i)
            voices[i].release();
    }
    else if (mode == PlayMode::Unison)
    {
        for (auto& v : voices)
            v.start (note, velocity, s, true, lastNote, glide);
    }
    else
    {
        // poly: a free voice, or the oldest
        dsp::SynthVoice* best = nullptr;
        for (auto& v : voices)
            if (! v.active())
            {
                best = &v;
                break;
            }
        if (best == nullptr)
            for (auto& v : voices)
                if (! v.held() && (best == nullptr || v.age < best->age))
                    best = &v;
        if (best == nullptr)
            for (auto& v : voices)
                if (best == nullptr || v.age < best->age)
                    best = &v;
        best->start (note, velocity, s, true, lastNote, glide);
        best->age = ++counter;
    }
    lastNote = (float) note;
}

void SynthHost::noteOff (int note, const SynthSound& s)
{
    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    if (sustained)
    {
        sustainedNotes.push_back (note);
        return;
    }
    const auto mode = s.playMode();
    if ((mode == PlayMode::Mono || mode == PlayMode::Legato) && ! heldNotes.empty())
    {
        // back to the last key still held
        voices[0].start (heldNotes.back(), lastVelocity, s, mode == PlayMode::Mono, lastNote, mode == PlayMode::Legato ? 0.03f : 0.0f);
        lastNote = (float) heldNotes.back();
        return;
    }
    for (auto& v : voices)
        if (v.note == note && v.held())
            v.release();
}

void SynthHost::sustain (bool on)
{
    sustained = on;
    if (! on)
    {
        for (int n : sustainedNotes)
            for (auto& v : voices)
                if (v.note == n && std::find (heldNotes.begin(), heldNotes.end(), n) == heldNotes.end())
                    v.release();
        sustainedNotes.clear();
    }
}

void SynthHost::allOff()
{
    heldNotes.clear();
    sustainedNotes.clear();
    for (auto& v : voices)
        v.release();
}

int SynthHost::activeVoices() const
{
    int n = 0;
    for (auto& v : voices)
        n += v.active() ? 1 : 0;
    return n;
}

float SynthHost::envelopeLevel() const
{
    float e = 0.0f;
    for (auto& v : voices)
        e = std::max (e, v.active() ? 1.0f : 0.0f);
    return e;
}

void SynthHost::process (int n, float* L, float* R, const SynthSound& s, const float* input, const LfoInputs& in)
{
    std::fill (L, L + n, 0.0f);
    std::fill (R, R + n, 0.0f);
    auto out = lfo.run (s.lfo, n, sr, false, in, noteStarted, envelopeLevel(), lastVelocity);
    noteStarted = false;
    lastLfo = out.shown;
    tapeMod = out.tapeSpeed;
    out.mods.pitchSemis += bend;
    // the vocoder's modulator: the input split into bands, each followed
    const float* vocoder = nullptr;
    if (s.engine == Engine::Vocoder && input != nullptr)
    {
        const int bands = 8 + (int) (s.eng.p[2] * 24.0f);
        for (int b = 0; b < bands; ++b)
        {
            vocBands[(size_t) b].set ((float) sr, 120.0f * std::pow (60.0f, (float) b / (float) bands), 6.0f);
            float peak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                vocBands[(size_t) b].process (input[i]);
                peak = std::max (peak, std::abs (vocBands[(size_t) b].bp));
            }
            vocEnv[(size_t) b] += (peak - vocEnv[(size_t) b]) * 0.3f;
        }
        vocoder = vocEnv.data();
    }
    const auto mode = s.playMode();
    for (size_t i = 0; i < voices.size(); ++i)
    {
        const float spread = mode == PlayMode::Unison ? ((float) i / (float) (voices.size() - 1) - 0.5f) : 0.0f;
        auto mods = out.mods;
        if (mode == PlayMode::Unison)
            mods.pitchSemis += spread * 0.25f; // the voices fanned out a little
        voices[i].render (L, R, n, s, mods, vocoder, spread * 0.8f);
    }
    // the sound's volume, then its FX
    const float vol = s.env.p[7] * 1.2f;
    for (int i = 0; i < n; ++i)
    {
        L[i] *= vol;
        R[i] *= vol;
    }
    if (s.fx.on)
    {
        Module m = s.fx.m;
        for (int k = 0; k < 4; ++k)
            m.p[(size_t) k] = juce::jlimit (0.0f, 1.0f, m.p[(size_t) k] + out.fxMod.p[(size_t) k]);
        fx.process (s.fx.type, m, L, R, n, in.bpm);
    }
}

// ======================================================================== the drums

void DrumHost::prepare (double s, int)
{
    sr = s;
    for (auto& v : voices)
        v.prepare (sr);
    fx.prepare (sr);
}

void DrumHost::noteOn (int key, float velocity, const DrumKit& k)
{
    key = juce::jlimit (0, kKeys - 1, key);
    lastKey = key;
    lastVelocity = velocity;
    noteStarted = true;
    // the same key again cuts itself; otherwise a free voice or the oldest
    dsp::DrumVoice* v = nullptr;
    for (auto& d : voices)
        if (d.active() && d.key == key)
            v = &d;
    if (v == nullptr)
        for (auto& d : voices)
            if (! d.active())
            {
                v = &d;
                break;
            }
    if (v == nullptr)
        v = &voices[0];
    v->start (key, k, velocity);
}

void DrumHost::noteOff (int key)
{
    for (auto& d : voices)
        if (d.key == key)
            d.release();
}

void DrumHost::allOff()
{
    for (auto& d : voices)
        d.release();
}

void DrumHost::process (int n, float* L, float* R, const DrumKit& k, const LfoInputs& in)
{
    std::fill (L, L + n, 0.0f);
    std::fill (R, R + n, 0.0f);
    auto out = lfo.run (k.lfo, n, sr, true, in, noteStarted, 1.0f, lastVelocity);
    noteStarted = false;
    lastLfo = out.shown;
    for (auto& v : voices)
        v.render (L, R, n, k, out.mods);
    if (k.fx.on)
    {
        Module m = k.fx.m;
        for (int i = 0; i < 4; ++i)
            m.p[(size_t) i] = juce::jlimit (0.0f, 1.0f, m.p[(size_t) i] + out.fxMod.p[(size_t) i]);
        fx.process (k.fx.type, m, L, R, n, in.bpm);
    }
}
} // namespace keytape
