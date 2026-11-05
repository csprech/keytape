// The sound: every synth engine and its factory presets, the drum engines, the effects, the LFOs, the master
// section and the tape styles make sound, stay finite and within bounds, and respond to their parameters.

#include "Rig.h"

namespace keytape::test
{
namespace
{
    struct Render
    {
        float peak = 0, rms = 0;
        bool finite = true;
    };

    Render renderSynth (const SynthSound& s, int note = 60, double seconds = 0.5, const float* input = nullptr)
    {
        SynthHost host;
        host.prepare (kSr, kBlock);
        std::vector<float> L (kBlock), R (kBlock), in (kBlock, 0.0f);
        LfoInputs lin;
        host.noteOn (note, 0.8f, s);
        Render r;
        double sum = 0;
        int64_t n = 0;
        for (int64_t done = 0; done < (int64_t) (seconds * kSr); done += kBlock)
        {
            if (input != nullptr)
                for (int i = 0; i < kBlock; ++i)
                    in[(size_t) i] = input[(size_t) ((done + i) % 4800)];
            host.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            for (int i = 0; i < kBlock; ++i)
            {
                if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i]))
                    r.finite = false;
                r.peak = std::max (r.peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
                sum += (double) L[(size_t) i] * L[(size_t) i];
            }
            n += kBlock;
        }
        r.rms = (float) std::sqrt (sum / (double) n);
        return r;
    }

    float zeroCrossHz (const SynthSound& s, int note)
    {
        SynthHost host;
        host.prepare (kSr, kBlock);
        std::vector<float> L (kBlock), R (kBlock), in (kBlock, 0.0f);
        LfoInputs lin;
        host.noteOn (note, 0.8f, s);
        int crossings = 0;
        float last = 0;
        const int blocks = (int) (kSr / kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            host.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            if (b < blocks / 4)
                continue;
            for (int i = 0; i < kBlock; ++i)
            {
                if (last <= 0 && L[(size_t) i] > 0)
                    ++crossings;
                last = L[(size_t) i];
            }
        }
        return (float) crossings / 0.75f;
    }
} // namespace

class DspTests : public juce::UnitTest
{
public:
    DspTests() : juce::UnitTest ("sound", "keytape") {}

