#include "Sequencers.h"

namespace keytape
{
namespace
{
    constexpr double kPpq = 96.0;
    // note values: 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
    const double kStepTicks[] = { 96.0, 48.0, 32.0, 24.0, 16.0, 12.0 };
    const char* kStepNames[] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    // trigger patterns: 8 steps, 1 = play, 0 = rest or skip
    const uint8_t kPatterns[][8] = { { 1, 1, 1, 1, 1, 1, 1, 1 }, { 1, 0, 1, 0, 1, 0, 1, 0 }, { 1, 1, 0, 1, 1, 0, 1, 0 }, { 1, 0, 0, 1, 0, 0, 1, 0 },
                                     { 1, 1, 1, 0, 1, 1, 1, 0 }, { 1, 0, 1, 1, 0, 1, 1, 0 }, { 1, 1, 0, 0, 1, 1, 0, 0 }, { 1, 0, 0, 0, 1, 0, 1, 0 } };
    constexpr int kNumPatterns = 8;
    const std::vector<int> kTypeAdds[] = { { 12 }, { 12, -12 }, { 7, 9, -5 }, { 2, 4, 5, 7, 9 } };
    int whiteIndex (int note)
    {
        // the white keys by their place on the keyboard (F to E, twice): 0..13, or -1 for a black key
        static const int whites[] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23 };
        const int offset = (((note - kLowestNote) % 24) + 24) % 24;
        for (int i = 0; i < 14; ++i)
            if (whites[i] == offset)
                return i;
        return -1;
    }
} // namespace

Sequencer::Sequencer()
{
    for (auto& p : hands)
        p.fill (-1);
    draw.fill (-1);
}

double Sequencer::ticksPerStep() const
{
    switch (type)
    {
        case Seq::Arp: return kStepTicks[juce::jlimit (0, 5, arpNoteValue)];
        case Seq::Chain: return kStepTicks[juce::jlimit (0, 5, chainNoteValue)];
        case Seq::Draw: return 24.0 / std::pow (2.0, drawDivider - 2); // the divider: /4 .. x16
        default: return 24.0; // sixteenths
    }
}

bool Sequencer::running() const
{
    switch (type)
    {
        case Seq::Arp: return ! held.empty();
        case Seq::Chain: return ! chain.empty() && (! keysDown.empty() || chainHold) && ! shiftEntry;
        case Seq::Hands: return ! handsPlaying.empty();
        case Seq::Hold: return false;
        case Seq::Grid: return (! keysDown.empty() && ! shiftEntry) || gridLive;
        case Seq::Draw: return ! keysDown.empty() || drawHold;
        case Seq::Spin: return ! balls.empty();
        case Seq::Count: break;
    }
    return false;
}

void Sequencer::noteOn (int offset, int note, float vel, std::vector<NoteEvent>& out)
{
    note = juce::jlimit (0, 127, note);
    out.push_back ({ offset, note, vel, true });
    const double len = type == Seq::Arp ? (double) arpLength * 2.0 : ticksPerStep() * 0.85;
    sounding.push_back ({ note, len });
}

void Sequencer::allOff (int offset, std::vector<NoteEvent>& out)
{
    for (auto& s : sounding)
        out.push_back ({ offset, s.first, 0.0f, false });
    sounding.clear();
}

void Sequencer::stopAll (std::vector<NoteEvent>& out)
{
    allOff (0, out);
    for (int d : drones)
        out.push_back ({ 0, d, 0.0f, false });
    drones.clear();
}

// ======================================================================== the keyboard

