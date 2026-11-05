#pragma once

// Keytape's top face on a fixed 1836 x 656 canvas: an aluminium slab with a grid of off-white cells, 17 across and
// 6 down (100 px a cell), laid out on the original's photos. Its owner scales it to the window.
//
// The panel reads the machine 30 times a second (a description under the engine's lock) and tells it when a key,
// the keyboard or an encoder moves. The computer keyboard plays too: the letters A to ' are the keys from F,
// shift is shift, 1-8 the sound keys, the arrows the arrow keys, space play, return record, escape stop, / help.
// With option held, moving the mouse tilts the unit (g-force); shaking it shakes it.

#include "../engine/Engine.h"
#include "Display.h"
#include "Widgets.h"

namespace keytape::ui
{
class Panel : public juce::Component, private juce::Timer
{
    friend struct GpuiAdapter; // (the GPUI front end drives this panel off-screen: bridge/KeytapeAdapter.cpp)

public:
    /** Whether a key is held (the GPUI front end answers this; otherwise the computer's keyboard). */
    static inline std::function<bool (int)> keyHook;
    static bool keyIsDown (int c) { return keyHook ? keyHook (c) : juce::KeyPress::isKeyCurrentlyDown (c); }

    static constexpr int kWidth = 1836, kHeight = 656, kCell = 100, kX0 = 28, kY0 = 28;
    explicit Panel (AudioEngine& engine);
    ~Panel() override;

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void focusLost (FocusChangeType) override;
    /** Reads the machine now (the timer does it 30 times a second). */
    void refreshNow();
    const DisplayState& getSnapshot() const { return snap; }
    /** The self-test: works every key, the keyboard, the encoders and the volume through their widgets and checks the
        machine follows (passes, failures). */
    std::pair<juce::StringArray, juce::StringArray> selfTestControls();
    /** A cell's rectangle on the canvas (col, row from the top left; spans in cells). */
    static juce::Rectangle<int> cell (float col, float row, float cols = 1.0f, float rows = 1.0f);

private:
    void timerCallback() override;
    void build();
    void paintBackground (juce::Graphics& g);
    void press (Key k);
    void release (Key k);
    void allUp();

    AudioEngine& engine;
    DisplayState snap;
    juce::Image background;
    float backgroundScale = 0.0f;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::map<Key, CellKey*> keys;
    std::array<PianoKey*, kKeys> piano {};
    std::array<Encoder*, 4> encoders {};
    VolumeKnob* volume = nullptr;
    DisplayView* display = nullptr;
    LevelMeter* meter = nullptr;
    // the computer keyboard
    std::map<int, int> heldNotes;   // key code -> keyboard key
    std::map<int, Key> heldButtons; // key code -> panel key
    bool shiftHeld = false;
    // g-force from the mouse
    juce::Point<float> lastMouse;
    float shakeEnergy = 0.0f;
    int lastShakeDir = 0;
    juce::uint32 lastShake = 0;
};
} // namespace keytape::ui
