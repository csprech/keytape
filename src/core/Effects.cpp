// The 8 effects, the master EQ, the master out's drive and the tape styles' coloring.

#include "Dsp.h"

namespace keytape::dsp
{
namespace
{
    float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
    float expRange (float v, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (v)); }
} // namespace

void Effect::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& d : dl)
        d.prepare ((int) (sr * 2.2));
    reset();
    // (the frequency shifter's allpass coefficients: two chains 90 degrees apart over the audio band)
    hilA.c = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
    hilB.c = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
}

void Effect::reset()
{
    for (auto& d : dl)
        d.clear();
    for (auto& s : sv)
        s.reset();
    std::fill (std::begin (fb), std::end (fb), 0.0f);
    env = phase = 0.0f;
}

void Effect::process (Fx type, const Module& m, float* L, float* R, int n, double bpm)
{
    juce::ignoreUnused (bpm);
    if (type != last)
    {
        reset();
        last = type;
    }
    const float fs = (float) sr;
    // the shifted encoders fine-tune: parameter i plus (fine i - 0.5) / 10
    std::array<float, 4> p {};
    for (int i = 0; i < 4; ++i)
        p[(size_t) i] = clamp01 (m.p[(size_t) i] + (m.p[(size_t) i + 4] - 0.5f) * 0.1f * (m.p[(size_t) i + 4] != 0.0f ? 1.0f : 0.0f));
    switch (type)
    {
        case Fx::Comb:
        {
            // a comb tuned to frequency, behind a short delay, its feedback frequency-shifted (sideband)
            const float combDelay = fs / expRange (p[0], 40.0f, 4000.0f);
            const float pre = p[1] * 0.25f * fs;
            const float shiftHz = (p[3] - 0.5f) * 400.0f;
            const float g = p[2] * 0.95f;
            for (int i = 0; i < n; ++i)
            {
                const float in = 0.5f * (L[i] + R[i]);
                dl[0].write (in);
                const float x = dl[0].read (std::max (1.0f, pre));
                float y = x + dl[1].read (combDelay) * g;
                // single-sideband shift of the feedback
                shiftPh += shiftHz / fs;
                if (shiftPh > 1.0f)
                    shiftPh -= 1.0f;
                if (shiftPh < 0.0f)
                    shiftPh += 1.0f;
                const float re = hilA.process (y), im = hilB.process (hilDelay);
                hilDelay = y;
                const float shifted = re * std::cos (kTwoPi * shiftPh) - im * std::sin (kTwoPi * shiftPh);
                dl[1].write (std::abs (shiftHz) > 1.0f ? shifted : y);
                L[i] = L[i] * 0.6f + y * 0.5f;
                R[i] = R[i] * 0.6f + dl[1].read (combDelay * 1.01f) * 0.5f;
            }
            break;
        }
        case Fx::Delay:
        {
            // range picks how long (short slapback .. long), speed sets the time within it; a little ping-pong
            const float maxSec = 0.08f + p[0] * 1.9f;
            const float t = std::max (0.005f, maxSec * (0.1f + 0.9f * p[1])) * fs;
            const float g = p[2] * 0.9f, wet = p[3];
            for (int i = 0; i < n; ++i)
            {
                const float dLeft = dl[0].read (t), dRight = dl[1].read (t * 1.02f);
                op[0].set (fs, 7000.0f);
                dl[0].write (L[i] + op[0].process (dRight) * g);
                dl[1].write (R[i] + dLeft * g);
                L[i] += dLeft * wet;
                R[i] += dRight * wet;
            }
            break;
        }
        case Fx::Cube:
        {
            // four delay lines in a cube (x and y sizes, their sum and difference), fed back through a Hadamard mix (z)
            const float x = 0.003f + p[0] * 0.15f, y = 0.003f + p[1] * 0.15f;
            const float t[4] = { x * fs, y * fs, (x + y) * 0.7f * fs, (std::abs (x - y) + 0.004f) * fs };
            const float g = p[2] * 0.48f, mix = p[3];
            for (int i = 0; i < n; ++i)
            {
                float a = dl[0].read (t[0]), b = dl[1].read (t[1]), c = dl[2].read (t[2]), d = dl[3].read (t[3]);
                const float h0 = a + b + c + d, h1 = a - b + c - d, h2 = a + b - c - d, h3 = a - b - c + d;
                dl[0].write (L[i] + h0 * g);
                dl[1].write (R[i] + h1 * g);
                dl[2].write (L[i] * 0.5f + h2 * g);
                dl[3].write (R[i] * 0.5f + h3 * g);
                L[i] = L[i] * (1.0f - mix * 0.5f) + (a + c) * 0.5f * mix;
                R[i] = R[i] * (1.0f - mix * 0.5f) + (b + d) * 0.5f * mix;
            }
            break;
        }
        case Fx::Hall:
        {
            // a hall: distance is pre-delay and size, gate cuts the tail short, color darkens it
            const float pre = p[0] * 0.12f * fs + 1.0f;
            const float size = 0.4f + p[0] * 0.55f;
            const float damp = 0.1f + (1.0f - p[2]) * 0.7f;
            const float mix = p[3];
            static const float lens[6] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.005f, 0.0017f };
            for (int i = 0; i < n; ++i)
            {
                const float in = 0.5f * (L[i] + R[i]);
                dl[7].write (in);
                const float x = dl[7].read (pre);
                // gate: an envelope on the input holds the tail open, then cuts it
                env = std::max (std::abs (in), env * (1.0f - 1.0f / (fs * (0.05f + (1.0f - p[1]) * 3.0f))));
                const float gateGain = p[1] < 0.02f ? 1.0f : juce::jlimit (0.0f, 1.0f, env * 40.0f);
                float sumL = 0, sumR = 0;
                for (int k = 0; k < 4; ++k)
                {
                    const float y = dl[k].read (lens[k] * fs * (0.6f + size));
                    fb[k] += damp * (y - fb[k]);
                    dl[k].write (x + fb[k] * (0.72f + size * 0.25f) * gateGain);
                    (k % 2 == 0 ? sumL : sumR) += y;
                }
                // diffusion
                const float a1 = dl[4].read (lens[4] * fs);
                dl[4].write (sumL + a1 * 0.5f);
                const float a2 = dl[5].read (lens[5] * fs);
                dl[5].write (sumR + a2 * 0.5f);
                L[i] = L[i] * (1.0f - mix * 0.4f) + (a1 - sumL * 0.5f) * mix * 0.6f;
                R[i] = R[i] * (1.0f - mix * 0.4f) + (a2 - sumR * 0.5f) * mix * 0.6f;
            }
            break;
        }
        case Fx::Sweep:
        {
            // a resonant filter opened by the signal's own level (filter follow), with a second band (frequency 2)
            for (int i = 0; i < n; ++i)
            {
                const float in = std::max (std::abs (L[i]), std::abs (R[i]));
                env += (in - env) * (in > env ? 0.01f : 0.0005f);
                const float cut = expRange (juce::jlimit (0.0f, 1.0f, p[0] + env * p[1] * 3.0f), 60.0f, 16000.0f);
                if ((i & 15) == 0)
                {
                    sv[0].set (fs, cut, 0.6f + p[2] * 10.0f);
                    sv[1].set (fs, cut, 0.6f + p[2] * 10.0f);
                    sv[2].set (fs, expRange (p[3], 100.0f, 8000.0f), 3.0f);
                    sv[3].set (fs, expRange (p[3], 100.0f, 8000.0f), 3.0f);
                }
                const float l = sv[0].process (L[i]), r = sv[1].process (R[i]);
                sv[2].process (L[i]);
                sv[3].process (R[i]);
                L[i] = l + sv[2].bp * 0.6f;
                R[i] = r + sv[3].bp * 0.6f;
            }
            break;
        }
        case Fx::Lofi:
        {
            // a narrow telephone band (tone), fewer bits (gsm), a lower rate (baud), digital dropouts (telemetry)
            sv[0].set (fs, expRange (p[0], 300.0f, 3500.0f), 1.2f);
            sv[1].set (fs, expRange (p[0], 300.0f, 3500.0f), 1.2f);
            const float bits = 12.0f - p[1] * 9.0f, q = std::pow (2.0f, bits);
            const float holdLen = 1.0f + p[2] * p[2] * 40.0f;
            for (int i = 0; i < n; ++i)
            {
                sv[0].process (L[i]);
                sv[1].process (R[i]);
                float l = sv[0].bp * 2.0f + L[i] * 0.1f, r = sv[1].bp * 2.0f + R[i] * 0.1f;
                if (++holdCount >= holdLen)
                {
                    holdCount = 0;
                    hold[0] = std::round (l * q) / q;
                    hold[1] = std::round (r * q) / q;
                    // telemetry: now and then the line drops out or chirps
                    if (noise.next() > 1.0f - p[3] * 0.02f)
                        crushPh = 0.03f * fs;
                }
                if (crushPh > 0.0f)
                {
                    crushPh -= 1.0f;
                    const float chirp = std::sin (crushPh * 0.3f) * 0.15f;
                    L[i] = chirp;
                    R[i] = chirp;
                }
                else
                {
                    L[i] = hold[0];
                    R[i] = hold[1];
                }
            }
            break;
        }
        case Fx::Thump:
        {
            // a low resonance (frequency), transients emphasised (punch), saturation in rounds, and power
            sv[0].set (fs, expRange (p[0], 35.0f, 250.0f), 2.0f);
            sv[1].set (fs, expRange (p[0], 35.0f, 250.0f), 2.0f);
            const int rounds = 1 + (int) (p[2] * 4.0f);
            const float drive = 1.0f + p[3] * 5.0f;
            for (int i = 0; i < n; ++i)
            {
                const float in = std::max (std::abs (L[i]), std::abs (R[i]));
                fb[0] += (in - fb[0]) * 0.02f;   // fast
                fb[1] += (in - fb[1]) * 0.0015f; // slow
                const float transient = 1.0f + std::max (0.0f, fb[0] - fb[1]) * p[1] * 12.0f;
                sv[0].process (L[i]);
                sv[1].process (R[i]);
                float l = (L[i] + sv[0].bp * 1.5f) * transient * drive, r = (R[i] + sv[1].bp * 1.5f) * transient * drive;
                for (int k = 0; k < rounds; ++k)
                {
                    l = std::tanh (l);
                    r = std::tanh (r);
                }
                L[i] = l * (0.8f / std::sqrt (drive));
                R[i] = r * (0.8f / std::sqrt (drive));
            }
            break;
        }
        case Fx::Spring:
        {
            // a spring: dispersive allpasses (turns) in a feedback loop, toned and damped
            const int stages = 2 + (int) (p[1] * 5.0f);
            op[0].set (fs, expRange (p[0], 800.0f, 9000.0f));
            const float g = 0.55f + (1.0f - p[2]) * 0.4f, mix = p[3];
            for (int i = 0; i < n; ++i)
            {
                float x = 0.5f * (L[i] + R[i]) + fb[0] * g;
                for (int k = 0; k < stages; ++k)
                {
                    const float d = (0.0011f + 0.0007f * (float) k) * fs;
                    // (a Schroeder allpass: unity gain, a smeared phase - the spring's dispersion)
                    const float y = dl[k].read (d);
                    const float w = x + y * 0.6f;
                    dl[k].write (w);
                    x = y - w * 0.6f;
                }
                const float tank = dl[7].read (0.035f * fs);
                dl[7].write (x);
                fb[0] = op[0].process (tank);
                L[i] = L[i] * (1.0f - mix * 0.4f) + tank * mix;
                R[i] = R[i] * (1.0f - mix * 0.4f) + dl[7].read (0.039f * fs) * mix;
            }
            break;
        }
        case Fx::Count: break;
    }
}

