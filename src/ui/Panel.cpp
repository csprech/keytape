#include "Panel.h"

namespace keytape::ui
{
namespace m = materials;

namespace
{
    // the keyboard on the computer's letters, from F: a w s e d f t? (the usual piano rows)
    const char* kNoteKeys = "awsedrfgyhujkolp;['";
    int noteForChar (juce::juce_wchar c)
    {
        c = juce::CharacterFunctions::toLowerCase (c);
        for (int i = 0; kNoteKeys[i] != 0; ++i)
            if ((juce::juce_wchar) kNoteKeys[i] == c)
                return i;
        return -1;
    }

    /** The white key under each of the 14 columns, and the black keys' tiles (from, to in columns) with each disc on
        the boundary between two white keys. */
    const int kWhiteKeys[14] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23 };
    struct BlackTile
    {
        float from, to, disc;
        int key;
    };
    const BlackTile kBlackTiles[10] = { { 0.0f, 1.5f, 1.0f, 1 },  { 1.5f, 2.5f, 2.0f, 3 },  { 2.5f, 4.0f, 3.0f, 5 },   { 4.0f, 5.5f, 5.0f, 8 },   { 5.5f, 7.0f, 6.0f, 10 },
                                        { 7.0f, 8.5f, 8.0f, 13 }, { 8.5f, 9.5f, 9.0f, 15 }, { 9.5f, 11.0f, 10.0f, 17 }, { 11.0f, 12.5f, 12.0f, 20 }, { 12.5f, 14.0f, 13.0f, 22 } };
} // namespace

juce::Rectangle<int> Panel::cell (float col, float row, float cols, float rows)
{
    const float gap = 2.0f;
    return juce::Rectangle<float> (kX0 + col * kCell + gap, kY0 + row * kCell + gap, cols * kCell - gap * 2.0f, rows * kCell - gap * 2.0f).toNearestInt();
}

Panel::Panel (AudioEngine& e) : engine (e)
{
    setSize (kWidth, kHeight);
    setWantsKeyboardFocus (true);
    build();
    refreshNow();
    startTimerHz (30);
}

Panel::~Panel()
{
    stopTimer();
    allUp();
}

