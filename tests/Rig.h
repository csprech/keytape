#pragma once

// A Keytape with no window or audio device: runs blocks of audio, presses keys, holds the keyboard, turns the
// encoders, and measures the output.

#include "core/Machine.h"
#include <juce_core/juce_core.h>

namespace keytape::test
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 256;

struct Rig
{
    Machine m;
    std::vector<float> L, R, inL, inR;
    std::function<float (int64_t)> input; // the mic signal (sample index -> value)
    int64_t t = 0;
    float peak = 0.0f;
    double sumSq = 0.0;
    int64_t counted = 0;
    std::vector<float> captured; // the left output, when capture is on
    bool capture = false;
    juce::MidiBuffer midiIn, midiOut;
    int clocksOut = 0;

    Rig()
    {
        m.prepare (kSr, kBlock);
        L.assign (kBlock, 0.0f);
        R.assign (kBlock, 0.0f);
        inL.assign (kBlock, 0.0f);
        inR.assign (kBlock, 0.0f);
    }

    /** Runs `seconds`; returns the output's peak over that time. */
    float run (double seconds)
    {
        peak = 0.0f;
        sumSq = 0.0;
        counted = 0;
        const int64_t total = (int64_t) (seconds * kSr);
        for (int64_t done = 0; done < total; done += kBlock)
        {
            const int n = (int) std::min<int64_t> (kBlock, total - done);
            for (int i = 0; i < n; ++i)
                inL[(size_t) i] = inR[(size_t) i] = input ? input (t + i) : 0.0f;
            Machine::AudioIO io;
            io.micL = inL.data();
            io.micR = inR.data();
            io.outL = L.data();
            io.outR = R.data();
            midiOut.clear();
            m.process (n, io, &midiIn, &midiOut);
            midiIn.clear();
            for (const auto meta : midiOut)
                if (meta.getMessage().isMidiClock())
                    ++clocksOut;
            for (int i = 0; i < n; ++i)
            {
                const float v = std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
                if (! std::isfinite (v))
                    peak = 1.0e9f;
                peak = std::max (peak, v);
                sumSq += (double) L[(size_t) i] * L[(size_t) i];
                if (capture)
                    captured.push_back (L[(size_t) i]);
            }
            counted += n;
            t += n;
            m.idle();
        }
        return peak;
    }
    float rms() const { return counted > 0 ? (float) std::sqrt (sumSq / (double) counted) : 0.0f; }

    void press (Key k) { m.press (k); }
    void release (Key k) { m.release (k); }
    void tapKey (Key k)
    {
        m.press (k);
        m.release (k);
    }
    void shifted (Key k)
    {
        m.press (Key::Shift);
        tapKey (k);
        m.release (Key::Shift);
    }
    void shiftTurn (int e, int d)
    {
        m.press (Key::Shift);
        m.turn (e, d);
        m.release (Key::Shift);
    }
    /** Plays keyboard key `k` for `seconds`, returns the peak while held. */
    float play (int k, double seconds, double tail = 0.0)
    {
        m.keyDown (k, 0.8f);
        const float p = run (seconds);
        m.keyUp (k);
        if (tail > 0.0)
            run (tail);
        return p;
    }
    DisplayState screen() const
    {
        DisplayState d;
        m.describe (d);
        return d;
    }
};

/** A Library that keeps snapshots and mixdowns in memory. */
struct MemoryLibrary : Library
{
    juce::StringArray synthNames, drumNames;
    std::vector<SynthSound> synths;
    std::vector<DrumKit> kits;
    int mixdowns = 0, mixLength = 0, reveals = 0;
    juce::StringArray snapshots (bool drum) override { return drum ? drumNames : synthNames; }
    bool loadSnapshot (bool drum, const juce::String& name, SynthSound* s, DrumKit* k) override
    {
        const int i = (drum ? drumNames : synthNames).indexOf (name);
        if (i < 0)
            return false;
        if (drum && k != nullptr)
            *k = kits[(size_t) i];
        if (! drum && s != nullptr)
            *s = synths[(size_t) i];
        return true;
    }
    void saveSnapshot (bool drum, const SynthSound* s, const DrumKit* k) override
    {
        if (drum && k != nullptr)
        {
            kits.push_back (*k);
            drumNames.add ("snap " + juce::String (kits.size()));
        }
        if (! drum && s != nullptr)
        {
            synths.push_back (*s);
            synthNames.add ("snap " + juce::String (synths.size()));
        }
    }
    void saveMixdown (int, const juce::AudioBuffer<float>&, int length, double) override
    {
        ++mixdowns;
        mixLength = length;
    }
    void reveal() override { ++reveals; }
    juce::StringArray midiDevices() override { return { "test device" }; }
};

inline float sine (int64_t i, float hz = 220.0f, float amp = 0.5f) { return amp * std::sin (6.2831853f * hz * (float) i / (float) kSr); }
} // namespace keytape::test
