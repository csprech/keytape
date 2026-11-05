#pragma once

// The seven sequencers (ARP, CHAIN, HANDS, HOLD, GRID, DRAW, SPIN). Synth and drum each have their own sequencer
// with its own memory. When it's on, the keyboard plays the sequencer (shift + keys enter notes); it sends notes
// to the instrument at the tempo.

#include "Model.h"
#include <functional>

namespace keytape
{
struct NoteEvent
{
    int offset = 0; // sample in the block
    int note = 60;
    float velocity = 0.8f;
    bool on = true;
};

class Sequencer
{
public:
    Seq type = Seq::Arp;
    bool enabled = false;
    bool drum = false; // (the drum sequencer: notes are keys)

    /** The keyboard while the sequencer is on. Returns true if the key was taken (not played directly). */
    bool keyDown (int note, bool shift);
    bool keyUp (int note, bool shift);
    /** Arrow keys with shift (rests, deleting, live edit). Returns true if used. */
    bool arrow (int dir, bool shift);
    void turn (int encoder, int delta, bool shift);
    void tap (int encoder, bool shift);
    void shake(); // (DRAW: erase)
    void stopAll (std::vector<NoteEvent>& out);

    /** Runs n samples at bpm; notes on and off go into out. */
    void process (int n, double sampleRate, double bpm, std::vector<NoteEvent>& out);
    /** Restarts the timing (tape play, MIDI start). */
    void restart()
    {
        tickPos = 0.0;
        step = 0;
        stepFired = false;
    }

    // ---- shown on the screen
    juce::String paramName (int i) const;
    float paramValue (int i) const; // 0..1 for the display
    juce::String paramText (int i) const;
    int currentStep() const { return step; }

    // ---- state (public for the display and the tests)
    // ARP
    std::vector<int> held;
    int arpNoteValue = 3, arpTriggerMode = 0, arpPattern = 0, arpLength = 12, arpType = 0;
    bool arpHold = false, arpSkip = true;
    float swing = 0.5f;
    // CHAIN: notes (-1 a rest, -2 holds the previous note on)
    std::vector<int> chain;
    int chainNoteValue = 3, chainPattern = 0, chainDirection = 0;
    bool chainManual = false, chainHold = false;
    int transpose = 0; // the key pressed, relative to C
    // HANDS: a pattern per white key (16 steps, a note or -1)
    std::array<std::array<int, 16>, 14> hands {};
    int handsKey = 0, handsCursor = 0, handsLength = 16, handsPlayMode = 0;
    bool handsHold = false;
    std::vector<int> handsPlaying;
    // HOLD
    int holdBreak = 65, holdTranspose = 0;
    bool holdPoly = true, holdLatch = true;
    std::vector<int> drones;
    // GRID: 16 steps, several notes each
    std::array<std::vector<int>, 16> grid;
    int gridCursor = 0, gridLength = 16, gridOffset = 0, gridDirection = 0;
    bool gridLive = false;
    // DRAW: a pitch (-1 none) per column of 64
    std::array<int, 64> draw {};
    int drawX = 0, drawY = 12, drawDivider = 2;
    bool drawGrid = false, drawHold = false;
    // SPIN: balls in a turning drum
    struct Ball
    {
        float x = 0, y = 0, vx = 0, vy = 0;
        int note = 60;
    };
    std::vector<Ball> balls;
    float spinSpeed = 0.55f, spinGravity = 0.5f, spinShape = 0.5f, spinBounce = 0.6f, spinAngle = 0.0f;
    bool spinManual = false;

    Sequencer();

private:
    void startSteps();
    void emitStep (int offset, std::vector<NoteEvent>& out);
    void allOff (int offset, std::vector<NoteEvent>& out);
    void noteOn (int offset, int note, float vel, std::vector<NoteEvent>& out);
    double ticksPerStep() const;
    bool running() const;
    double tickPos = 0.0;
    int step = 0, arpIndex = 0, chainPos = 0, pingDir = 1, patPos = 0;
    std::vector<std::pair<int, double>> sounding; // note, ticks left
    std::vector<int> keysDown;
    bool shiftEntry = false, stepFired = false;
    double crank = 0.0;
};
} // namespace keytape
