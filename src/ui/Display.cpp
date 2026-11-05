#include "Display.h"
#include "Widgets.h"

namespace keytape::ui
{
namespace
{
    constexpr float W = 320.0f, H = 160.0f;
    const juce::Colour kBg { 0xff070707 }, kWhite { 0xfff0f0ee }, kDim { 0xff5c5c5c }, kFaint { 0xff2a2a2a }, kRed { 0xffff4a2a };

    juce::Font screenFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Avenir Next Condensed", size, juce::Font::plain).withStyle (bold ? "Demi Bold" : "Medium"));
    }
    juce::Font thinFont (float size) { return juce::Font (juce::FontOptions ("Avenir Next", size, juce::Font::plain).withStyle ("Ultra Light")); }

    void text (juce::Graphics& g, const juce::String& s, float x, float y, float w, float h, float size, juce::Colour c,
               juce::Justification j = juce::Justification::centredLeft, bool bold = false)
    {
        g.setColour (c);
        g.setFont (screenFont (size, bold));
        g.drawText (s, juce::Rectangle<float> (x, y, w, h), j, true);
    }

    juce::Colour enc (int e) { return colours::onScreen (e); }

    juce::String clock (double s)
    {
        s = std::max (0.0, s);
        const int m = (int) (s / 60.0);
        const double r = s - m * 60.0;
        return juce::String (m) + ":" + juce::String ((int) r).paddedLeft ('0', 2) + "." + juce::String ((int) (std::fmod (r, 1.0) * 10.0));
    }