// ======================================================================== the master EQ

void MasterEq::prepare (double s)
{
    sr = s;
    lastDb = { -99, -99, -99 };
}

void MasterEq::design (int band, float db)
{
    const double A = std::pow (10.0, db / 40.0);
    const double hz = band == 0 ? 120.0 : band == 1 ? 1200.0 : 6000.0;
    const double w = 2.0 * juce::MathConstants<double>::pi * hz / sr, c = std::cos (w), s = std::sin (w);
    double b0, b1, b2, a0, a1, a2;
    if (band == 1)
    {
        const double alpha = s / (2.0 * 0.8);
        b0 = 1 + alpha * A, b1 = -2 * c, b2 = 1 - alpha * A, a0 = 1 + alpha / A, a1 = -2 * c, a2 = 1 - alpha / A;
    }
    else
    {
        const double alpha = s / (2.0 * 0.707), sq = 2.0 * std::sqrt (A) * alpha;
        if (band == 0)
            b0 = A * ((A + 1) - (A - 1) * c + sq), b1 = 2 * A * ((A - 1) - (A + 1) * c), b2 = A * ((A + 1) - (A - 1) * c - sq), a0 = (A + 1) + (A - 1) * c + sq,
            a1 = -2 * ((A - 1) + (A + 1) * c), a2 = (A + 1) + (A - 1) * c - sq;
        else
            b0 = A * ((A + 1) + (A - 1) * c + sq), b1 = -2 * A * ((A - 1) + (A + 1) * c), b2 = A * ((A + 1) + (A - 1) * c - sq), a0 = (A + 1) - (A - 1) * c + sq,
            a1 = 2 * ((A - 1) - (A + 1) * c), a2 = (A + 1) - (A - 1) * c - sq;
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& q = bq[(size_t) (band * 2 + ch)];
        q.b0 = (float) (b0 / a0);
        q.b1 = (float) (b1 / a0);
        q.b2 = (float) (b2 / a0);
        q.a1 = (float) (a1 / a0);
        q.a2 = (float) (a2 / a0);
    }
}

