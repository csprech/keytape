#pragma once

// The panel's parts: the keys (a square cell with a round boss, or a square boss), the keyboard's tall white keys
// and round black keys, the four encoders, the volume knob, the level meter. Each is drawn from the materials
// renderer (cached, at the screen's resolution) and moves when it's pressed.

#include "../core/Machine.h"
#include "Materials.h"

namespace keytape::ui
{
namespace colours
{
    const juce::Colour cap { 0xffe8e5e0 };      // the off-white keycaps
    const juce::Colour legend { 0xff76736f };   // gray line legends
    const juce::Colour red { 0xffff5427 };      // record and input
    const juce::Colour gap { 0xff121212 };      // between the cells
    const juce::Colour body { 0xffc8cacc };     // anodised aluminium
    const juce::Colour blackKey { 0xff17171a };
    // the encoder caps, and the same colours as the screen shows them
    const juce::Colour capBlue { 0xff1e2852 }, capOchre { 0xffc49e6c }, capGray { 0xff9c9ea0 }, capOrange { 0xffff5a2c };
    const juce::Colour screenBlue { 0xff4f74ff }, screenOchre { 0xffdcb47a }, screenGray { 0xffb4b6b8 }, screenOrange { 0xffff5f2e };
    inline juce::Colour encoderCap (int e) { return e == 0 ? capBlue : e == 1 ? capOchre : e == 2 ? capGray : capOrange; }
    inline juce::Colour onScreen (int e) { return e == 0 ? screenBlue : e == 1 ? screenOchre : e == 2 ? screenGray : screenOrange; }
} // namespace colours

/** The thin geometric type of the legends (and the screen's). */
juce::Font legendFont (float height, bool light = true);

/** Draws a key's legend (gray line art, or red for record and input) centred in `r`. */
void drawLegend (juce::Graphics& g, Key k, juce::Rectangle<float> r);

/** A square cell key: a flat off-white tile with a round boss (most keys) or a square boss (the blank key). */
class CellKey : public juce::Component
{
    friend struct GpuiAdapter;

public:
    enum class Boss { Round, Square };
    CellKey (Key key, Boss boss);
    Key key;
    std::function<void()> onPress, onRelease;
    /** Shown held (from the computer keyboard, or the machine). */
    void setShownDown (bool d);
    bool isShownDown() const { return mouseHeld || shownDown; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    Boss boss;
    bool mouseHeld = false, shownDown = false;
};

/** A key of the keyboard: a tall pill (white) on its two-cell tile, or a black disc on its tile of the black row
    (whose width varies; `discX` is the disc's centre, 0..1 across the tile). */
class PianoKey : public juce::Component
{
    friend struct GpuiAdapter;

public:
    PianoKey (int index, bool black, float discX = 0.5f);
    int index;
    bool black;
    std::function<void()> onDown, onUp;
    void setShownDown (bool d);
    bool isShownDown() const { return mouseHeld || shownDown; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    float discX;
    bool mouseHeld = false, shownDown = false;
};

/** An endless encoder in its 2 x 2 cell: a broad ring with a coloured cap. Drag (or scroll) turns it; a click
    without a drag is a tap. */
class Encoder : public juce::Component
{
    friend struct GpuiAdapter;

public:
    explicit Encoder (int index);
    int index;
    std::function<void (int)> onTurn;
    std::function<void()> onTap;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    /** Turns it as if by hand (the self-test). */
    void turnBy (int clicks);
    float angle = 0.0f; // (for the knurl's shine)

private:
    juce::Point<float> last;
    float travel = 0.0f, accum = 0.0f, wheelAccum = 0.0f;
    bool pressed = false;
};

/** The volume knob in its cell: drag up or down (or scroll) to set it. */
class VolumeKnob : public juce::Component
{
    friend struct GpuiAdapter;

public:
    VolumeKnob();
    std::function<float()> get;
    std::function<void (float)> set;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    float startValue = 0.0f;
};

/** The level meter on the right edge: a column of small white LEDs (the battery while help is held). */
class LevelMeter : public juce::Component
{
    friend struct GpuiAdapter;

public:
    void setLevel (float level, bool battery);
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;
    bool battery = false;
};
} // namespace keytape::ui