bool Sequencer::keyDown (int note, bool shift)
{
    if (! enabled)
        return false;
    const int rel = note - 60;
    switch (type)
    {
        case Seq::Arp:
            if (arpHold && keysDown.empty())
                held.clear(); // (a new chord replaces the held one)
            keysDown.push_back (note);
            if (std::find (held.begin(), held.end(), note) == held.end())
                held.push_back (note);
            std::sort (held.begin(), held.end());
            return true;
        case Seq::Chain:
            if (shift)
            {
                // step entry: the note is stored, and the step moves on when it's released
                shiftEntry = true;
                if ((int) chain.size() < 128)
                    chain.push_back (note);
                keysDown.push_back (note);
                return true;
            }
            transpose = rel;
            keysDown.push_back (note);
            if (keysDown.size() == 1)
            {
                chainPos = 0;
                tickPos = 0.0;
                stepFired = false;
            }
            return true;
        case Seq::Hands:
        {
            const int w = whiteIndex (note);
            if (shift)
            {
                // enter a note into the pattern being edited, at the cursor
                hands[(size_t) handsKey][(size_t) handsCursor] = note;
                handsCursor = (handsCursor + 1) % handsLength;
                return true;
            }
            if (w < 0)
                return false;
            handsKey = w;
            if (handsPlayMode == 1) // replace: the new key takes over
                handsPlaying.clear();
            if (std::find (handsPlaying.begin(), handsPlaying.end(), w) == handsPlaying.end())
                handsPlaying.push_back (w);
            if (handsPlaying.size() == 1)
                tickPos = 0.0;
                stepFired = false;
            return true;
        }
        case Seq::Hold:
            if (note < holdBreak)
            {
                // the drone side of the split: a key latches its note (again: releases it)
                const int d = note + holdTranspose;
                auto it = std::find (drones.begin(), drones.end(), d);
                if (it != drones.end())
                    drones.erase (it);
                else
                {
                    if (! holdPoly)
                        drones.clear();
                    drones.push_back (d);
                }
                return true;
            }
            return false; // (the other side plays normally)
        case Seq::Grid:
            if (shift)
            {
                auto& s = grid[(size_t) gridCursor];
                if (std::find (s.begin(), s.end(), note) == s.end())
                    s.push_back (note);
                if (! gridLive)
                    gridCursor = (gridCursor + 1) % gridLength;
                shiftEntry = true;
                return true;
            }
            transpose = rel;
            keysDown.push_back (note);
            if (keysDown.size() == 1 && ! gridLive)
            {
                step = 0;
                tickPos = 0.0;
                stepFired = false;
            }
            return true;
        case Seq::Draw:
            transpose = rel;
            drawY = juce::jlimit (0, 23, note - kLowestNote);
            keysDown.push_back (note);
            if (keysDown.size() == 1 && ! drawHold)
            {
                step = 0;
                tickPos = 0.0;
                stepFired = false;
            }
            return true;
        case Seq::Spin:
        {
            // drop a ball into the drum
            Ball b;
            b.note = note;
            b.x = 0.0f;
            b.y = -0.2f;
            b.vx = 0.15f * (float) ((note % 5) - 2);
            b.vy = 0.0f;
            if (balls.size() < 16)
                balls.push_back (b);
            return true;
        }
        case Seq::Count: break;
    }
    return false;
}

bool Sequencer::keyUp (int note, bool shift)
{
    juce::ignoreUnused (shift);
    if (! enabled)
        return false;
    keysDown.erase (std::remove (keysDown.begin(), keysDown.end(), note), keysDown.end());
    switch (type)
    {
        case Seq::Arp:
            if (! arpHold)
                held.erase (std::remove (held.begin(), held.end(), note), held.end());
            return true;
        case Seq::Chain:
            if (shiftEntry && keysDown.empty())
                shiftEntry = false;
            return true;
        case Seq::Hands:
        {
            const int w = whiteIndex (note);
            if (! handsHold)
                handsPlaying.erase (std::remove (handsPlaying.begin(), handsPlaying.end(), w), handsPlaying.end());
            return w >= 0;
        }
        case Seq::Hold: return note < holdBreak;
        case Seq::Grid:
            if (keysDown.empty())
                shiftEntry = false;
            return true;
        case Seq::Draw:
        case Seq::Spin: return true;
        case Seq::Count: break;
    }
    return false;
}

bool Sequencer::arrow (int dir, bool shift)
{
    if (! enabled || ! shift)
        return false;
    if (type == Seq::Chain)
    {
        if (dir > 0)
        {
            // with a key held: a long note (the held note carries on); otherwise a rest
            if ((int) chain.size() < 128)
                chain.push_back (! keysDown.empty() ? -2 : -1);
        }
        else if (! chain.empty())
            chain.pop_back();
        return true;
    }
    if (type == Seq::Grid)
    {
        gridCursor = (gridCursor + dir + gridLength) % gridLength;
        return true;
    }
    return false;
}

void Sequencer::shake()
{
    if (type == Seq::Draw)
        draw.fill (-1);
}

// ======================================================================== the encoders