void MasterEq::process (const Module& m, const std::array<bool, 4>& on, float* L, float* R, int n)
{
    if (! on[3])
        return; // (the EQ switched off)
    const float amount = m.p[3] * 2.0f; // (0.5: as set)
    bool any = false;
    for (int b = 0; b < 3; ++b)
    {
        const float db = on[(size_t) b] ? (m.p[(size_t) b] - 0.5f) * 24.0f * amount : 0.0f;
        if (std::abs (db - lastDb[(size_t) b]) > 0.01f)
        {
            design (b, db);
            lastDb[(size_t) b] = db;
        }
        any = any || std::abs (db) > 0.05f;
    }
    if (! any)
        return;
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        for (int b = 0; b < 3; ++b)
        {
            l = bq[(size_t) (b * 2)].process (l);
            r = bq[(size_t) (b * 2 + 1)].process (r);
        }
        L[i] = l;
        R[i] = r;
    }
}

// ======================================================================== drive

void Drive::prepare (double s) { sr = s; }

void Drive::process (float drive, float release, float* L, float* R, int n)
{
    if (drive < 0.005f)
    {
        lastGr = 0.0f;
        return;
    }
    const float fs = (float) sr;
    const float thresholdDb = -drive * 30.0f, ratio = 1.0f + drive * 9.0f, makeupDb = drive * 18.0f;
    const float att = 1.0f - std::exp (-1.0f / (0.002f * fs));
    const float rel = 1.0f - std::exp (-1.0f / ((0.02f + release * release * 1.2f) * fs));
    float maxGr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float level = std::max (std::abs (L[i]), std::abs (R[i]));
        env += (level - env) * (level > env ? att : rel);
        const float db = juce::Decibels::gainToDecibels (env, -100.0f);
        const float over = std::max (0.0f, db - thresholdDb);
        const float gr = over - over / ratio;
        maxGr = std::max (maxGr, gr);
        const float g = juce::Decibels::decibelsToGain (makeupDb - gr);
        float l = L[i] * g, r = R[i] * g;
        if (drive > 0.6f) // pushed hard, it distorts
        {
            const float k = 1.0f + (drive - 0.6f) * 8.0f;
            l = std::tanh (l * k) / std::tanh (k);
            r = std::tanh (r * k) / std::tanh (k);
        }
        L[i] = l;
        R[i] = r;
    }
    lastGr = maxGr;
}