void Panel::build()
{
    auto addKey = [this] (Key k, juce::Rectangle<int> r, CellKey::Boss boss = CellKey::Boss::Round) {
        auto c = std::make_unique<CellKey> (k, boss);
        c->setBounds (r);
        c->onPress = [this, k] { press (k); };
        c->onRelease = [this, k] { release (k); };
        keys[k] = c.get();
        addAndMakeVisible (*c);
        parts.push_back (std::move (c));
    };
    // ---- top rows: (speaker), volume, blank, help, tempo, the screen, the encoders, input, com
    auto vol = std::make_unique<VolumeKnob>();
    vol->setBounds (cell (2, 0));
    vol->get = [this] { return engine.getVolume(); };
    vol->set = [this] (float v) { engine.setVolume (v); };
    volume = vol.get();
    addAndMakeVisible (*vol);
    parts.push_back (std::move (vol));
    addKey (Key::Blank, cell (3, 0), CellKey::Boss::Square);
    addKey (Key::Help, cell (2, 1));
    addKey (Key::Tempo, cell (3, 1));
    auto disp = std::make_unique<DisplayView>();
    disp->setBounds (cell (4, 0, 4, 2));
    display = disp.get();
    addAndMakeVisible (*disp);
    parts.push_back (std::move (disp));
    for (int i = 0; i < 4; ++i)
    {
        auto enc = std::make_unique<Encoder> (i);
        enc->setBounds (cell (8.0f + i * 2.0f, 0, 2, 2));
        enc->onTurn = [this, i] (int d) {
            engine.withMachine ([&] (Machine& mc) { mc.turn (i, d); });
            refreshNow();
        };
        enc->onTap = [this, i] {
            engine.withMachine ([&] (Machine& mc) { mc.tap (i); });
            refreshNow();
        };
        encoders[(size_t) i] = enc.get();
        addAndMakeVisible (*enc);
        parts.push_back (std::move (enc));
    }
    addKey (Key::Input, cell (16, 0));
    addKey (Key::Com, cell (16, 1));
    // ---- the row of keys
    const Key row[17] = { Key::Synth, Key::Drum, Key::Tape, Key::Mixer, Key::T1, Key::T2, Key::T3, Key::T4,
                          Key::K1, Key::K2, Key::K3, Key::K4, Key::K5, Key::K6, Key::K7, Key::K8, Key::Seq };
    for (int i = 0; i < 17; ++i)
        addKey (row[i], cell ((float) i, 2));
    // ---- the left block
    const Key block[3][3] = { { Key::Lift, Key::Drop, Key::Split }, { Key::Record, Key::Play, Key::Stop }, { Key::Left, Key::Right, Key::Shift } };
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            addKey (block[r][c], cell ((float) c, (float) (3 + r)));
    // ---- the keyboard
    auto addPiano = [this] (int k, bool black, juce::Rectangle<int> r, float discX) {
        auto p = std::make_unique<PianoKey> (k, black, discX);
        p->setBounds (r);
        p->onDown = [this, k] {
            engine.withMachine ([&] (Machine& mc) { mc.keyDown (k, 0.8f); });
            refreshNow();
        };
        p->onUp = [this, k] {
            engine.withMachine ([&] (Machine& mc) { mc.keyUp (k); });
            refreshNow();
        };
        piano[(size_t) k] = p.get();
        addAndMakeVisible (*p);
        parts.push_back (std::move (p));
    };
    for (int i = 0; i < 14; ++i)
        addPiano (kWhiteKeys[i], false, cell (3.0f + i, 4, 1, 2), 0.5f);
    for (const auto& t : kBlackTiles)
    {
        const auto r = cell (3.0f + t.from, 3, t.to - t.from, 1);
        const float discX = ((3.0f + t.disc) * kCell + kX0 - (float) r.getX()) / (float) r.getWidth();
        addPiano (t.key, true, r, discX);
    }
    // ---- the level meter on the right edge
    auto lm = std::make_unique<LevelMeter>();
    lm->setBounds (kX0 + 17 * kCell + 40, 214, 28, 112);
    meter = lm.get();
    addAndMakeVisible (*lm);
    parts.push_back (std::move (lm));
}

// ======================================================================== drawing

void Panel::paint (juce::Graphics& g)
{
    const float sc = m::scaleOf (g);
    if (background.isNull() || std::abs (sc - backgroundScale) > 0.01f)
    {
        backgroundScale = sc;
        background = juce::Image (juce::Image::ARGB, juce::roundToInt (kWidth * sc), juce::roundToInt (kHeight * sc), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (sc));
        paintBackground (bg);
    }
    m::drawAt (g, background, getLocalBounds().toFloat());
}