void Sequencer::turn (int encoder, int delta, bool shift)
{
    auto stepInt = [delta] (int& v, int lo, int hi) { v = juce::jlimit (lo, hi, v + delta); };
    auto stepF = [delta] (float& v, float k = 0.02f) { v = juce::jlimit (0.0f, 1.0f, v + (float) delta * k); };
    switch (type)
    {
        case Seq::Arp:
            if (! shift)
            {
                if (encoder == 0) stepInt (arpNoteValue, 0, 5);
                if (encoder == 1) stepInt (arpTriggerMode, 0, 4);
                if (encoder == 2) stepInt (arpPattern, 0, kNumPatterns - 1);
                if (encoder == 3) arpHold = delta > 0;
            }
            else
            {
                if (encoder == 0) stepInt (arpLength, 1, 64);
                if (encoder == 1) stepInt (arpType, 0, 3);
                if (encoder == 2) arpSkip = delta > 0;
                if (encoder == 3) stepF (swing);
            }
            if (! arpHold && keysDown.empty())
                held.clear();
            break;
        case Seq::Chain:
            if (encoder == 0) shift ? (void) (chainManual = delta > 0) : stepInt (chainNoteValue, 0, 5);
            if (encoder == 0 && chainManual && shift) crank = 0.0;
            if (encoder == 1) stepF (swing);
            if (encoder == 2)
            {
                if (shift && ! chain.empty())
                    std::rotate (chain.begin(), chain.begin() + ((delta % (int) chain.size()) + (int) chain.size()) % (int) chain.size(), chain.end());
                else
                    stepInt (chainPattern, 0, kNumPatterns - 1);
            }
            if (encoder == 3) shift ? stepInt (chainDirection, 0, 2) : (void) (chainHold = delta > 0);
            // in manual mode, the blue encoder cranks the sequence along
            break;
        case Seq::Hands:
            if (encoder == 0)
            {
                handsCursor = (handsCursor + delta + handsLength * 4) % handsLength;
                if (shift)
                    hands[(size_t) handsKey][(size_t) handsCursor] = -1; // erase while moving
            }
            if (encoder == 1) stepF (swing);
            if (encoder == 2) stepInt (handsLength, 1, 16);
            if (encoder == 3) shift ? stepInt (handsPlayMode, 0, 2) : (void) (handsHold = delta > 0);
            break;
        case Seq::Hold:
            if (encoder == 0) stepInt (holdBreak, kLowestNote, kLowestNote + kKeys);
            if (encoder == 1) holdPoly = delta > 0;
            if (encoder == 2) stepInt (holdTranspose, -24, 24);
            if (encoder == 3) holdLatch = delta > 0;
            break;
        case Seq::Grid:
            if (encoder == 0)
            {
                gridCursor = (gridCursor + delta + gridLength * 4) % gridLength;
                if (shift)
                    grid[(size_t) gridCursor].clear();
            }
            if (encoder == 1)
            {
                if (shift)
                    std::rotate (grid.begin(), grid.begin() + ((delta % 16) + 16) % 16, grid.end());
                else
                    stepF (swing);
            }
            if (encoder == 2) shift ? stepInt (gridOffset, 0, 15) : stepInt (gridLength, 1, 16);
            if (encoder == 3) shift ? stepInt (gridDirection, 0, 2) : (void) (gridLive = delta > 0);
            break;
        case Seq::Draw:
            if (! shift)
            {
                // blue and ochre draw (x and y); gray and orange move the pen without drawing
                if (encoder == 0 || encoder == 2)
                {
                    const int dir = delta > 0 ? 1 : -1;
                    for (int k = 0; k < std::abs (delta); ++k)
                    {
                        drawX = juce::jlimit (0, 63, drawX + dir);
                        if (encoder == 0)
                            draw[(size_t) drawX] = drawY;
                    }
                }
                if (encoder == 1 || encoder == 3)
                {
                    drawY = juce::jlimit (0, 23, drawY + delta);
                    if (encoder == 1)
                        draw[(size_t) drawX] = drawY;
                }
            }
            else
            {
                if (encoder == 0 && delta < 0)
                    for (int k = 0; k < -delta; ++k)
                    {
                        draw[(size_t) drawX] = -1; // trace back, erasing
                        drawX = std::max (0, drawX - 1);
                    }
                if (encoder == 1) stepInt (drawDivider, 0, 6);
                if (encoder == 2) drawGrid = delta > 0;
                if (encoder == 3) drawHold = delta > 0;
            }
            break;
        case Seq::Spin:
            if (encoder == 0)
            {
                if (shift)
                    spinManual = delta > 0 ? true : spinManual && delta == 0;
                if (spinManual && ! shift)
                    spinAngle += (float) delta * 0.08f; // turned by hand
                else if (! shift)
                    stepF (spinSpeed);
            }
            if (encoder == 1) stepF (spinGravity);
            if (encoder == 2) stepF (spinShape);
            if (encoder == 3) stepF (spinBounce);
            break;
        case Seq::Count: break;
    }
}