    /** The live trace of the output in a box. */
    void scope (juce::Graphics& g, const DisplayState& d, juce::Rectangle<float> r, juce::Colour c, float gain = 1.0f)
    {
        juce::Path p;
        for (int i = 0; i < 256; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 255.0f;
            const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, d.scope[(size_t) i] * gain * 1.6f) * r.getHeight() * 0.5f;
            i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.1f));
    }

    /** Four concentric arcs: the four parameters (Keytape's dial cluster). */
    void arcs (juce::Graphics& g, const DisplayState& d, juce::Point<float> c, float r0)
    {
        for (int e = 0; e < 4; ++e)
        {
            if (! d.params[(size_t) e].visible)
                continue;
            const float r = r0 - (float) e * 8.5f;
            const float a0 = -2.4f, a1 = 2.4f;
            juce::Path track, fill;
            track.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);
            fill.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a0 + (a1 - a0) * d.params[(size_t) e].value, true);
            g.setColour (kFaint);
            g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (enc (e));
            g.strokePath (fill, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void params (juce::Graphics& g, const DisplayState& d)
    {
        for (int e = 0; e < 4; ++e)
        {
            const auto& p = d.params[(size_t) e];
            const float x = 4.0f + (float) e * 79.0f;
            if (! p.visible)
                continue;
            g.setColour (enc (e));
            g.fillEllipse (x, 135.5f, 5.0f, 5.0f);
            text (g, p.name, x + 8.0f, 131.0f, 70.0f, 12.0f, 9.0f, kDim);
            text (g, p.text, x, 143.0f, 74.0f, 12.0f, 10.5f, kWhite, juce::Justification::centredLeft, true);
            g.setColour (kFaint);
            g.fillRect (x, 156.0f, 74.0f, 1.5f);
            g.setColour (enc (e));
            g.fillRect (x, 156.0f, 74.0f * p.value, 1.5f);
        }
    }

    void miniKeyboard (juce::Graphics& g, juce::Rectangle<float> r, const std::array<bool, kKeys>& held, int mark, std::function<bool (int)> lit = {})
    {
        // two octaves from F, as on the panel: 14 white keys
        static const int whites[] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23 };
        static const float blackAt[] = { 1, 2, 3, 5, 6, 8, 9, 10, 12, 13 }; // (boundaries)
        static const int blacks[] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22 };
        const float kw = r.getWidth() / 14.0f;
        auto on = [&] (int k) { return held[(size_t) k] || k == mark || (lit && lit (k)); };
        for (int i = 0; i < 14; ++i)
        {
            const int k = whites[i];
            auto kr = juce::Rectangle<float> (r.getX() + i * kw + 0.5f, r.getY(), kw - 1.0f, r.getHeight());
            g.setColour (on (k) ? (k == mark ? enc (3) : kWhite) : juce::Colour (0xff3a3a3a));
            g.fillRect (kr);
        }
        for (int i = 0; i < 10; ++i)
        {
            const int k = blacks[i];
            auto kr = juce::Rectangle<float> (kw * 0.6f, r.getHeight() * 0.55f).withCentre ({ r.getX() + blackAt[i] * kw, r.getY() + r.getHeight() * 0.28f });
            g.setColour (on (k) ? (k == mark ? enc (3) : kWhite) : kBg);
            g.fillRect (kr);
            g.setColour (juce::Colour (0xff3a3a3a));
            g.drawRect (kr, 0.6f);
        }
    }

    void waveform (juce::Graphics& g, const DisplayState& d, juce::Rectangle<float> r, int markers)
    {
        if (d.sampleOverview.empty())
        {
            text (g, "no sample: input + key to sample", r.getX(), r.getCentreY() - 6.0f, r.getWidth(), 12.0f, 9.0f, kDim, juce::Justification::centred);
            return;
        }
        const int n = (int) d.sampleOverview.size();
        g.setColour (juce::Colour (0xffcfcfcf));
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) n;
            const float h = std::min (1.0f, d.sampleOverview[(size_t) i] * 1.3f) * r.getHeight() * 0.5f;
            g.fillRect (x, r.getCentreY() - h, std::max (0.8f, r.getWidth() / (float) n - 0.4f), std::max (0.6f, h * 2.0f));
        }
        for (int mk = 0; mk < markers; ++mk)
        {
            const float x = r.getX() + r.getWidth() * d.samplePoints[(size_t) mk];
            g.setColour (enc (markers == 2 ? mk + 1 : mk));
            g.fillRect (x - 0.6f, r.getY() - 2.0f, 1.2f, r.getHeight() + 4.0f);
            juce::Path tri;
            tri.addTriangle (x - 3.0f, r.getY() - 5.0f, x + 3.0f, r.getY() - 5.0f, x, r.getY());
            g.fillPath (tri);
        }
        if (d.zoomed)
            text (g, "zoom", r.getRight() - 30.0f, r.getY() - 10.0f, 30.0f, 9.0f, 8.0f, kDim, juce::Justification::centredRight);
    }

    // ---------------------------------------------------------------- the sound's screens

    void engineScreen (juce::Graphics& g, const DisplayState& d)
    {
        if (d.mode == Mode::Drum)
        {
            if (d.drumEngine == DrumEngine::Sampler)
                waveform (g, d, { 12.0f, 32.0f, 296.0f, 52.0f }, 2);
            else
            {
                scope (g, d, { 12.0f, 26.0f, 170.0f, 62.0f }, kWhite);
                arcs (g, d, { 250.0f, 58.0f }, 34.0f);
            }
            miniKeyboard (g, { 40.0f, 96.0f, 240.0f, 26.0f }, d.keysDown, d.drumKey);
            text (g, "key " + juce::String (d.drumKey + 1), 4.0f, 104.0f, 34.0f, 12.0f, 9.0f, kDim);
            return;
        }
        if (d.engine == Engine::Sampler)
        {
            waveform (g, d, { 12.0f, 34.0f, 296.0f, 66.0f }, 4);
            return;
        }
        scope (g, d, { 12.0f, 28.0f, 170.0f, 88.0f }, kWhite);
        g.setColour (kFaint);
        g.drawRect (12.0f, 24.0f, 170.0f, 96.0f, 0.8f);
        arcs (g, d, { 250.0f, 72.0f }, 40.0f);
        if (d.octave != 0)
            text (g, "oct " + juce::String (d.octave > 0 ? "+" : "") + juce::String (d.octave), 214.0f, 112.0f, 72.0f, 12.0f, 9.0f, kDim, juce::Justification::centred);
    }

    void envelopeScreen (juce::Graphics& g, const DisplayState& d)
    {
        const auto& p = d.envParams;
        const float base = 112.0f, top = 34.0f;
        if (d.mode == Mode::Drum)
        {
            // the drum envelope: attack, gain, release; timing marks where it starts
            const float gain = top + (base - top) * (1.0f - p[1]);
            const float x0 = 30.0f, xa = x0 + 6.0f + p[0] * 80.0f, xr = xa + 20.0f + p[2] * 160.0f;
            juce::Path e;
            e.startNewSubPath (x0, base);
            e.lineTo (xa, gain);
            e.quadraticTo (xa + (xr - xa) * 0.2f, base - 4.0f, xr, base);
            g.setColour (enc (0));
            g.strokePath (e, juce::PathStrokeType (2.0f));
            g.setColour (enc (1));
            g.drawLine (xa, gain, xa + 30.0f, gain, 1.0f);
            g.setColour (enc (3));
            const float xt = x0 + (p[3] - 0.5f) * 40.0f;
            g.drawLine (xt, top, xt, base, 1.0f);
            return;
        }
        if (d.shift)
        {
            // the play modes
            static const char* modes[] = { "poly", "mono", "legato", "unison" };
            const int m = juce::jlimit (0, 3, (int) (p[4] * 3.99f));
            for (int i = 0; i < 4; ++i)
            {
                auto r = juce::Rectangle<float> (20.0f + i * 72.0f, 46.0f, 64.0f, 40.0f);
                g.setColour (i == m ? enc (0) : kFaint);
                g.fillRoundedRectangle (r, 4.0f);
                text (g, modes[i], r.getX(), r.getY(), r.getWidth(), r.getHeight(), 12.0f, i == m ? kBg : kDim, juce::Justification::centred, true);
            }
            return;
        }
        const float wa = 14.0f + p[0] * 70.0f, wd = 14.0f + p[1] * 70.0f, ws = 50.0f, wr = 14.0f + p[3] * 80.0f;
        const float sus = base - (base - top) * p[2];
        float x = 12.0f;
        auto seg = [&] (float x1, float y0, float y1, int e, float w) {
            g.setColour (enc (e));
            g.drawLine (x1, y0, x1 + w, y1, 2.0f);
        };
        seg (x, base, top, 0, wa);
        x += wa;
        seg (x, top, sus, 1, wd);
        x += wd;
        seg (x, sus, sus, 2, ws);
        x += ws;
        seg (x, sus, base, 3, wr);
        g.setColour (kFaint);
        g.drawLine (12.0f, base + 1.0f, 308.0f, base + 1.0f, 0.8f);
    }

    void fxScreen (juce::Graphics& g, const DisplayState& d, const juce::String& name, bool on)
    {
        // the signal going in, through the effect's box, and coming out
        auto box = juce::Rectangle<float> (110.0f, 30.0f, 100.0f, 80.0f);
        g.setColour (on ? kWhite : kDim);
        g.drawRoundedRectangle (box, 6.0f, 1.2f);
        for (int e = 0; e < 4; ++e)
        {
            const float v = d.fxParams[(size_t) e];
            auto bar = juce::Rectangle<float> (box.getX() + 14.0f + e * 20.0f, box.getY() + 12.0f, 10.0f, 56.0f);
            g.setColour (kFaint);
            g.fillRect (bar);
            g.setColour (on ? enc (e) : kDim);
            g.fillRect (bar.withTop (bar.getBottom() - bar.getHeight() * v));
        }
        scope (g, d, { 12.0f, 50.0f, 90.0f, 40.0f }, kDim, 0.7f);
        scope (g, d, { 218.0f, 50.0f, 90.0f, 40.0f }, on ? kWhite : kDim);
        text (g, on ? name : name + " (off)", 0.0f, 112.0f, W, 14.0f, 11.0f, on ? kWhite : kDim, juce::Justification::centred, true);
    }

    void lfoScreen (juce::Graphics& g, const DisplayState& d)
    {
        const auto& p = d.lfoParams;
        const auto r = juce::Rectangle<float> (16.0f, 32.0f, 250.0f, 70.0f);
        auto shapeAt = [&] (float t) -> float {
            switch (d.lfoType)
            {
                case Lfo::Tremolo:
                {
                    const int s = juce::jlimit (0, 4, (int) (p[7] * 4.999f));
                    const float ph = t - std::floor (t);
                    return s == 0 ? std::sin (6.2831853f * ph) : s == 1 ? 1.0f - 2.0f * ph : s == 2 ? 2.0f * std::pow (1.0f - ph, 3.0f) - 1.0f : s == 3 ? (ph < 0.5f ? 1.0f : -1.0f) : (ph < 0.08f ? 1.0f : -0.2f);
                }
                case Lfo::Value:
                {
                    const int s = juce::jlimit (0, 3, (int) (p[6] * 3.999f));
                    const float ph = t - std::floor (t);
                    return s == 0 ? (ph < 0.5f ? 1.0f : -1.0f) : s == 1 ? 2.0f * ph - 1.0f : s == 2 ? 1.0f - 2.0f * ph : std::sin (6.2831853f * ph);
                }
                case Lfo::Random:
                {
                    const int step = (int) std::floor (t * 2.0f);
                    return juce::Random ((juce::int64) step * 7919 + 13).nextFloat() * 2.0f - 1.0f;
                }
                case Lfo::Sense: return std::sin (t * 9.0f) * std::exp (-std::fmod (t, 1.0f) * 3.0f);
                case Lfo::Velocity: return (float) ((int) (t * 3.0f) % 4) / 3.0f * 2.0f - 1.0f;
                default: return std::sin (6.2831853f * t * 0.5f);
            }
        };
        const float cycles = 1.0f + p[0] * 4.0f;
        juce::Path wave;
        for (int i = 0; i <= 200; ++i)
        {
            const float t = (float) i / 200.0f;
            const float y = r.getCentreY() - shapeAt (t * cycles) * r.getHeight() * 0.42f;
            i == 0 ? wave.startNewSubPath (r.getX() + t * r.getWidth(), y) : wave.lineTo (r.getX() + t * r.getWidth(), y);
        }
        g.setColour (d.lfoOn ? kWhite : kDim);
        g.strokePath (wave, juce::PathStrokeType (1.4f));
        // the value now, at the right
        g.setColour (kFaint);
        g.drawLine (284.0f, r.getY(), 284.0f, r.getBottom(), 1.0f);
        g.setColour (d.lfoOn ? enc (3) : kDim);
        g.fillEllipse (279.0f, r.getCentreY() - d.lfoShown * r.getHeight() * 0.42f - 5.0f, 10.0f, 10.0f);
        if (! d.lfoOn)
            text (g, "off", 0.0f, 106.0f, W, 14.0f, 11.0f, kDim, juce::Justification::centred, true);
    }

    // ---------------------------------------------------------------- tape and mixer

    void tapeScreen (juce::Graphics& g, const DisplayState& d)
    {
        const float x0 = 22.0f, x1 = 312.0f, span = 16.0f; // seconds across
        const double from = d.head - 6.0;
        auto xOf = [&] (double s) { return x0 + (float) ((s - from) / span) * (x1 - x0); };
        const float laneTop = 28.0f, laneH = 15.0f, laneGap = 4.0f;
        // bars
        if (d.barSeconds > 0.05)
            for (double b = std::floor (from / d.barSeconds) * d.barSeconds; b < from + span; b += d.barSeconds)
                if (b >= 0.0)
                {
                    g.setColour (kFaint);
                    g.drawLine (xOf (b), laneTop - 2.0f, xOf (b), laneTop + 4 * (laneH + laneGap), 0.6f);
                }
        // the loop
        if (d.loopIn >= 0.0 && d.loopOut > d.loopIn)
        {
            const float a = std::max (x0, xOf (d.loopIn)), b = std::min (x1, xOf (d.loopOut));
            if (b > a)
            {
                g.setColour (enc (0).withAlpha (d.loopOn ? 0.25f : 0.1f));
                g.fillRect (a, laneTop - 4.0f, b - a, 4 * (laneH + laneGap) + 4.0f);
            }
            g.setColour (enc (0));
            g.fillRect (xOf (d.loopIn), laneTop - 6.0f, 1.2f, 4 * (laneH + laneGap) + 6.0f);
            g.fillRect (xOf (d.loopOut), laneTop - 6.0f, 1.2f, 4 * (laneH + laneGap) + 6.0f);
        }
        for (int t = 0; t < kTracks; ++t)
        {
            const float y = laneTop + t * (laneH + laneGap);
            if (t == d.track)
            {
                g.setColour (juce::Colour (0xff1b1b1b));
                g.fillRect (x0, y - 1.0f, x1 - x0, laneH + 2.0f);
            }
            text (g, juce::String (t + 1), 4.0f, y, 14.0f, laneH, 10.0f, t == d.track ? kWhite : kDim, juce::Justification::centred, true);
            if (d.mutes[(size_t) t])
                text (g, "m", 14.0f, y, 8.0f, laneH, 7.0f, kRed, juce::Justification::centred);
        }
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (x0, 0.0f, x1 - x0, H).toNearestInt());
        for (const auto& tk : d.takes)
        {
            const float y = laneTop + tk.track * (laneH + laneGap);
            auto r = juce::Rectangle<float> (xOf (tk.start), y + 1.0f, std::max (1.5f, xOf (tk.end) - xOf (tk.start)), laneH - 2.0f);
            g.setColour (tk.sound ? enc (1) : (tk.track == d.track ? kWhite : juce::Colour (0xffa8a8a8)));
            g.fillRoundedRectangle (r, 2.0f);
            if (tk.active)
            {
                g.setColour (enc (3));
                g.drawRoundedRectangle (r.expanded (1.0f), 2.0f, 1.0f);
            }
        }
        g.restoreState();
        // the head
        const float hx = xOf (d.head);
        g.setColour (d.recording || d.armed ? kRed : kWhite);
        g.fillRect (hx - 0.7f, laneTop - 8.0f, 1.4f, 4 * (laneH + laneGap) + 8.0f);
        juce::Path tri;
        tri.addTriangle (hx - 4.0f, laneTop - 10.0f, hx + 4.0f, laneTop - 10.0f, hx, laneTop - 5.0f);
        g.fillPath (tri);
        // the whole tape (six minutes) in a line
        const float by = 110.0f;
        g.setColour (kFaint);
        g.fillRect (x0, by, x1 - x0, 2.0f);
        for (const auto& tk : d.takes)
        {
            g.setColour (kDim);
            g.fillRect (x0 + (float) (tk.start / d.length) * (x1 - x0), by, std::max (0.8f, (float) ((tk.end - tk.start) / d.length) * (x1 - x0)), 2.0f);
        }
        g.setColour (d.recording ? kRed : kWhite);
        g.fillRect (x0 + (float) (d.head / d.length) * (x1 - x0) - 1.0f, by - 3.0f, 2.0f, 8.0f);
        juce::String state = d.recording ? "rec" : d.armed ? "armed" : d.playing ? (d.reverse ? "rev" : "play") : "stop";
        if (d.breaking)
            state = "break";
        if (d.chopping)
            state = "chop";
        if (std::abs (d.speed - 1.0) > 0.005)
            state << "  " << juce::String (juce::roundToInt (d.speed * 100.0)) << "%";
        text (g, state, 22.0f, 114.0f, 150.0f, 12.0f, 9.0f, d.recording || d.armed ? kRed : kDim);
        static const char* grids[] = { "grid off", "bar", "beat", "1/2 beat" };
        text (g, juce::String (grids[juce::jlimit (0, 3, d.grid)]) + "  " + tapeStyleName (d.style), 140.0f, 114.0f, 172.0f, 12.0f, 9.0f, kDim, juce::Justification::centredRight);
    }

    void mixerScreen (juce::Graphics& g, const DisplayState& d)
    {
        for (int t = 0; t < kTracks; ++t)
        {
            const float cx = 46.0f + t * 76.0f;
            const auto track = juce::Rectangle<float> (cx - 6.0f, 26.0f, 12.0f, 76.0f);
            g.setColour (kFaint);
            g.fillRoundedRectangle (track, 2.0f);
            const auto col = d.mutes[(size_t) t] ? kDim : enc (t);
            g.setColour (col);
            g.fillRoundedRectangle (track.withTop (track.getBottom() - track.getHeight() * d.levels[(size_t) t]), 2.0f);
            // the meter beside it
            const float mv = std::min (1.0f, d.trackMeters[(size_t) t] * 1.2f);
            g.setColour (kWhite.withAlpha (0.8f));
            g.fillRect (cx + 10.0f, track.getBottom() - track.getHeight() * mv, 3.0f, track.getHeight() * mv);
            // pan
            g.setColour (kFaint);
            g.fillRect (cx - 16.0f, 110.0f, 32.0f, 1.2f);
            g.setColour (col);
            g.fillEllipse (cx - 16.0f + 32.0f * d.pans[(size_t) t] - 3.0f, 107.6f, 6.0f, 6.0f);
            if (d.mutes[(size_t) t])
                text (g, "mute", cx - 20.0f, 114.0f, 40.0f, 10.0f, 8.0f, kRed, juce::Justification::centred);
        }
    }

    void eqScreen (juce::Graphics& g, const DisplayState& d)
    {
        const auto r = juce::Rectangle<float> (14.0f, 28.0f, 292.0f, 86.0f);
        auto xOf = [&] (float f) { return r.getX() + r.getWidth() * (std::log10 (f) - std::log10 (20.0f)) / 3.0f; };
        const float amt = d.eq[3] * 2.0f;
        auto db = [&] (int band) { return d.eqOn[(size_t) band] ? (d.eq[(size_t) band] - 0.5f) * 24.0f * amt : 0.0f; };
        auto gainAt = [&] (float f) {
            const float lo = db (0) / (1.0f + std::pow (f / 120.0f, 2.0f));
            const float hi = db (2) * std::pow (f / 6000.0f, 2.0f) / (1.0f + std::pow (f / 6000.0f, 2.0f));
            const float l2 = std::log2 (f / 1200.0f);
            const float mid = db (1) * std::exp (-(l2 * l2) / (2.0f * 0.8f * 0.8f));
            return lo + mid + hi;
        };
        g.setColour (kFaint);
        g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 0.8f);
        for (float f : { 100.0f, 1000.0f, 10000.0f })
            g.drawLine (xOf (f), r.getY(), xOf (f), r.getBottom(), 0.5f);
        juce::Path p;
        for (int i = 0; i <= 160; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 160.0f);
            const float y = r.getCentreY() - juce::jlimit (-24.0f, 24.0f, gainAt (f)) * r.getHeight() / 52.0f;
            i == 0 ? p.startNewSubPath (xOf (f), y) : p.lineTo (xOf (f), y);
        }
        g.setColour (kWhite);
        g.strokePath (p, juce::PathStrokeType (1.6f));
        const float fs[3] = { 120.0f, 1200.0f, 6000.0f };
        for (int b = 0; b < 3; ++b)
        {
            const float y = r.getCentreY() - gainAt (fs[b]) * r.getHeight() / 52.0f;
            g.setColour (enc (b));
            if (d.eqOn[(size_t) b])
                g.fillEllipse (xOf (fs[b]) - 4.5f, y - 4.5f, 9.0f, 9.0f);
            else
                g.drawEllipse (xOf (fs[b]) - 4.5f, y - 4.5f, 9.0f, 9.0f, 1.2f);
        }
    }

    void masterOutScreen (juce::Graphics& g, const DisplayState& d)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto track = juce::Rectangle<float> (40.0f + ch * 34.0f, 28.0f, 18.0f, 86.0f);
            g.setColour (kFaint);
            g.fillRect (track);
            g.setColour (enc (ch));
            g.fillRect (track.withTop (track.getBottom() - track.getHeight() * d.masterOut[(size_t) ch]));
            text (g, ch == 0 ? "L" : "R", track.getX(), track.getBottom() + 1.0f, track.getWidth(), 10.0f, 8.0f, kDim, juce::Justification::centred);
        }
        if (d.linked)
            text (g, "linked", 30.0f, 16.0f, 80.0f, 10.0f, 8.0f, kDim, juce::Justification::centred);
        // drive: a gauge, its needle the gain reduction
        const juce::Point<float> c (210.0f, 92.0f);
        juce::Path track, fill;
        track.addCentredArc (c.x, c.y, 54.0f, 54.0f, 0.0f, -1.3f, 1.3f, true);
        fill.addCentredArc (c.x, c.y, 54.0f, 54.0f, 0.0f, -1.3f, -1.3f + 2.6f * d.masterOut[2], true);
        g.setColour (kFaint);
        g.strokePath (track, juce::PathStrokeType (6.0f));
        g.setColour (enc (2));
        g.strokePath (fill, juce::PathStrokeType (6.0f));
        const float a = -1.3f + 2.6f * juce::jlimit (0.0f, 1.0f, d.gainReduction / 12.0f);
        g.setColour (enc (3));
        g.drawLine (c.x, c.y, c.x + std::sin (a) * 46.0f, c.y - std::cos (a) * 46.0f, 1.6f);
        text (g, "drive", c.x - 30.0f, c.y - 22.0f, 60.0f, 12.0f, 9.0f, kDim, juce::Justification::centred);
    }

    // ---------------------------------------------------------------- tempo, input, output, com

    void tempoScreen (juce::Graphics& g, const DisplayState& d)
    {
        // a metronome swinging on the beat
        const juce::Point<float> pivot (54.0f, 108.0f);
        juce::Path body;
        body.addTriangle (pivot.x - 28.0f, pivot.y + 6.0f, pivot.x + 28.0f, pivot.y + 6.0f, pivot.x, pivot.y - 80.0f);
        g.setColour (kFaint);
        g.fillPath (body);
        const float swing = std::sin ((float) (d.beatPos * juce::MathConstants<double>::pi)) * 0.45f;
        g.setColour (d.metronome > 0.0f ? enc (3) : kWhite);
        g.drawLine (pivot.x, pivot.y, pivot.x + std::sin (swing) * 70.0f, pivot.y - std::cos (swing) * 70.0f, 2.0f);
        g.fillEllipse (pivot.x + std::sin (swing) * 50.0f - 4.0f, pivot.y - std::cos (swing) * 50.0f - 4.0f, 8.0f, 8.0f);
        // the bpm, large
        g.setColour (kWhite);
        g.setFont (thinFont (46.0f));
        g.drawText (d.sync == Sync::MidiSync && d.externalClock ? juce::String (d.bpm, 1) : juce::String (d.bpm, d.bpm == std::floor (d.bpm) ? 0 : 1),
                    juce::Rectangle<float> (110.0f, 30.0f, 200.0f, 54.0f), juce::Justification::centred);
        text (g, "bpm", 110.0f, 82.0f, 200.0f, 12.0f, 9.0f, kDim, juce::Justification::centred);
        text (g, syncName (d.sync) + juce::String (d.sync == Sync::MidiSync ? (d.externalClock ? " (locked)" : " (waiting)") : ""), 110.0f, 96.0f, 200.0f, 12.0f, 10.0f, enc (1), juce::Justification::centred, true);
        // the four beats of the bar
        const int beat = (int) std::floor (d.beatPos) % 4;
        for (int b = 0; b < 4; ++b)
        {
            g.setColour (b == beat ? (b == 0 ? enc (3) : kWhite) : kFaint);
            g.fillEllipse (178.0f + b * 14.0f, 113.0f, 7.0f, 7.0f);
        }
    }

    void inputScreen (juce::Graphics& g, const DisplayState& d)
    {
        text (g, inputName (d.inputSource), 12.0f, 24.0f, 200.0f, 16.0f, 13.0f, kWhite, juce::Justification::centredLeft, true);
        text (g, d.inputOn ? "monitoring on" : "monitoring off", 200.0f, 24.0f, 108.0f, 16.0f, 9.0f, d.inputOn ? enc (3) : kDim, juce::Justification::centredRight);
        // the level, and the threshold sampling waits for
        const auto bar = juce::Rectangle<float> (12.0f, 46.0f, 296.0f, 8.0f);
        g.setColour (kFaint);
        g.fillRect (bar);
        const float lv = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (std::max (1.0e-5f, d.inputLevel)) + 48.0f) / 48.0f);
        g.setColour (lv > 0.97f ? kRed : kWhite);
        g.fillRect (bar.withWidth (bar.getWidth() * lv));
        const float th = bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (std::max (1.0e-5f, d.threshold * 0.5f)) + 48.0f) / 48.0f);
        g.setColour (enc (2));
        g.fillRect (th - 0.8f, bar.getY() - 4.0f, 1.6f, bar.getHeight() + 8.0f);
        if (d.inputSource == Input::Radio)
        {
            // the band, its stations, and the needle
            const auto band = juce::Rectangle<float> (12.0f, 74.0f, 296.0f, 30.0f);
            auto xOf = [&] (double f) { return band.getX() + band.getWidth() * (float) ((f - 87.5) / 20.5); };
            for (double f = 88.0; f <= 108.0; f += 1.0)
            {
                g.setColour (kDim);
                const float h = std::fmod (f, 4.0) == 0.0 ? 10.0f : 5.0f;
                g.fillRect (xOf (f) - 0.4f, band.getBottom() - h, 0.8f, h);
                if (std::fmod (f, 4.0) == 0.0)
                    text (g, juce::String ((int) f), xOf (f) - 12.0f, band.getBottom() + 1.0f, 24.0f, 10.0f, 7.5f, kDim, juce::Justification::centred);
            }
            for (double s : d.stations)
            {
                g.setColour (kWhite);
                g.fillEllipse (xOf (s) - 2.0f, band.getY() + 2.0f, 4.0f, 4.0f);
            }
            g.setColour (enc (1));
            g.fillRect (xOf (d.fmMHz) - 0.8f, band.getY(), 1.6f, band.getHeight());
            text (g, juce::String (d.fmMHz, 1) + " MHz" + (d.stationName.isNotEmpty() ? "  " + d.stationName : juce::String()), 12.0f, 58.0f, 296.0f, 14.0f, 10.0f, enc (1), juce::Justification::centred, true);
        }
        else
            scope (g, d, { 12.0f, 66.0f, 296.0f, 46.0f }, kDim);
    }

    void outputScreen (juce::Graphics& g, const DisplayState& d)
    {
        if (d.outputMode == 0)
        {
            text (g, "mixdown", 12.0f, 24.0f, 140.0f, 16.0f, 12.0f, kWhite, juce::Justification::centredLeft, true);
            // the side being recorded, as a strip of output
            const auto r = juce::Rectangle<float> (40.0f, 46.0f, 240.0f, 46.0f);
            g.setColour (kFaint);
            g.drawRoundedRectangle (r, 6.0f, 1.2f);
            text (g, "side " + juce::String (d.side == 0 ? "a" : "b"), r.getX() + 8.0f, r.getY() + 4.0f, 100.0f, 14.0f, 11.0f, enc (2), juce::Justification::centredLeft, true);
            g.setColour (d.mixRecording ? kRed : kDim);
            g.setFont (thinFont (24.0f));
            g.drawText (clock (d.mixSeconds), r.reduced (8.0f, 4.0f), juce::Justification::bottomRight);
            if (d.mixRecording)
                g.fillEllipse (r.getX() + 10.0f, r.getBottom() - 16.0f, 8.0f, 8.0f);
            static const char* t[] = { "T1 side", "T2 stop", "T3 flip", "T4 rec" };
            for (int i = 0; i < 4; ++i)
                text (g, t[i], 12.0f + i * 76.0f, 100.0f, 72.0f, 12.0f, 9.0f, kDim, juce::Justification::centred);
        }
        else
        {
            text (g, "fm transmit", 12.0f, 24.0f, 200.0f, 16.0f, 12.0f, kWhite, juce::Justification::centredLeft, true);
            g.setColour (kWhite);
            g.setFont (thinFont (36.0f));
            g.drawText (juce::String (d.fmMHz, 1), juce::Rectangle<float> (0.0f, 46.0f, W, 40.0f), juce::Justification::centred);
            text (g, d.fmLive ? "on air (nothing broadcasts from a computer)" : "T1 on air", 0.0f, 92.0f, W, 12.0f, 9.0f, d.fmLive ? enc (3) : kDim, juce::Justification::centred);
        }
    }

    void listColumn (juce::Graphics& g, const juce::StringArray& items, int index, juce::Rectangle<float> r, juce::Colour hi, bool focus)
    {
        const int rows = (int) (r.getHeight() / 13.0f);
        const int first = juce::jlimit (0, std::max (0, items.size() - rows), index - rows / 2);
        for (int i = 0; i < rows && first + i < items.size(); ++i)
        {
            const int k = first + i;
            auto row = juce::Rectangle<float> (r.getX(), r.getY() + i * 13.0f, r.getWidth(), 12.0f);
            if (k == index)
            {
                g.setColour (focus ? hi : hi.withAlpha (0.35f));
                g.fillRoundedRectangle (row, 2.0f);
            }
            text (g, items[k], row.getX() + 4.0f, row.getY(), row.getWidth() - 8.0f, row.getHeight(), 10.0f, k == index && focus ? kBg : kWhite, juce::Justification::centredLeft, k == index);
        }
    }

    void sequencerScreen (juce::Graphics& g, const DisplayState& d)
    {
        if (! d.seqOn)
            text (g, "off", 0.0f, 112.0f, W, 12.0f, 10.0f, kDim, juce::Justification::centred);
        switch (d.seqType)
        {
            case Seq::Arp:
            {
                std::array<bool, kKeys> held {};
                for (int n : d.seqNotes)
                    if (n - kLowestNote >= 0 && n - kLowestNote < kKeys)
                        held[(size_t) (n - kLowestNote)] = true;
                miniKeyboard (g, { 30.0f, 74.0f, 260.0f, 34.0f }, held, -1);
                for (int s = 0; s < 16; ++s)
                {
                    g.setColour (s == d.seqStep % 16 ? enc (0) : kFaint);
                    g.fillRoundedRectangle (30.0f + s * 16.4f, 40.0f, 12.0f, 18.0f, 2.0f);
                }
                break;
            }
            case Seq::Chain:
            {
                const int n = (int) d.seqNotes.size();
                const int cur = n > 0 ? (d.seqStep + n - 1) % n : -1;
                for (int i = 0; i < std::min (n, 32); ++i)
                {
                    const float x = 12.0f + (i % 16) * 18.5f, y = 34.0f + (i / 16) * 38.0f;
                    const int v = d.seqNotes[(size_t) i];
                    g.setColour (i == cur && d.seqOn ? enc (3) : kFaint);
                    g.fillRoundedRectangle (x, y, 16.0f, 32.0f, 2.0f);
                    if (v >= 0)
                    {
                        g.setColour (kWhite);
                        g.fillRect (x + 3.0f, y + 28.0f - juce::jlimit (0.0f, 24.0f, (float) (v - kLowestNote)), 10.0f, 2.0f);
                    }
                    else
                        text (g, v == -1 ? "-" : "~", x, y, 16.0f, 32.0f, 10.0f, kDim, juce::Justification::centred);
                }
                if (n == 0)
                    text (g, "shift + keys to enter notes", 0.0f, 60.0f, W, 14.0f, 10.0f, kDim, juce::Justification::centred);
                break;
            }
            case Seq::Hands:
            case Seq::Grid:
            {
                const bool grid = d.seqType == Seq::Grid;
                for (int s = 0; s < 16; ++s)
                {
                    const float x = 12.0f + s * 18.5f;
                    const bool inLen = s < d.seqLength;
                    g.setColour (s == d.seqCursor ? enc (0).withAlpha (0.5f) : (s == d.seqStep % std::max (1, d.seqLength) && d.seqOn ? juce::Colour (0xff3a3a3a) : kFaint));
                    g.fillRoundedRectangle (x, 30.0f, 16.0f, 78.0f, 2.0f);
                    if (! inLen)
                        continue;
                    auto dot = [&] (int note) {
                        const float y = 104.0f - juce::jlimit (0.0f, 72.0f, (float) (note - kLowestNote + 12) * 2.0f);
                        g.setColour (kWhite);
                        g.fillRect (x + 2.0f, y, 12.0f, 3.0f);
                    };
                    if (grid)
                        for (int note : d.seqGrid[(size_t) s])
                            dot (note);
                    else if (d.seqHands[(size_t) s] >= 0)
                        dot (d.seqHands[(size_t) s]);
                }
                break;
            }
            case Seq::Hold:
            {
                std::array<bool, kKeys> held {};
                for (int n : d.drones)
                    if (n - kLowestNote >= 0 && n - kLowestNote < kKeys)
                        held[(size_t) (n - kLowestNote)] = true;
                miniKeyboard (g, { 30.0f, 52.0f, 260.0f, 40.0f }, held, -1);
                const float bx = 30.0f + 260.0f * (float) (d.holdBreak - kLowestNote) / (float) kKeys;
                g.setColour (enc (0));
                g.fillRect (bx - 1.0f, 40.0f, 2.0f, 64.0f);
                text (g, "drone | play", bx - 40.0f, 28.0f, 80.0f, 10.0f, 8.0f, kDim, juce::Justification::centred);
                break;
            }
            case Seq::Draw:
            {
                const auto r = juce::Rectangle<float> (12.0f, 30.0f, 296.0f, 80.0f);
                g.setColour (kFaint);
                g.drawRect (r, 0.6f);
                juce::Path p;
                bool open = false;
                for (int x = 0; x < 64; ++x)
                {
                    const int v = d.seqDraw[(size_t) x];
                    if (v < 0)
                    {
                        open = false;
                        continue;
                    }
                    const float px = r.getX() + r.getWidth() * (x + 0.5f) / 64.0f, py = r.getBottom() - r.getHeight() * (v + 0.5f) / 24.0f;
                    open ? p.lineTo (px, py) : p.startNewSubPath (px, py);
                    open = true;
                }
                g.setColour (kWhite);
                g.strokePath (p, juce::PathStrokeType (1.6f));
                g.setColour (enc (0));
                g.fillRect (r.getX() + r.getWidth() * (d.seqCursor + 0.5f) / 64.0f - 0.6f, r.getY(), 1.2f, r.getHeight());
                if (d.seqOn)
                {
                    g.setColour (enc (3));
                    g.fillRect (r.getX() + r.getWidth() * (d.seqStep % 64 + 0.5f) / 64.0f - 0.6f, r.getY(), 1.2f, r.getHeight());
                }
                break;
            }
            case Seq::Spin:
            {
                const juce::Point<float> c (160.0f, 70.0f);
                const float R = 42.0f;
                juce::Path poly;
                for (int i = 0; i <= d.spinSides; ++i)
                {
                    const float a = d.spinAngle + juce::MathConstants<float>::twoPi * (float) i / (float) d.spinSides + juce::MathConstants<float>::pi / (float) d.spinSides;
                    const juce::Point<float> pt (c.x + std::cos (a) * R, c.y + std::sin (a) * R);
                    i == 0 ? poly.startNewSubPath (pt) : poly.lineTo (pt);
                }
                g.setColour (kWhite);
                g.strokePath (poly, juce::PathStrokeType (1.4f));
                g.setColour (enc (3));
                for (auto& b : d.balls)
                    g.fillEllipse (c.x + b.first * R - 3.0f, c.y + b.second * R - 3.0f, 6.0f, 6.0f);
                break;
            }
            default: break;
        }
    }

    void guideScreen (juce::Graphics& g, const DisplayState& d)
    {
        listColumn (g, guideTitles(), d.listIndex, { 6.0f, 24.0f, 92.0f, 104.0f }, enc (0), true);
        g.setColour (kWhite);
        g.setFont (screenFont (9.5f));
        g.drawFittedText (guideText (d.listIndex), juce::Rectangle<int> (106, 24, 208, 130), juce::Justification::topLeft, 12, 1.0f);
    }

    void eraseScreen (juce::Graphics& g, const DisplayState& d)
    {
        text (g, "erase " + d.tapeName + "?", 0.0f, 30.0f, W, 18.0f, 14.0f, kRed, juce::Justification::centred, true);
        for (int t = 0; t < 4; ++t)
        {
            auto r = juce::Rectangle<float> (40.0f + t * 62.0f, 60.0f, 52.0f, 36.0f);
            g.setColour (d.eraseKeys[(size_t) t] ? kRed : kFaint);
            g.fillRoundedRectangle (r, 4.0f);
            text (g, "T" + juce::String (t + 1), r.getX(), r.getY(), r.getWidth(), r.getHeight(), 12.0f, kWhite, juce::Justification::centred, true);
        }
        text (g, "press all four to erase it for good", 0.0f, 104.0f, W, 12.0f, 9.0f, kDim, juce::Justification::centred);
    }

    void soundPathScreen (juce::Graphics& g, const DisplayState& d)
    {
        const int n = d.list.size();
        for (int i = 0; i < n; ++i)
        {
            const int row = i < 4 ? 0 : 1;
            const int col = row == 0 ? i : (n - 1 - i);
            auto r = juce::Rectangle<float> (14.0f + col * 76.0f, 34.0f + row * 46.0f, 64.0f, 28.0f);
            g.setColour (kFaint);
            g.fillRoundedRectangle (r, 4.0f);
            text (g, d.list[i], r.getX() + 2.0f, r.getY(), r.getWidth() - 4.0f, r.getHeight(), 9.0f, d.list[i].startsWith ("(") ? kDim : kWhite, juce::Justification::centred);
            g.setColour (kDim);
            if (row == 0 && i < 3)
                g.drawArrow ({ r.getRight() + 1.0f, r.getCentreY(), r.getRight() + 11.0f, r.getCentreY() }, 1.0f, 5.0f, 5.0f);
            if (i == 3)
                g.drawArrow ({ r.getCentreX(), r.getBottom() + 1.0f, r.getCentreX(), r.getBottom() + 17.0f }, 1.0f, 5.0f, 5.0f);
            if (row == 1 && i < n - 1)
                g.drawArrow ({ r.getX() - 1.0f, r.getCentreY(), r.getX() - 11.0f, r.getCentreY() }, 1.0f, 5.0f, 5.0f);
        }
        juce::ignoreUnused (d);
    }

    void comScreen (juce::Graphics& g, const DisplayState& d)
    {
        static const char* names[] = { "midi", "ctrl", "list", "disk" };
        for (int i = 0; i < 4; ++i)
        {
            auto r = juce::Rectangle<float> (14.0f + i * 75.0f, 40.0f, 66.0f, 52.0f);
            g.setColour (kFaint);
            g.fillRoundedRectangle (r, 5.0f);
            text (g, names[i], r.getX(), r.getY() + 8.0f, r.getWidth(), 20.0f, 13.0f, kWhite, juce::Justification::centred, true);
            text (g, "T" + juce::String (i + 1), r.getX(), r.getY() + 30.0f, r.getWidth(), 14.0f, 9.0f, kDim, juce::Justification::centred);
        }
        text (g, d.settings.usbCharging ? "power: usb (full)" : "power: battery (full)", 14.0f, 104.0f, 292.0f, 12.0f, 9.0f, kDim);
    }

    void menuText (juce::Graphics& g, const juce::String& big, const juce::String& small)
    {
        text (g, big, 0.0f, 44.0f, W, 22.0f, 16.0f, kWhite, juce::Justification::centred, true);
        g.setColour (kDim);
        g.setFont (screenFont (9.5f));
        g.drawFittedText (small, juce::Rectangle<int> (20, 72, 280, 50), juce::Justification::centredTop, 4, 1.0f);
    }

    void topBar (juce::Graphics& g, const DisplayState& d)
    {
        text (g, d.title, 6.0f, 2.0f, 200.0f, 16.0f, 12.0f, kWhite, juce::Justification::centredLeft, true);
        juce::String right;
        if (d.mode == Mode::Tape || d.page == Page::TapePage)
            right = clock (d.head);
        else if (d.page == Page::Engine || d.page == Page::Envelope || d.page == Page::Fx || d.page == Page::Lfo)
            right = juce::String (d.mode == Mode::Drum ? "kit " : "sound ") + juce::String (d.slot + 1);
        else
            right = juce::String (juce::roundToInt (d.bpm)) + " bpm";
        text (g, right, 200.0f, 2.0f, 114.0f, 16.0f, 11.0f, d.recording ? kRed : kDim, juce::Justification::centredRight, true);
        if (d.shift)
        {
            g.setColour (kWhite);
            g.fillRoundedRectangle (150.0f, 5.0f, 30.0f, 10.0f, 2.0f);
            text (g, "shift", 150.0f, 5.0f, 30.0f, 10.0f, 8.0f, kBg, juce::Justification::centred, true);
        }
    }

    void overlays (juce::Graphics& g, const DisplayState& d)
    {
        if (d.help.isNotEmpty())
        {
            auto r = juce::Rectangle<float> (16.0f, 30.0f, 288.0f, 70.0f);
            g.setColour (juce::Colour (0xf0181818));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (enc (3));
            g.drawRoundedRectangle (r, 6.0f, 1.2f);
            g.setColour (kWhite);
            g.setFont (screenFont (11.0f));
            g.drawFittedText (d.help, r.reduced (10.0f, 6.0f).toNearestInt(), juce::Justification::centred, 4, 1.0f);
        }
        if (d.popup.isNotEmpty())
        {
            const float w = std::min (260.0f, 30.0f + 6.0f * (float) d.popup.length());
            auto r = juce::Rectangle<float> (w, 18.0f).withCentre ({ W * 0.5f, 30.0f });
            g.setColour (kWhite);
            g.fillRoundedRectangle (r, 9.0f);
            text (g, d.popup, r.getX(), r.getY(), r.getWidth(), r.getHeight(), 10.5f, kBg, juce::Justification::centred, true);
        }
    }
} // namespace

