// The synth voice (13 engines) and the drum voice (KIT, the drum sampler).

#include "Dsp.h"

namespace keytape::dsp
{
namespace
{
    float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
    float noteHz (float note) { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
    float expRange (float v, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (v)); }
    float softClip (float x) { return x / (1.0f + std::abs (x)); }
    // the 16 "genes" of GENE: harmonic recipes (relative amplitudes of harmonics 1-8)
    const float kGenes[16][8] = {
        { 1, 0, 0, 0, 0, 0, 0, 0 },          { 1, 0.5f, 0.33f, 0.25f, 0.2f, 0.16f, 0.14f, 0.12f }, { 1, 0, 0.33f, 0, 0.2f, 0, 0.14f, 0 },
        { 1, 0.7f, 0, 0.4f, 0, 0.2f, 0, 0.1f }, { 0.4f, 1, 0.3f, 0.6f, 0.1f, 0.3f, 0, 0.2f },     { 1, 0, 0, 0.8f, 0, 0, 0.6f, 0 },
        { 0.2f, 0.4f, 1, 0.4f, 0.2f, 0.1f, 0.05f, 0 }, { 1, 0.2f, 0.6f, 0.1f, 0.5f, 0.05f, 0.4f, 0.02f }, { 0.6f, 0, 0, 0, 1, 0, 0, 0.5f },
        { 1, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f }, { 1, 0, 0, 0, 0, 0, 0, 0.9f },           { 0.3f, 1, 0, 0, 0.7f, 0, 0, 0.4f },
        { 1, 0.3f, 0.1f, 0.6f, 0.1f, 0.05f, 0.3f, 0.02f }, { 0.5f, 0.5f, 0.5f, 0.5f, 0, 0, 0, 0 }, { 1, 0, 0.6f, 0, 0, 0.4f, 0, 0.3f },
        { 0.1f, 0.2f, 0.4f, 0.8f, 1, 0.8f, 0.4f, 0.2f },
    };
} // namespace

float Adsr::next (float sr)
{
    switch (stage)
    {
        case Stage::Idle: return 0.0f;
        case Stage::Attack:
            level += 1.0f / std::max (1.0f, a * sr);
            if (level >= 1.0f)
            {
                level = 1.0f;
                stage = Stage::Decay;
            }
            break;
        case Stage::Decay:
            level += (s - level) * (1.0f - std::exp (-4.6f / std::max (1.0f, d * sr)));
            if (std::abs (level - s) < 1.0e-4f)
                stage = Stage::Sustain;
            break;
        case Stage::Sustain: level = s; break;
        case Stage::Release:
            level *= std::exp (-6.9f / std::max (1.0f, r * sr));
            if (level < 1.0e-4f)
            {
                level = 0.0f;
                stage = Stage::Idle;
            }
            break;
    }
    return level;
}

// ======================================================================== the synth voice

void SynthVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    str1.prepare ((int) (sr / 20.0) + 8);
    str2.prepare ((int) (sr / 20.0) + 8);
    hum.set ((float) sr, 400.0f);
}

void SynthVoice::start (int n, float vel, const SynthSound& s, bool retrigger, float glideFromNote, float glideSeconds)
{
    note = n;
    velocity = vel;
    gate = true;
    if (glideSeconds > 0.0f && glideFromNote >= 0.0f)
    {
        currentNote = glideFromNote;
        glideRate = std::abs ((float) n - glideFromNote) / std::max (0.001f, glideSeconds * (float) sr);
    }
    else
    {
        currentNote = (float) n;
        glideRate = 0.0f;
    }
    glideNote = (float) n;
    if (retrigger || ! env.active())
    {
        // a fresh note: phases spread so a cluster doesn't start in step; the string is plucked
        for (size_t i = 0; i < ph.size(); ++i)
            ph[i] = (float) i * 0.137f;
        env.level = 0.0f;
        env.noteOn();
        env2.level = 0.0f;
        env2.noteOn();
        holdCount = 0;
        samplePhase = 0.0;
        sampleForward = true;
        sampleReleased = false;
        sampleStarted = false;
        if (s.engine == Engine::String)
        {
            // the impulse: a burst of noise, brighter as impulse goes up, into both strings
            const float freq = noteHz ((float) n);
            const int len = juce::jlimit (8, (int) str1.buf.size() - 4, (int) (sr / freq));
            OnePole lp;
            lp.set ((float) sr, expRange (s.eng.p[3], 300.0f, 16000.0f));
            // (the burst is the most recent stretch of each delay line: one period, read straight back)
            for (int i = 0; i < len; ++i)
            {
                const float x = lp.process (noise.next());
                str1.write (x);
                str2.write (x);
            }
        }
    }
    else
        env.noteOn();
}

