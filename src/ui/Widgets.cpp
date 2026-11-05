#include "Widgets.h"

namespace keytape::ui
{
namespace m = materials;

namespace
{
    m::Shading capShading()
    {
        // moulded matt plastic: soft highlights, a fine grain
        m::Shading s;
        s.ambient = 0.42f;
        s.diffuse = 0.62f;
        s.specular = 0.07f;
        s.shininess = 14.0f;
        s.grain = 0.018f;
        s.bump = 0.06f;
        s.bumpScale = 0.9f;
        return s;
    }
    m::Shading glossBlack()
    {
        m::Shading s;
        s.ambient = 0.5f;
        s.diffuse = 0.5f;
        s.specular = 0.35f;
        s.shininess = 30.0f;
        s.grain = 0.02f;
        return s;
    }

    /** A tile with things on it, composed once per size and state at the screen's resolution. */
    juce::Image compose (const juce::String& key, int w, int h, float sc, const std::function<void (juce::Graphics&, float)>& draw)
    {
        return m::cached (key + "_" + juce::String (w) + "x" + juce::String (h) + "@" + juce::String (sc, 2), [=] {
            juce::Image img (juce::Image::ARGB, std::max (1, juce::roundToInt (w * sc)), std::max (1, juce::roundToInt (h * sc)), true);
            juce::Graphics g (img);
            draw (g, sc);
            return img;
        });
    }

    void drawTile (juce::Graphics& g, float sc, int w, int h, bool down)
    {
        const auto tile = m::cached ("kt_tile_" + juce::String (w) + "x" + juce::String (h) + "@" + juce::String (sc, 2), [=] {
            return m::pillow (juce::roundToInt (w * sc), juce::roundToInt (h * sc), 5.0f * sc, 2.4f * sc, 0.0f, colours::cap, capShading(), (uint32_t) (w * 31 + h));
        });
        g.setOpacity (1.0f);
        g.drawImageAt (tile, 0, 0);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.05f));
            g.fillRoundedRectangle (0.0f, 0.0f, w * sc, h * sc, 5.0f * sc);
        }
    }

    /** A boss (round, square or a pill) with its soft shadow on the tile. */
    void drawBoss (juce::Graphics& g, float sc, juce::Rectangle<float> r, float corner, juce::Colour colour, bool down, const m::Shading& shading, float bevelFrac = 0.16f)
    {
        const auto R = r * sc;
        juce::Path p;
        p.addRoundedRectangle (R, corner * sc);
        const float lift = down ? 1.4f : 3.0f;
        juce::DropShadow (juce::Colours::black.withAlpha (down ? 0.22f : 0.30f), juce::roundToInt ((down ? 3.0f : 6.0f) * sc), { juce::roundToInt (lift * 0.6f * sc), juce::roundToInt (lift * sc) }).drawForPath (g, p);
        const int bw = juce::roundToInt (R.getWidth()), bh = juce::roundToInt (R.getHeight());
        const float bevel = std::min (R.getWidth(), R.getHeight()) * bevelFrac;
        const auto img = m::cached ("kt_boss_" + juce::String (bw) + "x" + juce::String (bh) + "_" + juce::String (corner * sc, 1) + colour.toString() + juce::String (bevelFrac, 2), [=] {
            return m::pillow (bw, bh, corner * sc, bevel, bevel * 0.35f, colour, shading, (uint32_t) (bw * 7 + bh));
        });
        g.setOpacity (1.0f);
        g.drawImageAt (img, juce::roundToInt (R.getX()), juce::roundToInt (R.getY()));
    }

    void strokeLine (juce::Graphics& g, float x0, float y0, float x1, float y1, float w) { g.drawLine (x0, y0, x1, y1, w); }

    juce::Path arrowPath (juce::Point<float> from, juce::Point<float> to, float head)
    {
        juce::Path p;
        p.startNewSubPath (from);
        p.lineTo (to);
        const auto dir = (to - from) / std::max (0.001f, from.getDistanceFrom (to));
        const juce::Point<float> n (-dir.y, dir.x);
        p.startNewSubPath (to - dir * head + n * head * 0.8f);
        p.lineTo (to);
        p.lineTo (to - dir * head - n * head * 0.8f);
        return p;
    }
} // namespace