const juce::StringArray& guideTitles()
{
    static const juce::StringArray t { "the keys", "synth", "drum", "tape", "mixer", "sequencers", "tempo and sync", "com and midi" };
    return t;
}

juce::String guideText (int page)
{
    switch (juce::jlimit (0, 7, page))
    {
        case 0: return "shift gives every key and encoder a second job. help + a key explains it. the four mode keys: synth, drum, tape, mixer. "
                       "T1-T4 open the four parts of a sound, the tape's tracks, or the mixer's screens.";
        case 1: return "1-8 pick a sound; shift + 1-8 opens its presets; hold one for three seconds to save a snapshot. "
                       "T1 engine, T2 envelope (shift: play mode), T3 effect, T4 lfo; press T3 or T4 again for on and off. "
                       "the arrows shift the octave.";
        case 2: return "each key a drum. the encoders edit the last key played. the sampler slices one sample across the keys: "
                       "input, then hold a key to sample into it. drop a lifted take to make a kit of it.";
        case 3: return "record, play, stop. T1-T4 choose the track. 1 loop in, 2 out, 3 loop, 4 break, 5 reverse, 6 chop, 7 and 8 memos. "
                       "lift, drop, split (shift: join). blue scrubs, shift + blue slides a take. shift + record arms with the reels still.";
        case 4: return "T1 levels (shift: pan; tap: mute), T2 eq, T3 the master effect, T4 master out with drive. shift + T1-T4 mute. "
                       "shift + mixer shows the sound path.";
        case 5: return "the sequencer key turns it on; shift opens the seven: arp, chain, hands, hold, grid, draw, spin. "
                       "shift + keys enter notes where a sequencer takes them.";
        case 6: return "tempo: blue bpm, ochre sync, gray tape speed, orange metronome. press tempo to tap. "
                       "beat match runs the tape at the tempo; midi sync follows a clock; sync out pulses on the left output.";
        default: return "com: T1 midi (channel, clock, notes), T2 ctrl (the keys and encoders play something else), T3 the devices, "
                        "T4 opens the Keytape folder with the tapes, mixdowns and snapshots.";
    }
}