void Panel::paintBackground (juce::Graphics& g)
{
    const auto body = getLocalBounds().toFloat().reduced (1.0f);
    // the aluminium slab: bead-blasted, a little lighter at the top, a bright edge and a soft shadow under it
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 18, { 0, 8 }).drawForRectangle (g, body.toNearestInt());
    const auto tile = m::paintTile (colours::body, 256, 0.45f, 11);
    g.setTiledImageFill (tile, 0, 0, 1.0f);
    g.fillRoundedRectangle (body, 16.0f);
    juce::ColourGradient light (juce::Colours::white.withAlpha (0.16f), 0.0f, 0.0f, juce::Colours::black.withAlpha (0.10f), 0.0f, (float) kHeight, false);
    g.setGradientFill (light);
    g.fillRoundedRectangle (body, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (body.reduced (0.8f), 15.5f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (body, 16.0f, 1.0f);
    // the dark bed the cells sit in
    const auto face = juce::Rectangle<float> ((float) kX0 - 2.0f, (float) kY0 - 2.0f, 17.0f * kCell + 4.0f, 6.0f * kCell + 4.0f);
    g.setColour (colours::gap);
    g.fillRoundedRectangle (face, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (face.expanded (1.0f), 7.0f, 1.0f);
    // the speaker: a tile with a grille of round holes
    {
        const auto r = cell (0, 0, 2, 2);
        const auto tileImg = m::pillow (juce::roundToInt (r.getWidth() * backgroundScale), juce::roundToInt (r.getHeight() * backgroundScale), 5.0f * backgroundScale,
                                        2.4f * backgroundScale, 0.0f, colours::cap, m::plastic(), 77);
        m::drawAt (g, tileImg, r.toFloat());
        const auto c = r.toFloat().getCentre();
        const float pitch = 11.0f, half = 76.0f;
        for (float y = -half; y <= half + 0.1f; y += pitch)
            for (float x = -half; x <= half + 0.1f; x += pitch)
            {
                // a squircle of holes
                const float nx = x / (half + 4.0f), ny = y / (half + 4.0f);
                if (std::pow (std::abs (nx), 3.2f) + std::pow (std::abs (ny), 3.2f) > 1.0f)
                    continue;
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.fillEllipse (c.x + x - 2.6f, c.y + y - 2.0f, 5.2f, 5.2f);
                g.setColour (juce::Colour (0xff1d1d1d));
                g.fillEllipse (c.x + x - 2.6f, c.y + y - 2.6f, 5.2f, 5.2f);
            }
    }
    // the right edge: the microphone, the meter's cross, the name running up, the model badge
    const float rx = kX0 + 17.0f * kCell + 54.0f;
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillEllipse (rx - 3.5f, 72.0f, 7.0f, 7.0f);
    g.setColour (juce::Colour (0xff8a8c8f));
    g.fillRect (rx - 38.0f, 270.0f, 76.0f, 1.0f);
    g.fillRect (rx - 0.5f, 200.0f, 1.0f, 14.0f);
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, rx, 540.0f));
    g.setColour (juce::Colour (0xff4a4b4d));
    g.setFont (legendFont (40.0f));
    g.drawText ("KEYTAPE", juce::Rectangle<float> (rx - 120.0f, 540.0f - 24.0f, 240.0f, 48.0f), juce::Justification::centred, false);
    g.restoreState();
    const auto badge = juce::Rectangle<float> (60.0f, 22.0f).withCentre ({ rx, 612.0f });
    g.setColour (juce::Colour (0xff4a4b4d));
    g.drawRoundedRectangle (badge, 3.0f, 1.1f);
    g.setFont (legendFont (15.0f, false));
    g.drawText ("KT-4", badge, juce::Justification::centred, false);
}

// ======================================================================== the machine

void Panel::press (Key k)
{
    engine.withMachine ([k] (Machine& mc) { mc.press (k); });
    refreshNow();
}

void Panel::release (Key k)
{
    engine.withMachine ([k] (Machine& mc) { mc.release (k); });
    refreshNow();
}

void Panel::allUp()
{
    for (auto& [code, k] : heldNotes)
        engine.withMachine ([k] (Machine& mc) { mc.keyUp (k); });
    heldNotes.clear();
    for (auto& [code, k] : heldButtons)
        engine.withMachine ([k] (Machine& mc) { mc.release (k); });
    heldButtons.clear();
    if (shiftHeld)
    {
        shiftHeld = false;
        engine.withMachine ([] (Machine& mc) { mc.release (Key::Shift); });
    }
}

void Panel::refreshNow()
{
    std::array<bool, (size_t) Key::Count> down {};
    engine.withMachine ([&] (Machine& mc) {
        mc.describe (snap);
        for (int k = 0; k < (int) Key::Count; ++k)
            down[(size_t) k] = mc.isDown ((Key) k);
    });
    display->setState (snap);
    meter->setLevel (snap.meter, snap.showBattery);
    for (auto& [k, w] : keys)
        w->setShownDown (down[(size_t) k] && ! w->isMouseButtonDown());
    for (int i = 0; i < kKeys; ++i)
        if (piano[(size_t) i] != nullptr)
            piano[(size_t) i]->setShownDown (snap.keysDown[(size_t) i]);
    volume->repaint();
}