    void runTest() override
    {
        beginTest ("every synth engine's factory presets sound, finite and bounded");
        {
            // a vocoder needs something to listen to
            std::vector<float> voice (4800);
            for (int i = 0; i < 4800; ++i)
                voice[(size_t) i] = 0.4f * std::sin (6.2831853f * 180.0f * i / (float) kSr) * (0.5f + 0.5f * std::sin (6.2831853f * 3.0f * i / (float) kSr))
                                    + 0.2f * std::sin (6.2831853f * 1200.0f * i / (float) kSr);
            Machine m;
            m.prepare (kSr, kBlock); // (gives the sampler presets their sample)
            for (int e = 0; e < (int) Engine::Count; ++e)
            {
                const auto& lib = factorySynthPresets ((Engine) e);
                expect (lib.size() >= 4, juce::String (engineName ((Engine) e)) + " has presets");
                for (const auto& preset : lib)
                {
                    auto s = preset.sound;
                    if (s.engine == Engine::Sampler && s.sample == nullptr)
                        s.sample = m.sounds.synth[7].sample;
                    const auto r = renderSynth (s, 60, 0.6, voice.data());
                    expect (r.finite, preset.name + " finite");
                    expect (r.rms > 0.003f, preset.name + " (" + engineName ((Engine) e) + ") sounds: rms " + juce::String (r.rms));
                    expect (r.peak < 2.5f, preset.name + " bounded: " + juce::String (r.peak));
                }
            }
        }

        beginTest ("pitch follows the key (shape engine): an octave up doubles the frequency");
        {
            SynthSound s;
            s.engine = Engine::Shape;
            s.eng.p = { 0.6f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f }; // (a saw, one oscillator, open, no resonance)
            s.env.p = { 0.0f, 0.5f, 1.0f, 0.2f, 0.0f, 0.0f, 0.3f, 0.8f };
            const float a = zeroCrossHz (s, 57), b = zeroCrossHz (s, 69);
            expectWithinAbsoluteError (a, 220.0f, 8.0f);
            expectWithinAbsoluteError (b, 440.0f, 12.0f);
        }

        beginTest ("every engine's four encoder parameters change the sound");
        for (int e = 0; e < (int) Engine::Count; ++e)
        {
            if ((Engine) e == Engine::Vocoder || (Engine) e == Engine::Sampler)
                continue;
            auto base = factorySynthPresets ((Engine) e).front().sound;
            const auto ref = renderSynth (base, 60, 0.3);
            for (int p = 0; p < 4; ++p)
            {
                auto s = base;
                s.eng.p[(size_t) p] = s.eng.p[(size_t) p] > 0.5f ? 0.02f : 0.98f;
                const auto r = renderSynth (s, 60, 0.3);
                expect (std::abs (r.rms - ref.rms) > 1.0e-4f || std::abs (r.peak - ref.peak) > 1.0e-4f,
                        juce::String (engineName ((Engine) e)) + " " + engineParamName ((Engine) e, p) + " changes the sound");
            }
        }

        beginTest ("the envelope: attack is slow when turned up, release lets the note ring");
        {
            auto s = factorySynthPresets (Engine::Shape).front().sound;
            s.env.p = { 0.0f, 0.3f, 1.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.8f };
            SynthHost h;
            h.prepare (kSr, kBlock);
            std::vector<float> L (kBlock), R (kBlock), in (kBlock, 0.0f);
            LfoInputs lin;
            h.noteOn (60, 0.8f, s);
            h.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            float fast = 0;
            for (float v : L)
                fast = std::max (fast, std::abs (v));
            s.env.p[0] = 0.8f;
            SynthHost h2;
            h2.prepare (kSr, kBlock);
            h2.noteOn (60, 0.8f, s);
            h2.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            float slow = 0;
            for (float v : L)
                slow = std::max (slow, std::abs (v));
            expect (slow < fast * 0.3f, "a slow attack starts quietly");
            h.noteOff (60, s);
            for (int b = 0; b < 40; ++b)
                h.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            expectEquals (h.activeVoices(), 0, "a short release ends the note");
        }

        beginTest ("play modes: poly holds six notes, mono one");
        {
            auto s = factorySynthPresets (Engine::Shape).front().sound;
            s.env.p[4] = 0.0f; // poly
            SynthHost h;
            h.prepare (kSr, kBlock);
            for (int n = 0; n < 8; ++n)
                h.noteOn (60 + n, 0.8f, s);
            std::vector<float> L (kBlock), R (kBlock), in (kBlock, 0.0f);
            LfoInputs lin;
            h.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            expectEquals (h.activeVoices(), kVoices);
            s.env.p[4] = 0.3f; // mono
            expect (s.playMode() == PlayMode::Mono);
            SynthHost h2;
            h2.prepare (kSr, kBlock);
            h2.noteOn (60, 0.8f, s);
            h2.noteOn (64, 0.8f, s);
            h2.process (kBlock, L.data(), R.data(), s, in.data(), lin);
            expectEquals (h2.activeVoices(), 1);
        }

        beginTest ("drums: every KIT sound and every sampler slice plays");
        {
            Machine m;
            m.prepare (kSr, kBlock);
            for (int engine = 0; engine < 2; ++engine)
            {
                DrumKit kit = engine == 0 ? m.sounds.drum[1] : factoryKits (DrumEngine::Kit).front().kit;
                expect (kit.engine == (engine == 0 ? DrumEngine::Sampler : DrumEngine::Kit));
                int quiet = 0;
                for (int k = 0; k < kKeys; ++k)
                {
                    DrumHost h;
                    h.prepare (kSr, kBlock);
                    h.noteOn (k, 0.9f, kit);
                    std::vector<float> L (kBlock), R (kBlock);
                    LfoInputs lin;
                    float peak = 0;
                    bool finite = true;
                    for (int b = 0; b < 40; ++b)
                    {
                        h.process (kBlock, L.data(), R.data(), kit, lin);
                        for (int i = 0; i < kBlock; ++i)
                        {
                            finite = finite && std::isfinite (L[(size_t) i]);
                            peak = std::max (peak, std::abs (L[(size_t) i]));
                        }
                    }
                    expect (finite);
                    expect (peak < 2.5f);
                    if (peak < 0.01f)
                        ++quiet;
                }
                expectEquals (quiet, 0, juce::String (engine == 0 ? "sampler" : "kit") + ": every key sounds");
            }
            for (const auto& preset : factoryKits (DrumEngine::Kit))
            {
                DrumHost h;
                h.prepare (kSr, kBlock);
                h.noteOn (0, 0.9f, preset.kit);
                std::vector<float> L (kBlock), R (kBlock);
                LfoInputs lin;
                float peak = 0;
                for (int b = 0; b < 20; ++b)
                {
                    h.process (kBlock, L.data(), R.data(), preset.kit, lin);
                    for (float v : L)
                        peak = std::max (peak, std::abs (v));
                }
                expect (peak > 0.01f, preset.name + " sounds");
            }
        }

        beginTest ("the eight effects: finite, bounded, and each changes the sound");
        for (int f = 0; f < (int) Fx::Count; ++f)
        {
            dsp::Effect fx;
            fx.prepare (kSr);
            Module m;
            m.p = { 0.5f, 0.5f, 0.5f, 0.7f, 0.5f, 0.5f, 0.5f, 0.5f };
            std::vector<float> L (kBlock), R (kBlock);
            dsp::Noise noise;
            float peak = 0;
            double diff = 0;
            bool finite = true;
            for (int b = 0; b < 200; ++b)
            {
                std::vector<float> dry (kBlock);
                for (int i = 0; i < kBlock; ++i)
                {
                    const float v = (b % 20 < 3 ? noise.next() * 0.5f : 0.0f) + 0.3f * std::sin ((float) (b * kBlock + i) * 0.03f);
                    dry[(size_t) i] = L[(size_t) i] = R[(size_t) i] = v;
                }
                fx.process ((Fx) f, m, L.data(), R.data(), kBlock, 120.0);
                for (int i = 0; i < kBlock; ++i)
                {
                    finite = finite && std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
                    peak = std::max (peak, std::abs (L[(size_t) i]));
                    diff += std::abs (L[(size_t) i] - dry[(size_t) i]);
                }
            }
            expect (finite, juce::String (fxName ((Fx) f)) + " finite");
            expect (peak < 3.0f, juce::String (fxName ((Fx) f)) + " bounded: " + juce::String (peak));
            expect (diff > 10.0, juce::String (fxName ((Fx) f)) + " changes the sound");
        }

        beginTest ("the LFOs move the sound");
        for (int l = 0; l < (int) Lfo::Count; ++l)
        {
            LfoRunner runner;
            LfoModule lfo;
            lfo.type = (Lfo) l;
            lfo.on = true;
            lfo.m.p = { 0.6f, 0.9f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f };
            LfoInputs in;
            if ((Lfo) l == Lfo::Sense)
                lfo.m.p[0] = 0.0f; // (listening to the input)
            if ((Lfo) l == Lfo::Midi)
                lfo.m.p = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // (CC 1 to the engine's first parameter)
            std::array<float, 6> lo, hi;
            lo.fill (1e9f);
            hi.fill (-1e9f);
            for (int b = 0; b < 400; ++b)
            {
                in.inputLevel = 0.25f + 0.25f * std::sin (b * 0.1f);
                in.cc[0] = 0.5f + 0.5f * std::sin (b * 0.07f);
                const auto o = runner.run (lfo, kBlock, kSr, false, in, b % 50 == 0, 0.5f, (b % 7) / 7.0f);
                const float v[6] = { o.mods.eng[0], o.mods.pitchSemis, o.mods.volume, o.mods.env[0], o.fxMod.p[0], o.tapeSpeed };
                for (int k = 0; k < 6; ++k)
                {
                    lo[(size_t) k] = std::min (lo[(size_t) k], v[k]);
                    hi[(size_t) k] = std::max (hi[(size_t) k], v[k]);
                }
            }
            float range = 0;
            for (int k = 0; k < 6; ++k)
                range = std::max (range, hi[(size_t) k] - lo[(size_t) k]);
            expect (range > 0.01f, juce::String (lfoName ((Lfo) l)) + " modulates");
        }

        beginTest ("the master section: a flat EQ passes the sound; drive is bounded; every tape style is finite");
        {
            dsp::MasterEq eq;
            eq.prepare (kSr);
            Module m;
            m.p = { 0.5f, 0.5f, 0.5f, 0.5f };
            std::array<bool, 4> on { true, true, true, true };
            std::vector<float> L (kBlock), R (kBlock);
            for (int i = 0; i < kBlock; ++i)
                L[(size_t) i] = R[(size_t) i] = 0.5f * std::sin (i * 0.05f);
            auto ref = L;
            eq.process (m, on, L.data(), R.data(), kBlock);
            double err = 0;
            for (int i = 0; i < kBlock; ++i)
                err = std::max (err, (double) std::abs (L[(size_t) i] - ref[(size_t) i]));
            expect (err < 0.01, "flat eq is transparent: " + juce::String (err));
            dsp::Drive d;
            d.prepare (kSr);
            for (int i = 0; i < kBlock; ++i)
                L[(size_t) i] = R[(size_t) i] = 3.0f * std::sin (i * 0.05f);
            d.process (1.0f, 0.5f, L.data(), R.data(), kBlock);
            for (float v : L)
                expect (std::abs (v) < 1.6f);
            for (int st = 0; st < (int) TapeStyle::Count; ++st)
            {
                dsp::TapeColour c;
                c.prepare (kSr);
                bool finite = true;
                for (int b = 0; b < 50; ++b)
                {
                    for (int i = 0; i < kBlock; ++i)
                        L[(size_t) i] = R[(size_t) i] = 0.5f * std::sin ((b * kBlock + i) * 0.05f);
                    c.process ((TapeStyle) st, L.data(), R.data(), kBlock, 1.0f);
                    for (float v : L)
                        finite = finite && std::isfinite (v) && std::abs (v) < 2.0f;
                }
                expect (finite, tapeStyleName ((TapeStyle) st));
            }
        }

        beginTest ("sounds and kits survive a trip through their files");
        {
            auto s = factorySynthPresets (Engine::Fm)[1].sound;
            s.fx.on = true;
            s.lfo.type = Lfo::Value;
            SynthSound back;
            soundFromJson (soundToJson (s), back);
            expect (back.engine == s.engine && back.eng == s.eng && back.env == s.env && back.fx == s.fx && back.lfo == s.lfo && back.name == s.name);
            auto k = factoryKits (DrumEngine::Kit)[1].kit;
            k.keys[3].p[2] = 0.123f;
            DrumKit kb;
            kitFromJson (kitToJson (k), kb);
            expect (kb.engine == k.engine && kb.keys == k.keys && kb.env == k.env && kb.name == k.name);
            MixerState mx;
            mx.level[2] = 0.3f;
            mx.mute[1] = true;
            mx.fx.type = Fx::Spring;
            MixerState mb;
            mixerFromJson (mixerToJson (mx), mb);
            expect (mb.level == mx.level && mb.mute == mx.mute && mb.fx == mx.fx);
        }
    }
};

static DspTests dspTests;
} // namespace keytape::test