void SynthVoice::release()
{
    gate = false;
    env.noteOff();
    env2.noteOff();
    sampleReleased = true;
}

void SynthVoice::kill()
{
    gate = false;
    env.stage = Adsr::Stage::Idle;
    env.level = 0.0f;
    note = -1;
}

void SynthVoice::render (float* L, float* R, int n, const SynthSound& s, const Mods& mods, const float* vocoderEnv, float unisonSpread)
{
    if (! env.active())
        return;
    std::array<float, kParams> p {};
    for (int i = 0; i < kParams; ++i)
        p[(size_t) i] = clamp01 (s.eng.p[(size_t) i] + mods.eng[(size_t) i]);
    // the envelope: times on a square law
    env.a = 0.001f + std::pow (clamp01 (s.env.p[0] + mods.env[0]), 2.0f) * 5.0f;
    env.d = 0.005f + std::pow (clamp01 (s.env.p[1] + mods.env[1]), 2.0f) * 5.0f;
    env.s = clamp01 (s.env.p[2] + mods.env[2]);
    env.r = 0.005f + std::pow (clamp01 (s.env.p[3] + mods.env[3]), 2.0f) * 6.0f;
    const float fs = (float) sr;
    // block-rate filter settings per engine
    const float keyHz = noteHz (currentNote + mods.pitchSemis);
    switch (s.engine)
    {
        case Engine::Shape:
        {
            const float cut = expRange (p[2], 40.0f, 18000.0f) * (0.6f + 0.6f * env.level);
            f1.set (fs, cut, 0.5f + std::pow (p[3], 2.0f) * 14.0f);
            f2.set (fs, cut, 0.5f + std::pow (p[3], 2.0f) * 14.0f);
            break;
        }
        case Engine::Gene: f1.set (fs, expRange (p[0], 80.0f, 18000.0f), 0.8f); f3.set (fs, keyHz * 2.0f, 2.0f); break;
        case Engine::Warp: f1.set (fs, expRange (p[1], 80.0f, 18000.0f), 1.2f); f2.set (fs, expRange (p[1], 80.0f, 18000.0f), 1.2f); break;
        case Engine::Dual: f1.set (fs, expRange (p[7], 60.0f, 18000.0f) * (0.5f + env2.level), 1.0f); break;
        case Engine::Pulse: f1.set (fs, expRange (p[0], 60.0f, 18000.0f), 1.4f); break;
        case Engine::Volt: f1.set (fs, expRange (p[2], 60.0f, 16000.0f) * (0.4f + env.level), 0.7f + p[2] * 3.0f); break;
        case Engine::Vocoder:
        {
            const int bands = 8 + (int) (p[2] * 24.0f);
            for (int b = 0; b < bands; ++b)
                vocCarrier[(size_t) b].set (fs, 120.0f * std::pow (60.0f, (float) b / (float) bands) * (0.5f + 1.5f * p[1]), 6.0f);
            break;
        }
        default: break;
    }
    const float pan = juce::jlimit (-1.0f, 1.0f, mods.pan + unisonSpread);
    const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
    for (int i = 0; i < n; ++i)
    {
        if (glideRate > 0.0f)
        {
            currentNote += currentNote < glideNote ? std::min (glideRate, glideNote - currentNote) : -std::min (glideRate, currentNote - glideNote);
            if (std::abs (currentNote - glideNote) < 1.0e-4f)
                glideRate = 0.0f;
        }
        const float freq = noteHz (currentNote + mods.pitchSemis);
        float right = 0.0f;
        float left = 0.0f;
        if (s.engine == Engine::Vocoder && vocoderEnv != nullptr)
        {
            // the carrier through a filter bank shaped by the input's band envelopes
            const int bands = 8 + (int) (p[2] * 24.0f);
            const float dt = freq / fs;
            ph[0] += dt;
            if (ph[0] >= 1.0f)
                ph[0] -= 1.0f;
            const float saw = 2.0f * ph[0] - 1.0f - polyBlep (ph[0], dt);
            const float pulse = (ph[0] < 0.5f ? 1.0f : -1.0f);
            const float carrier = saw * (1.0f - p[0]) + pulse * p[0] + noise.next() * 0.05f;
            float voc = 0.0f;
            for (int b = 0; b < bands; ++b)
            {
                vocCarrier[(size_t) b].process (carrier);
                voc += vocCarrier[(size_t) b].bp * vocoderEnv[b] * 6.0f;
            }
            voc = 1.2f * softClip (voc * 8.0f / (float) bands); // (as loud with 8 bands as with 32)
            left = right = voc * p[3] + carrier * (1.0f - p[3]) * 0.3f;
        }
        else
            left = renderEngine (s, p, freq, right);
        const float e = env.next (fs) * velocity * mods.volume;
        env2.next (fs);
        L[i] += left * e * gl;
        R[i] += right * e * gr;
    }
    if (! env.active())
        note = -1;
}