void Panel::timerCallback() { refreshNow(); }

// ======================================================================== the computer keyboard and the mouse

bool Panel::keyPressed (const juce::KeyPress& kp)
{
    const int code = kp.getKeyCode();
    const auto ch = kp.getTextCharacter();
    if (heldNotes.count (code) != 0 || heldButtons.count (code) != 0)
        return true; // (auto-repeat)
    const int note = noteForChar (ch != 0 ? ch : (juce::juce_wchar) code);
    if (note >= 0 && ! kp.getModifiers().isCommandDown())
    {
        heldNotes[code] = note;
        engine.withMachine ([note] (Machine& mc) { mc.keyDown (note, 0.8f); });
        refreshNow();
        return true;
    }
    Key k = Key::Count;
    if (code >= '1' && code <= '8')
        k = (Key) ((int) Key::K1 + (code - '1'));
    else if (code == juce::KeyPress::leftKey)
        k = Key::Left;
    else if (code == juce::KeyPress::rightKey)
        k = Key::Right;
    else if (code == juce::KeyPress::spaceKey)
        k = Key::Play;
    else if (code == juce::KeyPress::returnKey)
        k = Key::Record;
    else if (code == juce::KeyPress::escapeKey)
        k = Key::Stop;
    else if (ch == '/' || ch == '?')
        k = Key::Help;
    if (k == Key::Count || kp.getModifiers().isCommandDown())
        return false;
    heldButtons[code] = k;
    press (k);
    return true;
}

bool Panel::keyStateChanged (bool)
{
    bool any = false;
    for (auto it = heldNotes.begin(); it != heldNotes.end();)
        if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            const int k = it->second;
            engine.withMachine ([k] (Machine& mc) { mc.keyUp (k); });
            it = heldNotes.erase (it);
            any = true;
        }
        else
            ++it;
    for (auto it = heldButtons.begin(); it != heldButtons.end();)
        if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            const Key k = it->second;
            engine.withMachine ([k] (Machine& mc) { mc.release (k); });
            it = heldButtons.erase (it);
            any = true;
        }
        else
            ++it;
    if (any)
        refreshNow();
    return any;
}

void Panel::modifierKeysChanged (const juce::ModifierKeys& mods)
{
    if (mods.isShiftDown() != shiftHeld)
    {
        shiftHeld = mods.isShiftDown();
        shiftHeld ? press (Key::Shift) : release (Key::Shift);
    }
    if (! mods.isAltDown())
        engine.withMachine ([] (Machine& mc) { mc.setTilt (0.0f); });
}

void Panel::focusLost (FocusChangeType) { allUp(); }

void Panel::mouseMove (const juce::MouseEvent& e)
{
    // option + the mouse: the accelerometer. Left and right tilt; a quick back and forth shakes.
    if (! e.mods.isAltDown())
    {
        lastMouse = e.position;
        return;
    }
    const float tilt = juce::jlimit (-1.0f, 1.0f, (e.position.x - kWidth * 0.5f) / (kWidth * 0.4f));
    const float dx = e.position.x - lastMouse.x;
    lastMouse = e.position;
    const int dir = dx > 4.0f ? 1 : dx < -4.0f ? -1 : 0;
    const auto now = juce::Time::getMillisecondCounter();
    if (dir != 0 && dir != lastShakeDir)
    {
        shakeEnergy = (now - lastShake < 250) ? shakeEnergy + 1.0f : 1.0f;
        lastShake = now;
        lastShakeDir = dir;
    }
    const bool shook = shakeEnergy >= 4.0f;
    if (shook)
        shakeEnergy = 0.0f;
    engine.withMachine ([&] (Machine& mc) {
        mc.setTilt (tilt);
        if (shook)
            mc.shake();
    });
}

// ======================================================================== the self-test