void Sequencer::tap (int encoder, bool shift)
{
    juce::ignoreUnused (shift);
    if (type == Seq::Hold && encoder == 0)
        drones.clear(); // clear
    if (type == Seq::Grid && encoder == 0)
        for (auto& s : grid)
            s.clear();
    if (type == Seq::Hands && encoder == 0)
        hands[(size_t) handsKey].fill (-1);
    if (type == Seq::Spin && encoder == 0)
        balls.clear();
}

// ======================================================================== running

void Sequencer::emitStep (int offset, std::vector<NoteEvent>& out)
{
    const auto& pattern = kPatterns[juce::jlimit (0, kNumPatterns - 1, type == Seq::Arp ? arpPattern : chainPattern)];
    switch (type)
    {
        case Seq::Arp:
        {
            if (held.empty())
                return;
            // the chord plus the type's additions, played up
            std::vector<int> notes = held;
            if (arpTriggerMode != 4)
            {
                const auto& adds = kTypeAdds[juce::jlimit (0, 3, arpType)];
                const size_t base = notes.size();
                if (arpTriggerMode == 1 && base > 0) // once: added to the last note
                    for (int a : adds)
                        notes.push_back (held.back() + a);
                else if (arpTriggerMode >= 2)        // each / all
                    for (size_t i = 0; i < base; ++i)
                        for (int a : adds)
                            notes.push_back (held[i] + a);
            }
            // the trigger pattern: a dash pauses (a rest) or is skipped (the next dot plays now)
            if (pattern[patPos % 8] == 0)
            {
                if (! arpSkip)
                {
                    ++patPos;
                    return;
                }
                for (int k = 0; k < 8 && pattern[patPos % 8] == 0; ++k)
                    ++patPos;
            }
            ++patPos;
            if (arpTriggerMode == 4) // trig: the first note locked to the beat, the rest on the pattern
                noteOn (offset, step % 4 == 0 ? notes.front() : notes[(size_t) (arpIndex++ % (int) notes.size())], 0.8f, out);
            else
                noteOn (offset, notes[(size_t) (arpIndex++ % (int) notes.size())], 0.8f, out);
            return;
        }
        case Seq::Chain:
        {
            if (chain.empty())
                return;
            const int n = (int) chain.size();
            int idx = chainPos;
            if (chainDirection == 1)
                idx = n - 1 - chainPos;
            if (chainDirection == 2)
                idx = juce::Random::getSystemRandom().nextInt (n);
            chainPos = (chainPos + 1) % n;
            const int v = chain[(size_t) idx];
            if (! pattern[step % 8])
                return;
            if (v == -2)
            {
                for (auto& s : sounding)
                    s.second += ticksPerStep(); // the note held on
                return;
            }
            if (v >= 0)
                noteOn (offset, v + transpose, 0.8f, out);
            return;
        }
        case Seq::Hands:
            for (int w : handsPlaying)
            {
                const int v = hands[(size_t) w][(size_t) (step % handsLength)];
                if (v >= 0)
                    noteOn (offset, v, 0.8f, out);
            }
            return;
        case Seq::Grid:
        {
            const int len = gridLength;
            int s = step % len;
            if (gridDirection == 1)
                s = len - 1 - s;
            if (gridDirection == 2)
            {
                const int cycle = std::max (1, 2 * len - 2);
                const int k = step % cycle;
                s = k < len ? k : cycle - k;
            }
            s = (s + gridOffset) % 16;
            if (gridLive)
                gridCursor = s;
            for (int v : grid[(size_t) s])
                noteOn (offset, v + (drum ? 0 : transpose), 0.8f, out);
            return;
        }
        case Seq::Draw:
        {
            const int x = step % 64;
            const int v = draw[(size_t) x];
            const int prev = draw[(size_t) ((x + 63) % 64)];
            if (v >= 0 && (v != prev || sounding.empty()))
                noteOn (offset, kLowestNote + v + (drum ? 0 : transpose), 0.8f, out);
            return;
        }
        default: return;
    }
}

