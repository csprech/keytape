#pragma once

// The 4-track tape: each track a list of takes (stretches of stereo audio placed along the tape). Recording
// overdubs: the new take is what was on the tape plus what's played, and it replaces what it covers. The tape
// moves at a speed (varispeed: slower is lower quality, as the take is written at the tape's own rate), either way,
// looped, stopped short (break) or chopped. Lift, drop, split, join and slide edit the takes.

#include "Model.h"
#include <optional>

namespace keytape
{
struct Take
{
    AudioPtr audio;      // stereo, at the tape's rate
    int64_t start = 0;   // on the tape (tape samples)
    int64_t offset = 0;  // into the audio
    int64_t length = 0;
    juce::var soundData; // a sound saved to tape as data (lifted from synth or drum mode)
    int64_t end() const { return start + length; }
};

struct TapeTrack
{
    std::vector<Take> takes; // sorted by start, not overlapping
    int takeAt (int64_t pos) const;
    float read (double pos, int ch, int& hint) const;
};

/** A tape: its name and style, four tracks, the loop, and the tempo the tape runs at normal speed. */
struct Reel
{
    juce::String name { "tape" };
    TapeStyle style = TapeStyle::Studio;
    std::array<TapeTrack, kTracks> tracks;
    int64_t loopIn = -1, loopOut = -1;
    bool loopOn = false;
    double bpmAtNormal = 120.0;
    bool dirty = false;
    bool empty() const;
    int64_t lastEnd() const;
};

class TapeMachine
{
public:
    void prepare (double sampleRate, int maxBlock);
    double getSampleRate() const { return sr; }
    int64_t tapeLength() const { return (int64_t) (6.0 * 60.0 * sr); } // six minutes at normal speed

    Reel reel;

    // ---- transport (message thread, under the instrument's lock)
    bool isPlaying() const { return playing; }
    bool isRecording() const { return recording; }
    bool isArmed() const { return armed; }
    void play();
    void stop();
    /** Record: from stop it starts recording and playing; while playing it punches in. */
    void record();
    /** Shift + record: recording with the reels still; the tape moves only as it's scrubbed. */
    void arm();
    void setReversePlay (bool r) { reversePlay = r; }
    bool getReversePlay() const { return reversePlay; }
    double position() const { return pos; }
    void locate (int64_t p);
    /** The arrow keys held: shuttle at speed (+-), 0 to let go. */
    void shuttle (int direction);
    /** The blue encoder: moves the tape by `ticks` (audible, and recorded if armed). */
    void scrub (int ticks);
    int track = 0; // the selected track (T1-T4)
    float recLevel = 0.8f;
    /** Varispeed: 1 = normal. In beat match it follows the tempo. */
    double speed = 1.0;
    int gridResolution = 1; // 0 off, 1 bar, 2 beat, 3 half beat (shift + 2)

    // ---- tricks
    void setLoopIn() { reel.loopIn = snap ((int64_t) pos); fixLoop(); }
    void setLoopOut() { reel.loopOut = snap ((int64_t) pos); fixLoop(); }
    void toggleLoop();
    void loopCurrentTake();
    void setTrickReverse (bool on) { trickReverse = on; }
    void setBreak (bool on);
    void setChop (bool on, double bpm);
    bool isBreaking() const { return breaking; }
    bool isChopping() const { return chopping; }
    void moveLoopByBar (int dir);
    void jumpBar (int dir);
    int64_t barLength() const { return (int64_t) (4.0 * 60.0 / reel.bpmAtNormal * sr); }

    // ---- edits (message thread)
    /** Lift: the take under the head on the selected track (or, with the loop set, the loop's region) into memory. */
    bool lift();
    /** Lift all: the same across the four tracks. */
    bool liftAll();
    /** Drop what's in memory at the head (the tape moves to its end). */
    bool drop();
    bool split();
    bool join();
    /** Slide the active take (shift + blue), by ticks; it stops at its neighbours. */
    void slide (int ticks);
    void eraseAll();
    /** What lift put in memory: per track (one, or four for lift all) and a mix (for the samplers). */
    std::array<std::optional<Take>, kTracks> clipboard;
    bool clipboardAll = false;
    AudioPtr clipboardMix() const;
    /** Drop a sound saved as data onto the selected track at the head (lift in synth / drum mode). */
    void dropSoundData (const juce::var& sound, int64_t lengthSamples);
    /** The take under the head on the selected track (for recalling a sound saved as data). */
    const Take* activeTake() const;

    // ---- audio (audio thread)
    /** Plays the four tracks into out[track][2] (n samples) and records `in` (stereo) onto the selected track. */
    void process (int n, const float* inL, const float* inR, std::array<std::array<float*, 2>, kTracks>& out);
    /** A take finished recording: the message thread files it (allocation happens there). */
    bool takePendingTake();
    float meter() const { return lastLevel; }

private:
    void finishRecording();
    void ensureRecBuffer(); // (message thread, under the lock)
    void insertTake (int t, Take take);
    void removeRange (int t, int64_t from, int64_t to);
    AudioPtr render (int t, int64_t from, int64_t to) const;
    int64_t snap (int64_t p) const;
    void fixLoop();
    void writeRec (double p, float l, float r);

    double sr = 48000.0;
    double pos = 0.0;
    bool playing = false, recording = false, armed = false, reversePlay = false, trickReverse = false;
    int shuttleDir = 0;
    float scrubVelocity = 0.0f;
    // break: the tape slows to a stop while the time runs on
    bool breaking = false;
    float breakSpeed = 1.0f;
    double virtualPos = 0.0;
    // chop: a tempo-locked repeat of the slice at the head
    bool chopping = false;
    double chopStart = 0, chopLen = 0;
    // recording: written at absolute tape positions into a scratch track, filed as a take afterwards
    juce::AudioBuffer<float> recBuf;
    int64_t recMin = -1, recMax = -1;
    double lastRecPos = -1.0;
    float lastRecL = 0, lastRecR = 0;
    bool pendingTake = false;
    std::array<int, kTracks> hints {};
    float lastLevel = 0.0f;
};
} // namespace keytape