float SynthVoice::renderEngine (const SynthSound& s, const std::array<float, kParams>& p, float freq, float& right)
{
    const float fs = (float) sr;
    const float dt = juce::jlimit (0.0f, 0.45f, freq / fs);
    auto adv = [] (float& phase, float d) {
        phase += d;
        if (phase >= 1.0f)
            phase -= std::floor (phase);
    };
    auto saw = [&] (float phase, float d) { return 2.0f * phase - 1.0f - polyBlep (phase, d); };
    auto square = [&] (float phase, float d, float width) {
        float v = phase < width ? 1.0f : -1.0f;
        v += polyBlep (phase, d);
        float p2 = phase - width;
        if (p2 < 0.0f)
            p2 += 1.0f;
        v -= polyBlep (p2, d);
        return v;
    };
    switch (s.engine)
    {
        case Engine::Swarm:
        {
            // a cloud of detuned saws that blooms open over the wave envelope
            const int count = 1 + (int) std::lround (p[0] * 7.0f);
            holdCount += 1.0f / fs;
            const float bloom = 0.02f + p[1] * 3.0f;
            float l = 0, r = 0;
            for (int k = 0; k < count; ++k)
            {
                const float offset = count == 1 ? 0.0f : ((float) k / (float) (count - 1) - 0.5f);
                const float cents = offset * p[2] * 70.0f;
                const float d = dt * std::pow (2.0f, cents / 1200.0f);
                adv (ph[(size_t) k], d);
                const float amp = juce::jlimit (0.0f, 1.0f, holdCount / bloom * (float) count - (float) k);
                const float v = saw (ph[(size_t) k], d) * amp;
                const float side = 0.5f + offset * p[3];
                l += v * (1.0f - side);
                r += v * side;
            }
            const float norm = 1.0f / std::sqrt ((float) count);
            right = r * norm * 1.2f;
            return l * norm * 1.2f;
        }
        case Engine::Bits:
        {
            adv (ph[0], dt);
            const float x = std::sin (kTwoPi * ph[0]);
            float v = std::sin (x * (1.0f + p[0] * 7.0f)); // a folding shaper
            // octave: a sub octave down below the middle, an octave up above it
            adv (subPh, dt * (p[1] < 0.5f ? 0.5f : 2.0f));
            v += std::sin (kTwoPi * subPh) * std::abs (p[1] - 0.5f) * 1.6f;
            // detune below the middle, ring modulation above
            adv (ph[1], dt * (p[2] < 0.5f ? std::pow (2.0f, p[2] * 0.06f) : 1.0f + (p[2] - 0.5f) * 6.0f));
            const float other = std::sin (kTwoPi * ph[1]);
            v = p[2] < 0.5f ? v + other * 0.7f : v * (1.0f - (p[2] - 0.5f) * 2.0f) + v * other * (p[2] - 0.5f) * 2.0f;
            // digitalness: fewer bits and a lower rate
            const float bits = 16.0f - p[3] * 13.0f;
            const float q = std::pow (2.0f, bits);
            holdCount += 1.0f;
            if (holdCount >= 1.0f + p[3] * p[3] * 30.0f)
            {
                holdCount = 0.0f;
                held1 = std::round (v * 0.5f * q) / q;
            }
            right = held1;
            return held1;
        }
        case Engine::Shape:
        {
            // the waveform morph: noise, pulse, square, saw, saw+sub, and back to noise; width spreads two oscillators
            auto morph = [&] (float phase, float d, float sub) {
                const float pos = p[0] * 5.0f;
                const int a = juce::jlimit (0, 4, (int) pos);
                const float f = pos - (float) a;
                auto wave = [&] (int w) {
                    switch (w)
                    {
                        case 0: return noise.next() * 0.7f;
                        case 1: return square (phase, d, 0.2f);
                        case 2: return square (phase, d, 0.5f);
                        case 3: return saw (phase, d);
                        case 4: return saw (phase, d) + std::sin (kTwoPi * sub) * 0.8f;
                        default: return noise.next() * 0.7f;
                    }
                };
                return wave (a) * (1.0f - f) + wave (a + 1) * f;
            };
            adv (ph[0], dt);
            const float d2 = dt * std::pow (2.0f, p[1] * 0.25f / 12.0f);
            adv (ph[1], d2);
            adv (subPh, dt * 0.5f);
            const float a = morph (ph[0], dt, subPh), b = morph (ph[1], d2, subPh);
            const float w = p[1];
            const float l = f1.process (a * (1.0f - 0.5f * w) + b * 0.5f * w);
            const float r = f2.process (b * (1.0f - 0.5f * w) + a * 0.5f * w);
            right = r * 0.7f;
            return l * 0.7f;
        }
        case Engine::Gene:
        {
            // a harmonic recipe (one of 16 genes, blended), warped in phase, with noise
            adv (ph[0], dt);
            const float pos = p[1] * 15.0f;
            const int g0 = juce::jlimit (0, 15, (int) pos), g1 = std::min (15, g0 + 1);
            const float f = pos - (float) g0;
            const float warp = p[2] * 0.45f;
            const float phw = ph[0] < 0.5f ? ph[0] * (0.5f - warp) / 0.5f : (0.5f - warp) + (ph[0] - 0.5f) * (0.5f + warp) / 0.5f;
            float v = 0.0f, norm = 0.0f;
            for (int h = 0; h < 8; ++h)
            {
                if (freq * (float) (h + 1) > fs * 0.45f)
                    break;
                const float amp = kGenes[g0][h] * (1.0f - f) + kGenes[g1][h] * f;
                v += amp * std::sin (kTwoPi * phw * (float) (h + 1));
                norm += amp;
            }
            v /= std::max (1.0f, norm * 0.6f);
            f3.process (noise.next());
            v += f3.bp * p[3] * 2.0f;
            const float out1 = f1.process (v);
            right = out1;
            return out1;
        }
        case Engine::Warp:
        {
            // a wave shortened within its cycle (type and length), a second copy offset in phase, and chorus
            adv (ph[0], dt);
            const float len = 1.0f + p[0] * 6.0f;
            auto shaped = [&] (float phase) {
                const float q = std::min (1.0f, phase * len);
                const float t = p[0];
                return q >= 1.0f ? 0.0f : (std::sin (kTwoPi * q) * (1.0f - t) + (2.0f * q - 1.0f) * t);
            };
            float other = ph[0] + p[2] * 0.5f;
            if (other >= 1.0f)
                other -= 1.0f;
            const float v = shaped (ph[0]) - shaped (other) * p[2];
            const float l = f1.process (v), r = f2.process (v);
            str1.write (l);
            str2.write (r);
            lastOut += 1.0f / fs;
            const float depth = p[3] * 0.004f * fs;
            const float cl = str1.read (fs * 0.006f + depth * (1.0f + std::sin (kTwoPi * 0.6f * lastOut)));
            const float cr = str2.read (fs * 0.007f + depth * (1.0f + std::cos (kTwoPi * 0.5f * lastOut)));
            right = (r + cr * p[3]) * 0.7f;
            return (l + cl * p[3]) * 0.7f;
        }
        case Engine::Dual:
        {
            // two oscillators: 2 (at frequency) modulates 1 (cross mod); each has its waveform and envelope; the
            // crossfader balances them; one filter (cutoff)
            const float f2Hz = freq * std::pow (2.0f, (p[0] - 0.5f) * 4.0f);
            adv (ph[1], juce::jlimit (0.0f, 0.45f, f2Hz / fs));
            auto wave = [&] (float phase, float w, float d) {
                const float sine = std::sin (kTwoPi * phase), sw = saw (phase, d), sq = square (phase, d, 0.5f);
                return w < 0.5f ? sine * (1.0f - w * 2.0f) + sw * w * 2.0f : sw * (2.0f - w * 2.0f) + sq * (w * 2.0f - 1.0f);
            };
            const float o2 = wave (ph[1], p[5], f2Hz / fs);
            adv (ph[0], juce::jlimit (0.0f, 0.45f, dt * (1.0f + o2 * p[3] * 2.0f)));
            const float o1 = wave (ph[0], p[1], dt);
            holdCount += 1.0f / fs;
            const float e1 = std::exp (-holdCount / (0.02f + p[2] * p[2] * 3.0f));
            const float e2 = std::exp (-holdCount / (0.02f + p[6] * p[6] * 3.0f));
            const float v = o1 * (1.0f - p[4]) * e1 + o2 * p[4] * e2;
            const float filtered = f1.process (v);
            right = filtered * 0.8f;
            return filtered * 0.8f;
        }
        case Engine::Fm:
        {
            // FM: amount, the modulator's ratio, four topologies, detune between the sides
            static const float ratios[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f };
            const float ratio = ratios[juce::jlimit (0, 7, (int) (p[1] * 7.99f))];
            const float index = p[0] * 8.0f * (0.3f + 0.7f * env.level);
            const int topology = juce::jlimit (0, 3, (int) (p[2] * 3.99f));
            adv (ph[1], dt * ratio);
            adv (ph[2], dt * ratio * 2.01f);
            float mod = std::sin (kTwoPi * ph[1]);
            if (topology == 1)
                mod = std::sin (kTwoPi * ph[1] + std::sin (kTwoPi * ph[2]) * index * 0.5f); // a stack
            else if (topology == 2)
                mod = 0.5f * (mod + std::sin (kTwoPi * ph[2]));                          // two modulators
            else if (topology == 3)
            {
                mod = std::sin (kTwoPi * ph[1] + lastOut * index * 0.3f);                // feedback
                lastOut = mod;
            }
            adv (ph[0], dt * std::pow (2.0f, p[3] * 0.08f / 12.0f));
            adv (ph[3], dt * std::pow (2.0f, -p[3] * 0.08f / 12.0f));
            const float l = std::sin (kTwoPi * ph[0] + mod * index);
            right = std::sin (kTwoPi * ph[3] + mod * index) * 0.7f;
            return l * 0.7f;
        }
        case Engine::Phase:
        {
            // phase distortion: the phase bent at a knee (shift, amount), a resonant window (filter), tilted
            adv (ph[0], dt);
            const float knee = juce::jlimit (0.02f, 0.98f, 0.5f - (p[0] - 0.5f) * 0.9f * p[1] * 1.5f);
            const float tilt = 1.0f + (p[3] - 0.5f) * 1.6f;
            const float x = std::pow (ph[0], tilt);
            const float phw = x < knee ? 0.5f * x / knee : 0.5f + 0.5f * (x - knee) / (1.0f - knee);
            float v = -std::cos (kTwoPi * phw);
            if (p[2] > 0.02f)
            {
                const float resonance = 1.0f + p[2] * 14.0f * (0.4f + 0.6f * env.level);
                const float window = 1.0f - ph[0];
                v = v * (1.0f - p[2]) + std::sin (kTwoPi * ph[0] * resonance) * window * p[2];
            }
            right = v * 0.7f;
            return v * 0.7f;
        }
        case Engine::Pulse:
        {
            // a pulse with its width swept (modulation), a second detuned pulse, through a filter
            lastOut += 1.0f / fs;
            const float width = 0.5f - 0.42f * p[3] * (0.5f + 0.5f * std::sin (kTwoPi * 0.8f * lastOut));
            adv (ph[0], dt);
            float v = square (ph[0], dt, width);
            const float d2 = dt * std::pow (2.0f, p[2] * 0.3f / 12.0f + (p[2] > 0.8f ? 1.0f : 0.0f));
            adv (ph[1], d2);
            v += square (ph[1], d2, 0.5f) * p[2];
            v = softClip (v * (0.5f + p[1] * 3.0f));
            const float out1 = f1.process (v);
            right = out1 * 0.8f;
            return out1 * 0.8f;
        }
        case Engine::Sampler:
        {
            if (s.sample == nullptr || s.sample->length() < 2)
                return 0.0f;
            const auto& a = *s.sample;
            const double len = (double) a.length();
            double start = p[0] * len, end = std::max ((double) p[3] * len, start + 1.0);
            const double loopIn = juce::jlimit (start, end, (double) p[1] * len), loopOut = juce::jlimit (loopIn, end, (double) p[2] * len);
            const int direction = p[4] < 0.34f ? 0 : p[4] < 0.67f ? 1 : 2; // forward, reverse, ping-pong loop
            const double rate = std::pow (2.0, (std::log2 (freq / 261.6256f)) + (p[5] - 0.5) * 2.0 / 12.0) * a.sampleRate / sr;
            if (! sampleStarted)
            {
                sampleStarted = true;
                samplePhase = direction == 1 ? end - 1.0 : start;
                sampleForward = direction != 1;
            }
            // loop while held (if the loop has a length), then play on to the end
            const bool looping = ! sampleReleased && loopOut - loopIn > 16.0;
            double pos = samplePhase;
            samplePhase += sampleForward ? rate : -rate;
            if (looping)
            {
                if (sampleForward && samplePhase >= loopOut)
                {
                    if (direction == 2)
                        sampleForward = false;
                    else
                        samplePhase -= loopOut - loopIn;
                }
                else if (! sampleForward && samplePhase < loopIn)
                {
                    if (direction == 2)
                        sampleForward = true;
                    else
                        samplePhase += loopOut - loopIn;
                }
            }
            if ((sampleForward && samplePhase >= end) || (! sampleForward && samplePhase <= start))
            {
                env.stage = Adsr::Stage::Release;
                env.level = std::min (env.level, 0.05f);
            }
            pos = juce::jlimit (0.0, len - 2.0, pos);
            const int i0 = (int) pos;
            const float fr = (float) (pos - i0);
            auto at = [&] (int ch) {
                const float* d = a.data.getReadPointer (std::min (ch, a.data.getNumChannels() - 1));
                return d[i0] + (d[i0 + 1] - d[i0]) * fr;
            };
            float l = at (0), r = at (1);
            // the loop fade: a soft edge near the loop points
            if (looping && p[6] > 0.01f)
            {
                const double fadeLen = std::max (1.0, (loopOut - loopIn) * p[6] * 0.5);
                const double toOut = loopOut - pos, fromIn = pos - loopIn;
                const float g = (float) juce::jlimit (0.0, 1.0, std::min (toOut, fromIn) / fadeLen);
                l *= 0.3f + 0.7f * g;
                r *= 0.3f + 0.7f * g;
            }
            const float gain = p[7] * 1.6f;
            right = r * gain;
            return l * gain;
        }
        case Engine::String:
        {
            // two plucked strings (detune), damped in the loop (tension), ringing for decay
            const float d1 = fs / freq;
            const float d2 = fs / (freq * std::pow (2.0f, p[2] * 0.3f / 12.0f));
            const float fbk = std::pow (0.001f, 1.0f / (freq * (0.15f + p[1] * p[1] * 8.0f)));
            const float bright = 0.15f + p[0] * 0.8f;
            const float a1 = str1.read (d1), a2 = str2.read (d2);
            strLp1 += bright * (a1 - strLp1);
            strLp2 += bright * (a2 - strLp2);
            str1.write (strLp1 * fbk);
            str2.write (strLp2 * fbk);
            right = (a2 * 0.7f + a1 * 0.3f) * 1.2f;
            return (a1 * 0.7f + a2 * 0.3f) * 1.2f;
        }
        case Engine::Volt:
        {
            // a drifting analog oscillator pair (modulation), a hum and noise floor (ground noise), filtered
            if (--driftCount <= 0)
            {
                driftCount = (int) (fs * 0.05f);
                driftTarget = noise.next();
            }
            drift += (driftTarget - drift) * 0.0005f;
            const float d = dt * std::pow (2.0f, drift * p[0] * 0.5f / 12.0f);
            adv (ph[0], d);
            const float d2 = d * std::pow (2.0f, (p[3] * 0.35f) / 12.0f);
            adv (ph[1], d2);
            float v = saw (ph[0], d) * 0.6f + (1.0f - 4.0f * std::abs (ph[1] - 0.5f)) * 0.5f;
            lastOut += 1.0f / fs;
            v += (std::sin (kTwoPi * 50.0f * lastOut) * 0.2f + hum.process (noise.next()) * 0.6f) * p[1];
            const float out1 = f1.process (v);
            right = out1 * 0.8f;
            return out1 * 0.8f;
        }
        case Engine::Vocoder:
        default:
        {
            // (without an input the vocoder plays its carrier, filtered)
            adv (ph[0], dt);
            const float v = saw (ph[0], dt) * (1.0f - p[3] * 0.7f) * 0.4f;
            right = v;
            return v;
        }
    }
}