void Sequencer::process (int n, double sampleRate, double bpm, std::vector<NoteEvent>& out)
{
    if (! enabled)
        return;
    const double ticksPerSample = bpm / 60.0 * kPpq / sampleRate;
    // HOLD: the drones sound continuously
    if (type == Seq::Hold)
    {
        for (int d : drones)
            if (std::none_of (sounding.begin(), sounding.end(), [d] (auto& s) { return s.first == d; }))
            {
                out.push_back ({ 0, d, 0.7f, true });
                sounding.push_back ({ d, 1.0e12 });
            }
        for (auto it = sounding.begin(); it != sounding.end();)
            if (std::find (drones.begin(), drones.end(), it->first) == drones.end())
            {
                out.push_back ({ 0, it->first, 0.0f, false });
                it = sounding.erase (it);
            }
            else
                ++it;
        return;
    }
    // SPIN: the balls fall and bounce in a turning polygon; a bounce plays its note, harder is louder
    if (type == Seq::Spin)
    {
        const float dt = (float) n / (float) sampleRate;
        const int sides = 3 + (int) (spinShape * 5.0f);
        const bool open = spinShape > 0.97f;
        if (! spinManual)
            spinAngle += (spinSpeed - 0.5f) * 4.0f * dt;
        const float g = 0.5f + spinGravity * 3.0f;
        for (auto it = balls.begin(); it != balls.end();)
        {
            auto& b = *it;
            b.vy += g * dt;
            b.x += b.vx * dt;
            b.y += b.vy * dt;
            // the wall nearest the ball: a polygon of radius 1, turned by spinAngle
            const float a = std::atan2 (b.y, b.x) - spinAngle;
            const float seg = juce::MathConstants<float>::twoPi / (float) sides;
            const float local = a - seg * std::round (a / seg);
            const float wall = std::cos (juce::MathConstants<float>::pi / (float) sides) / std::cos (local);
            const float r = std::hypot (b.x, b.y);
            if (r > wall)
            {
                if (open && std::abs (local) < seg * 0.25f && b.y > 0.0f)
                {
                    it = balls.erase (it); // out through the opening
                    continue;
                }
                const float nx = b.x / r, ny = b.y / r;
                const float vn = b.vx * nx + b.vy * ny;
                if (vn > 0.0f)
                {
                    const float speed = std::abs (vn);
                    b.vx -= (1.0f + spinBounce) * vn * nx;
                    b.vy -= (1.0f + spinBounce) * vn * ny;
                    // the wall moves: it carries the ball round
                    b.vx += -ny * (spinSpeed - 0.5f) * 0.6f;
                    b.vy += nx * (spinSpeed - 0.5f) * 0.6f;
                    if (speed > 0.05f)
                        noteOn (0, b.note, juce::jlimit (0.1f, 1.0f, speed * 0.6f), out);
                }
                b.x = nx * wall * 0.999f;
                b.y = ny * wall * 0.999f;
            }
            ++it;
        }
    }
    // the sounding notes run out
    for (auto it = sounding.begin(); it != sounding.end();)
    {
        it->second -= ticksPerSample * n;
        if (it->second <= 0.0)
        {
            out.push_back ({ n - 1, it->first, 0.0f, false });
            it = sounding.erase (it);
        }
        else
            ++it;
    }
    if (type == Seq::Spin)
        return;
    if (! running())
    {
        if (! sounding.empty() && type != Seq::Arp)
            allOff (0, out);
        tickPos = 0.0;
        step = 0;
        stepFired = false;
        return;
    }
    if (type == Seq::Chain && chainManual)
        return; // (cranked by hand)
    // steps: with swing, every other step is late
    const double stepTicks = ticksPerStep();
    double t = tickPos;
    for (int i = 0; i < n; ++i)
    {
        const double swingOffset = (step % 2 == 1) ? (swing - 0.5) * stepTicks : 0.0;
        if (! stepFired && t >= swingOffset)
        {
            emitStep (i, out);
            stepFired = true;
        }
        t += ticksPerSample;
        if (t >= stepTicks)
        {
            t -= stepTicks;
            ++step;
            stepFired = false;
        }
    }
    tickPos = t;
}

// ======================================================================== on the screen