// ======================================================================== tape colour

void TapeColour::prepare (double s)
{
    sr = s;
    wowL.prepare ((int) (s * 0.05));
    wowR.prepare ((int) (s * 0.05));
}

void TapeColour::process (TapeStyle style, float* L, float* R, int n, float speed)
{
    const float fs = (float) sr;
    // slower tape: less treble, more wow (speed 1 = normal)
    const float slow = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (speed));
    float wow = 0, flutter = 0, hiss = 0, sat = 0, top = 20000.0f;
    switch (style)
    {
        case TapeStyle::Studio: top = 19000.0f; break;
        case TapeStyle::Vintage: wow = 0.0012f; flutter = 0.0003f; hiss = 0.0006f; sat = 1.6f; top = 12000.0f; break;
        case TapeStyle::Cassette: wow = 0.003f; flutter = 0.0012f; hiss = 0.003f; sat = 2.2f; top = 8000.0f; break;
        case TapeStyle::MoDisc: top = 14000.0f; break;
        case TapeStyle::Count: break;
    }
    top *= 1.0f - slow * 0.75f;
    wow += slow * 0.004f;
    toneL.set (fs, top, 0.6f);
    toneR.set (fs, top, 0.6f);
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (sat > 0.0f)
        {
            l = std::tanh (l * sat) / std::tanh (sat);
            r = std::tanh (r * sat) / std::tanh (sat);
        }
        if (wow > 0.0f || flutter > 0.0f)
        {
            wowPh += 0.55f / fs;
            flutPh += 9.0f / fs;
            if (wowPh > 1.0f)
                wowPh -= 1.0f;
            if (flutPh > 1.0f)
                flutPh -= 1.0f;
            const float d = (0.006f + wow * std::sin (kTwoPi * wowPh) + flutter * std::sin (kTwoPi * flutPh)) * fs;
            wowL.write (l);
            wowR.write (r);
            l = wowL.read (d);
            r = wowR.read (d);
        }
        l = toneL.process (l) + noise.next() * hiss;
        r = toneR.process (r) + noise.next() * hiss;
        if (style == TapeStyle::MoDisc)
        {
            // block floating point at a few bits: the swirl of lossy data compression
            codecL[(size_t) codecPos] = l;
            codecR[(size_t) codecPos] = r;
            l = codecL[(size_t) ((codecPos + 1) % 64)];
            r = codecR[(size_t) ((codecPos + 1) % 64)];
            if (++codecPos >= 64)
            {
                codecPos = 0;
                for (auto* block : { &codecL, &codecR })
                {
                    float peak = 1.0e-6f;
                    for (auto v : *block)
                        peak = std::max (peak, std::abs (v));
                    const float q = 24.0f / peak;
                    for (auto& v : *block)
                        v = std::round (v * q) / q;
                }
            }
        }
        L[i] = l;
        R[i] = r;
    }
}
} // namespace keytape::dsp