void paintDisplay (juce::Graphics& g, juce::Rectangle<float> area, const DisplayState& d)
{
    g.setColour (kBg);
    g.fillRect (area);
    const float sc = std::min (area.getWidth() / W, area.getHeight() / H);
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    g.addTransform (juce::AffineTransform::scale (sc).translated (area.getCentreX() - W * sc * 0.5f, area.getCentreY() - H * sc * 0.5f));
    topBar (g, d);
    switch (d.page)
    {
        case Page::Engine: engineScreen (g, d); break;
        case Page::Envelope: envelopeScreen (g, d); break;
        case Page::Fx: fxScreen (g, d, fxName (d.fxType), d.fxOn); break;
        case Page::Lfo: lfoScreen (g, d); break;
        case Page::TapePage: tapeScreen (g, d); break;
        case Page::MixerPage: mixerScreen (g, d); break;
        case Page::EqPage: eqScreen (g, d); break;
        case Page::MasterFxPage: fxScreen (g, d, fxName (d.fxType), d.fxOn); break;
        case Page::MasterOutPage: masterOutScreen (g, d); break;
        case Page::TempoPage: tempoScreen (g, d); break;
        case Page::InputPage: inputScreen (g, d); break;
        case Page::OutputPage: outputScreen (g, d); break;
        case Page::SequencerPage: sequencerScreen (g, d); break;
        case Page::Guide: guideScreen (g, d); break;
        case Page::EraseConfirm: eraseScreen (g, d); break;
        case Page::SoundPath: soundPathScreen (g, d); break;
        case Page::ComPage: comScreen (g, d); break;
        case Page::PresetBrowser:
        case Page::TapeBrowser:
            listColumn (g, d.list, d.listIndex, { 8.0f, 24.0f, 140.0f, 104.0f }, enc (0), d.page == Page::PresetBrowser);
            listColumn (g, d.list2, d.list2Index, { 160.0f, 24.0f, 152.0f, 104.0f }, enc (1), true);
            if (d.page == Page::TapeBrowser)
            {
                static const char* taps[] = { "tap: rename", "load", "new", "erase" };
                for (int i = 0; i < 4; ++i)
                    text (g, taps[i], 4.0f + i * 79.0f, 140.0f, 74.0f, 12.0f, 9.0f, enc (i));
            }
            break;
        case Page::EngineBrowser:
        case Page::FxBrowser:
        case Page::LfoBrowser:
        case Page::SeqBrowser:
            listColumn (g, d.list, d.listIndex, { 70.0f, 24.0f, 180.0f, 104.0f }, enc (0), true);
            text (g, "tap blue to choose", 0.0f, 140.0f, W, 12.0f, 9.0f, kDim, juce::Justification::centred);
            break;
        case Page::ListPage:
            listColumn (g, d.list, -1, { 20.0f, 28.0f, 280.0f, 100.0f }, enc (0), false);
            break;
        case Page::MidiPage: menuText (g, "midi", "in and out on the computer's MIDI ports and the \"Keytape\" port; clock in follows another instrument, clock out leads it"); break;
        case Page::CtrlPage: menuText (g, "ctrl", "the keys and encoders send MIDI (notes, and CC 1-4 relative) instead of playing Keytape"); break;
        case Page::DiskPage: menuText (g, "disk", "the Keytape folder is open: tapes, mixdowns, snapshots, samples, and a radio folder for stations"); break;
        case Page::SettingsPage:
        {
            static const char* items[] = { "about", "screen", "tuning" };
            juce::StringArray l (items, 3);
            listColumn (g, l, d.params[0].visible ? juce::jlimit (0, 2, juce::roundToInt (d.params[0].value * 2.0f)) : 0, { 12.0f, 28.0f, 120.0f, 60.0f }, enc (0), true);
            text (g, "Keytape KT-4  research build", 150.0f, 30.0f, 160.0f, 12.0f, 9.0f, kDim);
            break;
        }
        default: break;
    }
    params (g, d);
    overlays (g, d);
    g.restoreState();
}

void DisplayView::paint (juce::Graphics& g)
{
    // the glass: black, with a faint sheen
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (r, 4.0f);
    const auto screen = r.reduced (r.getWidth() * 0.035f, r.getHeight() * 0.05f);
    paintDisplay (g, screen, state);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.5f, r.getBottom(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r, 4.0f);
}
} // namespace keytape::ui