juce::String Sequencer::paramName (int i) const
{
    static const char* names[][8] = {
        { "note value", "trigger mode", "pattern", "hold", "note length", "type", "pause / skip", "swing" },   // arp
        { "note value", "swing", "pattern", "hold", "manual", "fine", "rotate", "direction" },                 // chain
        { "cursor", "swing", "length", "hold", "erase", "fine", "fine", "play mode" },                         // hands
        { "break point", "mono / poly", "transpose", "hold", "clear", "fine", "fine", "fine" },                // hold
        { "cursor", "swing", "length", "live", "erase", "rotate", "move", "direction" },                       // grid
        { "draw x", "draw y", "move x", "move y", "erase", "divider", "grid", "hold" },                        // draw
        { "speed", "heaviness", "shape", "bounciness", "manual", "fine", "fine", "fine" },                     // spin
    };
    return names[juce::jlimit (0, (int) Seq::Count - 1, (int) type)][juce::jlimit (0, 7, i)];
}

float Sequencer::paramValue (int i) const
{
    switch (type)
    {
        case Seq::Arp: return i == 0 ? arpNoteValue / 5.0f : i == 1 ? arpTriggerMode / 4.0f : i == 2 ? arpPattern / 7.0f : arpHold ? 1.0f : 0.0f;
        case Seq::Chain: return i == 0 ? chainNoteValue / 5.0f : i == 1 ? swing : i == 2 ? chainPattern / 7.0f : chainHold ? 1.0f : 0.0f;
        case Seq::Hands: return i == 0 ? handsCursor / 15.0f : i == 1 ? swing : i == 2 ? (handsLength - 1) / 15.0f : handsHold ? 1.0f : 0.0f;
        case Seq::Hold: return i == 0 ? (holdBreak - kLowestNote) / (float) kKeys : i == 1 ? (holdPoly ? 1.0f : 0.0f) : i == 2 ? (holdTranspose + 24) / 48.0f : holdLatch ? 1.0f : 0.0f;
        case Seq::Grid: return i == 0 ? gridCursor / 15.0f : i == 1 ? swing : i == 2 ? (gridLength - 1) / 15.0f : gridLive ? 1.0f : 0.0f;
        case Seq::Draw: return i == 0 ? drawX / 63.0f : i == 1 ? drawY / 23.0f : i == 2 ? drawX / 63.0f : drawY / 23.0f;
        case Seq::Spin: return i == 0 ? spinSpeed : i == 1 ? spinGravity : i == 2 ? spinShape : spinBounce;
        case Seq::Count: break;
    }
    return 0.0f;
}

juce::String Sequencer::paramText (int i) const
{
    switch (type)
    {
        case Seq::Arp:
        {
            static const char* modes[] = { "none", "once", "each", "all", "trig" };
            return i == 0 ? juce::String (kStepNames[arpNoteValue]) : i == 1 ? juce::String (modes[arpTriggerMode]) : i == 2 ? "p" + juce::String (arpPattern + 1)
                                                                                                                                 : juce::String (arpHold ? "hold" : "off");
        }
        case Seq::Chain: return i == 0 ? juce::String (kStepNames[chainNoteValue]) : i == 1 ? juce::String (juce::roundToInt (swing * 100)) + "%" : i == 2 ? "p" + juce::String (chainPattern + 1) : juce::String (chainHold ? "hold" : "off");
        case Seq::Hands: return i == 0 ? juce::String (handsCursor + 1) : i == 1 ? juce::String (juce::roundToInt (swing * 100)) + "%" : i == 2 ? juce::String (handsLength) : juce::String (handsHold ? "hold" : "off");
        case Seq::Hold: return i == 0 ? juce::String (noteName (holdBreak)) : i == 1 ? juce::String (holdPoly ? "poly" : "mono") : i == 2 ? juce::String (holdTranspose) : juce::String (holdLatch ? "hold" : "off");
        case Seq::Grid: return i == 0 ? juce::String (gridCursor + 1) : i == 1 ? juce::String (juce::roundToInt (swing * 100)) + "%" : i == 2 ? juce::String (gridLength) : juce::String (gridLive ? "live" : "off");
        case Seq::Draw: return i < 2 ? juce::String (i == 0 ? drawX : drawY) : juce::String (i == 2 ? drawX : drawY);
        case Seq::Spin: return juce::String (juce::roundToInt (paramValue (i) * 100));
        case Seq::Count: break;
    }
    return {};
}
} // namespace keytape