// ======================================================================== the drum voice

void DrumVoice::prepare (double sampleRate) { sr = sampleRate; }

void DrumVoice::start (int k, const DrumKit& kit, float vel)
{
    key = k;
    on = gate = true;
    t = 0.0f;
    ph1 = ph2 = 0.0f;
    amp1 = amp2 = 1.0f;
    velocity = vel;
    pos = 0.0;
    attackEnv = 0.0f;
    filt.reset();
    const auto& p = kit.keys[(size_t) k].p;
    if (kit.engine == DrumEngine::Sampler && kit.sample != nullptr)
    {
        const double len = (double) kit.sample->length();
        const bool reverse = p[4] >= 0.5f;
        pos = reverse ? p[2] * len - 2.0 : p[1] * len;
    }
}

void DrumVoice::release() { gate = false; }

void DrumVoice::render (float* L, float* R, int n, const DrumKit& kit, const Mods& mods)
{
    if (! on)
        return;
    const float fs = (float) sr;
    const auto& raw = kit.keys[(size_t) key].p;
    std::array<float, kParams> p {};
    for (int i = 0; i < kParams; ++i)
        p[(size_t) i] = clamp01 (raw[(size_t) i] + mods.eng[(size_t) i]);
    // the drum envelope (a transient shaper): attack, gain, release, timing
    const auto& e = kit.env.p;
    const float attackBoost = 1.0f + (clamp01 (e[0] + mods.env[0]) - 0.5f) * 3.0f;
    const float gain = clamp01 (e[1] + mods.env[1]) * 1.6f * velocity * mods.volume;
    const float tail = std::pow (4.0f, (clamp01 (e[2] + mods.env[2]) - 0.5f) * 2.0f);
    const float timing = std::pow (3.0f, (clamp01 (e[3] + mods.env[3]) - 0.5f) * 2.0f);
    const float pan = juce::jlimit (-1.0f, 1.0f, mods.pan + (kit.engine == DrumEngine::Sampler ? (p[5] - 0.5f) * 2.0f : 0.0f));
    const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
    const float pitchMul = std::pow (2.0f, mods.pitchSemis / 12.0f);
    if (kit.engine == DrumEngine::Kit)
    {
        // oscillator 1 (pitch, wave, envelope, cross mod) and oscillator 2 (pitch, wave, envelope), filtered
        const float f1 = 30.0f * std::pow (2.0f, p[0] * 8.0f) * pitchMul;
        const float f2 = 30.0f * std::pow (2.0f, p[4] * 9.0f) * pitchMul;
        const float d1 = (0.01f + std::pow (p[2], 2.0f) * 2.0f) * tail, d2 = (0.005f + std::pow (p[6], 2.0f) * 1.5f) * tail;
        const float cut = 100.0f * std::pow (160.0f, p[7]);
        filt.set (fs, cut, 0.9f);
        for (int i = 0; i < n; ++i)
        {
            t += 1.0f / fs;
            const float transient = std::exp (-t / (0.004f * timing)) * (attackBoost - 1.0f);
            const float bend = 1.0f + (1.5f + p[3] * 3.0f) * std::exp (-t / (0.02f + 0.04f * timing));
            auto wave = [&] (float& phase, float freq, float w) {
                phase += freq / fs;
                if (phase >= 1.0f)
                    phase -= std::floor (phase);
                const float sine = std::sin (kTwoPi * phase), tri = 1.0f - 4.0f * std::abs (phase - 0.5f), nz = noise.next();
                return w < 0.5f ? sine * (1.0f - w * 2.0f) + tri * w * 2.0f : tri * (2.0f - w * 2.0f) + nz * (w * 2.0f - 1.0f);
            };
            const float o2 = wave (ph2, f2, p[5]);
            const float o1 = wave (ph1, f1 * bend * (1.0f + o2 * p[3] * 0.8f), p[1]);
            const float a1 = std::exp (-t / d1), a2 = std::exp (-t / d2);
            float v = o1 * a1 + o2 * a2 * 0.8f;
            filt.process (v);
            v = p[7] < 0.5f ? filt.lp + (v - filt.lp) * (p[7] * 2.0f) : filt.hp + (v - filt.hp) * ((1.0f - p[7]) * 2.0f);
            v *= gain * (1.0f + transient);
            L[i] += v * gl;
            R[i] += v * gr;
            if (a1 < 0.0005f && a2 < 0.0005f)
            {
                on = false;
                break;
            }
        }
        return;
    }
    // the drum sampler: the key's slice (in, out), tuned, in its play mode and direction, with attack and gain
    if (kit.sample == nullptr || kit.sample->length() < 2)
    {
        on = false;
        return;
    }
    const auto& a = *kit.sample;
    const double len = (double) a.length();
    double in = p[1] * len, out = std::max (p[2] * len, in + 2.0);
    const bool reverse = p[4] >= 0.5f;
    const int playMode = p[3] < 0.34f ? 0 : p[3] < 0.67f ? 1 : 2; // one shot, gate, loop
    const double rate = std::pow (2.0, (p[0] - 0.5) * 24.0 / 12.0) * pitchMul * a.sampleRate / sr;
    const float attackSamples = std::max (1.0f, p[6] * p[6] * 0.2f * fs * timing);
    for (int i = 0; i < n; ++i)
    {
        if (playMode == 1 && ! gate)
        {
            amp1 *= 0.995f;
            if (amp1 < 0.001f)
            {
                on = false;
                break;
            }
        }
        if (pos < in || pos >= out - 1.0)
        {
            if (playMode == 2 && gate)
                pos = reverse ? out - 2.0 : in;
            else
            {
                on = false;
                break;
            }
        }
        const int i0 = juce::jlimit (0, (int) len - 2, (int) pos);
        const float fr = (float) (pos - i0);
        const float* dl = a.data.getReadPointer (0);
        const float* dr = a.data.getReadPointer (std::min (1, a.data.getNumChannels() - 1));
        float l = dl[i0] + (dl[i0 + 1] - dl[i0]) * fr, r = dr[i0] + (dr[i0 + 1] - dr[i0]) * fr;
        if (kit.stacked)
            l = r = 0.5f * (l + r);
        attackEnv = std::min (1.0f, attackEnv + 1.0f / attackSamples);
        const float g = gain * p[7] * 1.4f * attackEnv * amp1 * (1.0f + std::exp (-t / (0.004f * timing)) * (attackBoost - 1.0f));
        t += 1.0f / fs;
        L[i] += l * g * gl;
        R[i] += r * g * gr;
        pos += reverse ? -rate : rate;
    }
}
} // namespace keytape::dsp