std::pair<juce::StringArray, juce::StringArray> Panel::selfTestControls()
{
    juce::StringArray ok, bad;
    auto machine = [this] (auto fn) { return engine.withMachine (fn); };
    // every panel key reaches the machine (held while the widget is held)
    const auto slots = machine ([] (Machine& mc) { return std::make_pair (mc.sounds.synthSlot, mc.sounds.drumSlot); });
    int wired = 0;
    for (auto& [k, w] : keys)
    {
        if (k == Key::Blank)
            continue;
        w->onPress();
        const bool held = machine ([k = k] (Machine& mc) { return mc.isDown (k); });
        w->onRelease();
        const bool after = machine ([k = k] (Machine& mc) { return mc.isDown (k); });
        if (! held || after)
            bad.add (juce::String ("key ") + keyName (k) + " doesn't reach the machine");
        else
            ++wired;
        // (put the machine back where it was: a mode key, the sequencer etc. changed things)
        machine ([] (Machine& mc) {
            mc.tape.stop();
            mc.input.on = false;
            mc.press (Key::Synth);
            mc.release (Key::Synth);
            for (auto& s : mc.seqs)
                s.enabled = false;
        });
    }
    if (keys.size() != (size_t) Key::Count)
        bad.add ("the panel has " + juce::String ((int) keys.size()) + " keys, not " + juce::String ((int) Key::Count));
    machine ([slots] (Machine& mc) {
        mc.sounds.synthSlot = slots.first;
        mc.sounds.drumSlot = slots.second;
    });
    ok.add ("ok    " + juce::String (wired) + " keys reach the machine (and the blank one is blank)");
    // the mode keys change the screen
    const std::pair<Key, Page> modes[] = { { Key::Drum, Page::Engine }, { Key::Tape, Page::TapePage }, { Key::Mixer, Page::MixerPage }, { Key::Synth, Page::Engine } };
    for (auto& [k, p] : modes)
    {
        keys[k]->onPress();
        keys[k]->onRelease();
        refreshNow();
        if (snap.page != p)
            bad.add (juce::String ("the ") + keyName (k) + " key doesn't open its screen");
    }
    ok.add ("ok    mode keys open their screens");
    // the keyboard: every key is held in the machine while its widget is
    int keyboard = 0;
    for (int i = 0; i < kKeys; ++i)
    {
        piano[(size_t) i]->onDown();
        const bool held = snap.keysDown[(size_t) i];
        piano[(size_t) i]->onUp();
        if (! held || snap.keysDown[(size_t) i])
            bad.add ("keyboard key " + juce::String (i + 1) + " doesn't reach the machine");
        else
            ++keyboard;
    }
    ok.add ("ok    " + juce::String (keyboard) + " keyboard keys");
    // the encoders turn the four parameters, and a tap is a tap
    for (int e = 0; e < 4; ++e)
    {
        const float before = snap.params[(size_t) e].value;
        encoders[(size_t) e]->turnBy (before > 0.5f ? -10 : 10);
        refreshNow();
        if (std::abs (snap.params[(size_t) e].value - before) < 0.01f)
            bad.add ("encoder " + juce::String (e + 1) + " doesn't turn its parameter");
        encoders[(size_t) e]->turnBy (before > 0.5f ? 10 : -10);
    }
    keys[Key::Mixer]->onPress();
    keys[Key::Mixer]->onRelease();
    machine ([] (Machine& mc) { mc.mixer.mute[1] = false; });
    encoders[1]->onTap();
    if (! machine ([] (Machine& mc) { return mc.mixer.mute[1]; }))
        bad.add ("a tap on the ochre encoder doesn't mute track 2 in the mixer");
    encoders[1]->onTap();
    keys[Key::Synth]->onPress();
    keys[Key::Synth]->onRelease();
    ok.add ("ok    4 encoders turn, and tap");
    // the volume knob
    const float v = engine.getVolume();
    volume->set (0.33f);
    if (std::abs (engine.getVolume() - 0.33f) > 1.0e-4f)
        bad.add ("the volume knob doesn't set the volume");
    volume->set (v);
    ok.add ("ok    volume knob");
    refreshNow();
    return { ok, bad };
}
} // namespace keytape::ui