juce::Font legendFont (float height, bool light)
{
    return juce::Font (juce::FontOptions ("Avenir Next", height, juce::Font::plain).withStyle (light ? "Ultra Light" : "Regular"));
}

void drawLegend (juce::Graphics& g, Key k, juce::Rectangle<float> r)
{
    const float s = r.getWidth();
    const float lw = std::max (1.0f, s * 0.032f);
    const auto c = r.getCentre();
    g.setColour (colours::legend);
    const juce::PathStrokeType stroke (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    auto numeral = [&] (const juce::String& n, float size, float dy) {
        g.setFont (legendFont (s * size));
        g.drawText (n, juce::Rectangle<float> (s, s * size * 1.2f).withCentre (c.translated (0.0f, dy * s)), juce::Justification::centred, false);
    };
    auto subLabel = [&] (const juce::String& t) {
        g.setFont (legendFont (s * 0.2f, false));
        g.drawText (t, juce::Rectangle<float> (s, s * 0.24f).withCentre (c.translated (0.0f, s * 0.2f)), juce::Justification::centred, false);
    };
    switch (k)
    {
        case Key::Help:
        {
            // a speech bubble
            juce::Path p;
            p.addRoundedRectangle (c.x - s * 0.2f, c.y - s * 0.16f, s * 0.4f, s * 0.26f, s * 0.12f);
            p.startNewSubPath (c.x - s * 0.04f, c.y + s * 0.1f);
            p.lineTo (c.x - s * 0.1f, c.y + s * 0.2f);
            p.lineTo (c.x + s * 0.04f, c.y + s * 0.1f);
            g.strokePath (p, stroke);
            return;
        }
        case Key::Tempo:
        {
            // a metronome: its case and the arm swung over
            juce::Path p;
            p.startNewSubPath (c.x - s * 0.05f, c.y - s * 0.2f);
            p.lineTo (c.x + s * 0.05f, c.y - s * 0.2f);
            p.lineTo (c.x + s * 0.15f, c.y + s * 0.18f);
            p.lineTo (c.x - s * 0.15f, c.y + s * 0.18f);
            p.closeSubPath();
            p.startNewSubPath (c.x, c.y + s * 0.1f);
            p.lineTo (c.x + s * 0.16f, c.y - s * 0.16f);
            g.strokePath (p, stroke);
            return;
        }
        case Key::Synth:
        {
            // a sine, a little over two periods
            juce::Path p;
            for (int i = 0; i <= 40; ++i)
            {
                const float t = (float) i / 40.0f;
                const juce::Point<float> pt (c.x - s * 0.2f + t * s * 0.4f, c.y - std::sin (t * 4.4f * juce::MathConstants<float>::pi) * s * 0.08f);
                i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
            }
            g.strokePath (p, stroke);
            return;
        }
        case Key::Drum:
        {
            // a drum seen from the side, a stick resting on it
            juce::Path p;
            p.addEllipse (c.x - s * 0.16f, c.y - s * 0.08f, s * 0.32f, s * 0.1f);
            p.startNewSubPath (c.x - s * 0.16f, c.y - s * 0.03f);
            p.lineTo (c.x - s * 0.16f, c.y + s * 0.14f);
            p.startNewSubPath (c.x + s * 0.16f, c.y - s * 0.03f);
            p.lineTo (c.x + s * 0.16f, c.y + s * 0.14f);
            p.addCentredArc (c.x, c.y + s * 0.14f, s * 0.16f, s * 0.05f, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
            p.startNewSubPath (c.x + s * 0.02f, c.y - s * 0.06f);
            p.lineTo (c.x + s * 0.2f, c.y - s * 0.2f);
            g.strokePath (p, stroke);
            return;
        }
        case Key::Tape:
        {
            juce::Path p;
            p.addEllipse (c.x - s * 0.19f, c.y - s * 0.07f, s * 0.14f, s * 0.14f);
            p.addEllipse (c.x + s * 0.05f, c.y - s * 0.07f, s * 0.14f, s * 0.14f);
            p.startNewSubPath (c.x - s * 0.12f, c.y + s * 0.07f);
            p.lineTo (c.x + s * 0.12f, c.y + s * 0.07f);
            g.strokePath (p, stroke);
            return;
        }
        case Key::Mixer:
        {
            const float hs[4] = { 0.14f, 0.24f, 0.1f, 0.2f };
            for (int i = 0; i < 4; ++i)
            {
                const float x = c.x - s * 0.12f + (float) i * s * 0.08f;
                strokeLine (g, x, c.y + s * 0.12f, x, c.y + s * 0.12f - hs[i] * s, lw);
            }
            return;
        }
        case Key::T1: numeral ("1", 0.5f, 0.0f); return;
        case Key::T2: numeral ("2", 0.5f, 0.0f); return;
        case Key::T3: numeral ("3", 0.5f, 0.0f); return;
        case Key::T4: numeral ("4", 0.5f, 0.0f); return;
        case Key::K1: numeral ("1", 0.3f, -0.08f); subLabel ("IN"); return;
        case Key::K2: numeral ("2", 0.3f, -0.08f); subLabel ("OUT"); return;
        case Key::K3:
        {
            numeral ("3", 0.3f, -0.08f);
            // loop: a round arrow
            juce::Path p;
            p.addCentredArc (c.x, c.y + s * 0.2f, s * 0.09f, s * 0.05f, 0.0f, 0.4f, juce::MathConstants<float>::twoPi - 0.2f, true);
            g.strokePath (p, stroke);
            g.strokePath (arrowPath ({ c.x + s * 0.02f, c.y + s * 0.15f }, { c.x + s * 0.06f, c.y + s * 0.15f }, s * 0.03f), stroke);
            return;
        }
        case Key::K4:
        {
            numeral ("4", 0.3f, -0.08f);
            // break: the reels, winding down
            juce::Path p;
            p.addEllipse (c.x - s * 0.11f, c.y + s * 0.16f, s * 0.08f, s * 0.08f);
            p.addEllipse (c.x + s * 0.01f, c.y + s * 0.16f, s * 0.08f, s * 0.08f);
            p.addCentredArc (c.x + s * 0.11f, c.y + s * 0.2f, s * 0.05f, s * 0.05f, 0.0f, 0.3f, 2.4f, true);
            g.strokePath (p, stroke);
            return;
        }
        case Key::K5:
        {
            numeral ("5", 0.3f, -0.08f);
            // reverse: a letter R turned round
            g.saveState();
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f, c.x, c.y));
            subLabel ("R");
            g.restoreState();
            return;
        }
        case Key::K6:
        {
            numeral ("6", 0.3f, -0.08f);
            // chop: a row of slices
            for (int i = 0; i < 4; ++i)
                g.fillEllipse (c.x - s * 0.105f + (float) i * s * 0.07f - lw * 0.7f, c.y + s * 0.2f - lw * 0.7f, lw * 1.4f, lw * 1.4f);
            return;
        }
        case Key::K7: numeral ("7", 0.3f, -0.08f); subLabel ("M1"); return;
        case Key::K8: numeral ("8", 0.3f, -0.08f); subLabel ("M2"); return;
        case Key::Seq:
        {
            // a little staircase of steps
            const float pts[5][2] = { { -0.15f, 0.06f }, { -0.07f, -0.04f }, { 0.0f, 0.06f }, { 0.08f, -0.08f }, { 0.15f, 0.02f } };
            for (auto& pt : pts)
                g.fillEllipse (c.x + pt[0] * s - lw, c.y + pt[1] * s - lw, lw * 2.0f, lw * 2.0f);
            return;
        }
        case Key::Input:
        {
            // a microphone, in red
            g.setColour (colours::red);
            juce::Path p;
            p.addRoundedRectangle (c.x - s * 0.06f, c.y - s * 0.2f, s * 0.12f, s * 0.22f, s * 0.06f);
            p.addCentredArc (c.x, c.y - s * 0.02f, s * 0.11f, s * 0.11f, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
            p.startNewSubPath (c.x, c.y + s * 0.09f);
            p.lineTo (c.x, c.y + s * 0.18f);
            g.strokePath (p, juce::PathStrokeType (lw * 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }
        case Key::Com:
        {
            g.drawEllipse (c.x - s * 0.07f, c.y - s * 0.2f, s * 0.14f, s * 0.14f, lw);
            g.fillEllipse (c.x - lw * 1.1f, c.y - s * 0.13f - lw * 1.1f, lw * 2.2f, lw * 2.2f);
            g.setFont (legendFont (s * 0.2f, false));
            g.drawText ("COM", juce::Rectangle<float> (s, s * 0.24f).withCentre (c.translated (0.0f, s * 0.1f)), juce::Justification::centred, false);
            return;
        }
        case Key::Lift:
            g.strokePath (arrowPath ({ c.x, c.y + s * 0.1f }, { c.x, c.y - s * 0.16f }, s * 0.07f), stroke);
            strokeLine (g, c.x - s * 0.12f, c.y + s * 0.17f, c.x + s * 0.12f, c.y + s * 0.17f, lw);
            return;
        case Key::Drop:
            g.strokePath (arrowPath ({ c.x, c.y - s * 0.18f }, { c.x, c.y + s * 0.08f }, s * 0.07f), stroke);
            strokeLine (g, c.x - s * 0.12f, c.y + s * 0.17f, c.x + s * 0.12f, c.y + s * 0.17f, lw);
            return;
        case Key::Split:
        {
            // scissors
            juce::Path p;
            p.addEllipse (c.x - s * 0.17f, c.y - s * 0.12f, s * 0.09f, s * 0.09f);
            p.addEllipse (c.x - s * 0.17f, c.y + s * 0.03f, s * 0.09f, s * 0.09f);
            p.startNewSubPath (c.x - s * 0.09f, c.y - s * 0.05f);
            p.lineTo (c.x + s * 0.17f, c.y + s * 0.09f);
            p.startNewSubPath (c.x - s * 0.09f, c.y + s * 0.05f);
            p.lineTo (c.x + s * 0.17f, c.y - s * 0.09f);
            g.strokePath (p, stroke);
            return;
        }
        case Key::Record:
            g.setColour (colours::red);
            g.fillEllipse (juce::Rectangle<float> (s * 0.15f, s * 0.15f).withCentre (c));
            return;
        case Key::Play:
        {
            g.setColour (colours::legend.darker (0.6f));
            juce::Path p;
            p.addTriangle (c.x - s * 0.06f, c.y - s * 0.09f, c.x - s * 0.06f, c.y + s * 0.09f, c.x + s * 0.1f, c.y);
            g.fillPath (p);
            return;
        }
        case Key::Stop:
            g.setColour (colours::legend.darker (0.6f));
            g.fillRect (juce::Rectangle<float> (s * 0.14f, s * 0.14f).withCentre (c));
            return;
        case Key::Left: g.strokePath (arrowPath ({ c.x + s * 0.14f, c.y }, { c.x - s * 0.14f, c.y }, s * 0.07f), stroke); return;
        case Key::Right: g.strokePath (arrowPath ({ c.x - s * 0.14f, c.y }, { c.x + s * 0.14f, c.y }, s * 0.07f), stroke); return;
        case Key::Shift:
            g.setFont (legendFont (s * 0.24f, false));
            g.drawText ("shift", r, juce::Justification::centred, false);
            return;
        case Key::Blank:
        case Key::Count: return;
    }
}

// ======================================================================== keys

CellKey::CellKey (Key k, Boss b) : key (k), boss (b) { setRepaintsOnMouseActivity (false); }

void CellKey::setShownDown (bool d)
{
    if (d != shownDown)
    {
        shownDown = d;
        repaint();
    }
}

void CellKey::paint (juce::Graphics& g)
{
    const float sc = m::scaleOf (g);
    const int w = getWidth(), h = getHeight();
    const bool down = isShownDown();
    const bool round = boss == Boss::Round;
    const auto img = compose (round ? "kt_key_round" : "kt_key_square", w, h, sc * 1.0f, [=] (juce::Graphics& gi, float s) {
        drawTile (gi, s, w, h, false);
        const float d = w * (round ? 0.66f : 0.6f);
        drawBoss (gi, s, juce::Rectangle<float> (d, d).withCentre ({ w * 0.5f, h * 0.5f - 1.0f }), round ? d * 0.5f : d * 0.16f, colours::cap, false, capShading(), round ? 0.16f : 0.12f);
    });
    const auto imgDown = compose (round ? "kt_key_round_down" : "kt_key_square_down", w, h, sc, [=] (juce::Graphics& gi, float s) {
        drawTile (gi, s, w, h, true);
        const float d = w * (round ? 0.66f : 0.6f);
        drawBoss (gi, s, juce::Rectangle<float> (d, d).withCentre ({ w * 0.5f, h * 0.5f }), round ? d * 0.5f : d * 0.16f, colours::cap.darker (0.03f), true, capShading(), round ? 0.16f : 0.12f);
    });
    m::drawAt (g, down ? imgDown : img, getLocalBounds().toFloat());
    const float d = w * 0.66f;
    drawLegend (g, key, juce::Rectangle<float> (d, d).withCentre ({ w * 0.5f, h * 0.5f + (down ? 0.0f : -1.0f) }));
}

void CellKey::mouseDown (const juce::MouseEvent&)
{
    mouseHeld = true;
    repaint();
    if (onPress)
        onPress();
}

void CellKey::mouseUp (const juce::MouseEvent&)
{
    mouseHeld = false;
    repaint();
    if (onRelease)
        onRelease();
}

PianoKey::PianoKey (int i, bool b, float x) : index (i), black (b), discX (x) {}

void PianoKey::setShownDown (bool d)
{
    if (d != shownDown)
    {
        shownDown = d;
        repaint();
    }
}

void PianoKey::paint (juce::Graphics& g)
{
    const float sc = m::scaleOf (g);
    const int w = getWidth(), h = getHeight();
    const bool down = isShownDown();
    const float dx = discX;
    const bool isBlack = black;
    const auto img = compose (isBlack ? "kt_black_" + juce::String (dx, 3) + (down ? "d" : "u") : juce::String (down ? "kt_white_d" : "kt_white_u"), w, h, sc, [=] (juce::Graphics& gi, float s) {
        drawTile (gi, s, w, h, down);
        if (isBlack)
        {
            const float d = (float) h * 0.56f;
            drawBoss (gi, s, juce::Rectangle<float> (d, d).withCentre ({ w * dx, h * 0.5f + (down ? 0.6f : -0.6f) }), d * 0.5f, colours::blackKey, down, glossBlack(), 0.2f);
        }
        else
        {
            const float pw = (float) w * 0.74f, ph = (float) h * 0.88f;
            drawBoss (gi, s, juce::Rectangle<float> (pw, ph).withCentre ({ w * 0.5f, h * 0.5f + (down ? 0.6f : -1.5f) }), pw * 0.5f, down ? colours::cap.darker (0.03f) : colours::cap, down, capShading(), 0.11f);
        }
    });
    m::drawAt (g, img, getLocalBounds().toFloat());
}

void PianoKey::mouseDown (const juce::MouseEvent&)
{
    mouseHeld = true;
    repaint();
    if (onDown)
        onDown();
}

void PianoKey::mouseUp (const juce::MouseEvent&)
{
    mouseHeld = false;
    repaint();
    if (onUp)
        onUp();
}

// ======================================================================== encoders and the volume knob

Encoder::Encoder (int i) : index (i) {}

void Encoder::paint (juce::Graphics& g)
{
    const float sc = m::scaleOf (g);
    const int w = getWidth(), h = getHeight();
    const int e = index;
    const auto img = compose ("kt_encoder_" + juce::String (e), w, h, sc, [=] (juce::Graphics& gi, float s) {
        drawTile (gi, s, w, h, false);
        const juce::Point<float> c (w * 0.5f, h * 0.5f);
        // a fine groove around the ring
        gi.setColour (juce::Colour (0xffb9b5b0));
        const float gr = w * 0.275f;
        gi.drawEllipse (juce::Rectangle<float> (gr * 2.0f * s, gr * 2.0f * s).withCentre (c * s), 1.2f * s);
        // the broad ring
        const float rd = w * 0.5f;
        drawBoss (gi, s, juce::Rectangle<float> (rd, rd).withCentre (c), rd * 0.5f, colours::cap.brighter (0.02f), false, capShading(), 0.07f);
        // the coloured cap
        const float cd = w * 0.24f;
        drawBoss (gi, s, juce::Rectangle<float> (cd, cd).withCentre (c), cd * 0.5f, colours::encoderCap (e), false, e == 0 ? glossBlack() : capShading(), 0.22f);
    });
    m::drawAt (g, img, getLocalBounds().toFloat());
    // pressed (a tap): the cap goes in a little
    if (pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillEllipse (juce::Rectangle<float> (w * 0.24f, w * 0.24f).withCentre ({ w * 0.5f, h * 0.5f }));
    }
}

void Encoder::mouseDown (const juce::MouseEvent& e)
{
    last = e.position;
    travel = 0.0f;
    accum = 0.0f;
    pressed = true;
    repaint();
}

void Encoder::mouseDrag (const juce::MouseEvent& e)
{
    // up or right turns it clockwise; a click every 6 px
    const auto d = e.position - last;
    last = e.position;
    travel += std::abs (d.x) + std::abs (d.y);
    accum += (d.x - d.y) / 6.0f;
    const int clicks = (int) accum;
    if (clicks != 0)
    {
        accum -= (float) clicks;
        turnBy (clicks);
    }
}

void Encoder::mouseUp (const juce::MouseEvent&)
{
    pressed = false;
    repaint();
    if (travel < 4.0f && onTap)
        onTap();
}

void Encoder::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheelAccum += (w.deltaY + w.deltaX) * (w.isReversed ? -1.0f : 1.0f) * 12.0f;
    const int clicks = (int) wheelAccum;
    if (clicks != 0)
    {
        wheelAccum -= (float) clicks;
        turnBy (clicks);
    }
}

void Encoder::turnBy (int clicks)
{
    angle += (float) clicks * 0.15f;
    if (onTurn)
        onTurn (clicks);
}

VolumeKnob::VolumeKnob() = default;

void VolumeKnob::paint (juce::Graphics& g)
{
    const float sc = m::scaleOf (g);
    const int w = getWidth(), h = getHeight();
    const auto img = compose ("kt_volume", w, h, sc, [=] (juce::Graphics& gi, float s) {
        drawTile (gi, s, w, h, false);
        const float d = w * 0.58f;
        drawBoss (gi, s, juce::Rectangle<float> (d, d).withCentre ({ w * 0.5f, h * 0.5f }), d * 0.5f, colours::cap, false, capShading(), 0.24f);
    });
    m::drawAt (g, img, getLocalBounds().toFloat());
    // the pointer: a short engraved line from the edge in
    const float v = get ? get() : 0.5f;
    const float a = juce::degreesToRadians (-135.0f + 270.0f * v);
    const juce::Point<float> c (w * 0.5f, h * 0.5f);
    const float r0 = w * 0.13f, r1 = w * 0.26f;
    g.setColour (colours::legend.withAlpha (0.9f));
    g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, std::max (1.0f, w * 0.022f));
}

void VolumeKnob::mouseDown (const juce::MouseEvent&) { startValue = get ? get() : 0.5f; }

void VolumeKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (set)
        set (juce::jlimit (0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() / 160.0f + (float) e.getDistanceFromDragStartX() / 160.0f));
    repaint();
}

void VolumeKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (set && get)
        set (juce::jlimit (0.0f, 1.0f, get() + (w.deltaY + w.deltaX) * 0.25f));
    repaint();
}

void VolumeKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (set)
        set (0.8f);
    repaint();
}

// ======================================================================== the level meter

void LevelMeter::setLevel (float l, bool b)
{
    if (std::abs (l - level) > 0.002f || b != battery)
    {
        level = l;
        battery = b;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    // ten small LEDs, bottom up; a level in dB from -40 to 0 (the top one warns of clipping)
    const int n = 10;
    const float pitch = (float) getHeight() / (float) n;
    const float db = juce::Decibels::gainToDecibels (std::max (1.0e-5f, level));
    const int lit = battery ? n : juce::jlimit (0, n, (int) std::ceil ((db + 40.0f) / 40.0f * (float) n));
    for (int i = 0; i < n; ++i)
    {
        const auto r = juce::Rectangle<float> ((float) getWidth() * 0.5f, pitch * 0.34f).withCentre ({ getWidth() * 0.5f, getHeight() - pitch * (i + 0.5f) });
        const bool on = i < lit;
        if (on)
        {
            const auto c = (! battery && i == n - 1) ? colours::red : juce::Colours::white;
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (r.expanded (1.5f), 1.5f);
            g.setColour (c);
        }
        else
            g.setColour (juce::Colour (0xff8e9093));
        g.fillRoundedRectangle (r, 0.8f);
    }
}
} // namespace keytape::ui
