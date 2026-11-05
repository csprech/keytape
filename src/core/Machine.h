#pragma once

// Keytape without its window: every key, the keyboard, the four encoders (turn, tap, with shift), the screens
// and browsers, the tape and mixer, tempo and sync, input and output, COM and MIDI. The message thread calls the
// control functions and the audio thread calls process(), both under the owner's lock. The display is described
// as data (describe), which the panel draws and the tests read.

#include "Instruments.h"
#include "Sequencers.h"
#include "Tape.h"
#include <functional>

namespace keytape
{
enum class Key : uint8_t
{
    Help, Tempo, Blank,
    Synth, Drum, Tape, Mixer,
    T1, T2, T3, T4,
    K1, K2, K3, K4, K5, K6, K7, K8,
    Seq, Input, Com,
    Lift, Drop, Split,
    Record, Play, Stop,
    Left, Right, Shift,
    Count
};
const char* keyName (Key);

enum class Page : uint8_t
{
    Engine, Envelope, Fx, Lfo,
    PresetBrowser, EngineBrowser, FxBrowser, LfoBrowser, SeqBrowser, TapeBrowser,
    TapePage, MixerPage, EqPage, MasterFxPage, MasterOutPage, SoundPath,
    TempoPage, SettingsPage, InputPage, OutputPage, ComPage, MidiPage, CtrlPage, ListPage, DiskPage,
    SequencerPage, Guide, EraseConfirm
};

/** Files: the presets' snapshots, tapes, mixdowns (provided by the engine; the tests leave it out). */
struct Library
{
    virtual ~Library() = default;
    virtual juce::StringArray snapshots (bool drum) = 0;
    virtual bool loadSnapshot (bool drum, const juce::String& name, SynthSound* s, DrumKit* k) = 0;
    virtual void saveSnapshot (bool drum, const SynthSound* s, const DrumKit* k) = 0;
    virtual void saveMixdown (int side, const juce::AudioBuffer<float>& audio, int length, double sampleRate) = 0;
    virtual void reveal() = 0;
    virtual juce::StringArray midiDevices() = 0;
};

struct ParamView
{
    juce::String name, text;
    float value = 0.0f; // 0..1 (bipolar ones centre at 0.5)
    bool visible = false;
};

/** Everything the display shows, copied out under the lock. */
struct DisplayState
{
    Mode mode = Mode::Synth;
    Page page = Page::Engine;
    bool shift = false;
    juce::String title, subtitle, help, popup;
    std::array<ParamView, 4> params;
    std::array<float, 256> scope {};
    // the sound
    Engine engine = Engine::Shape;
    DrumEngine drumEngine = DrumEngine::Kit;
    std::array<float, kParams> engineParams {}, envParams {}, fxParams {}, lfoParams {};
    Fx fxType = Fx::Delay;
    Lfo lfoType = Lfo::Tremolo;
    bool fxOn = false, lfoOn = false;
    float lfoShown = 0.0f;
    int slot = 0, octave = 0, drumKey = 0;
    juce::String soundName;
    std::array<bool, kKeys> keysDown {};
    std::vector<float> sampleOverview; // the sampler's waveform (peaks), and its points
    std::array<float, 4> samplePoints {};
    bool zoomed = false;
    // the tape
    struct TakeView
    {
        int track = 0;
        double start = 0, end = 0; // seconds
        bool active = false, sound = false;
    };
    std::vector<TakeView> takes;
    double head = 0, length = 360, loopIn = -1, loopOut = -1, barSeconds = 2;
    bool loopOn = false, playing = false, recording = false, armed = false, reverse = false, breaking = false, chopping = false;
    double speed = 1.0;
    float recLevel = 0.8f, tapeLevel = 0.0f;
    int track = 0, tapeIndex = 0, grid = 1;
    TapeStyle style = TapeStyle::Studio;
    juce::String tapeName;
    // the mixer
    std::array<float, kTracks> levels {}, pans {}, trackMeters {};
    std::array<bool, kTracks> mutes {};
    std::array<float, 4> eq {}, masterOut {};
    std::array<bool, 4> eqOn {};
    float gainReduction = 0.0f;
    bool linked = true;
    // lists (browsers, guide, menus)
    juce::String listTitle;
    juce::StringArray list;
    int listIndex = 0;
    juce::StringArray list2;
    int list2Index = 0;
    // the sequencer
    Seq seqType = Seq::Arp;
    bool seqOn = false;
    int seqStep = 0;
    std::vector<int> seqNotes;                 // chain notes, held arp notes
    std::array<std::vector<int>, 16> seqGrid;  // grid steps
    std::array<int, 64> seqDraw {};
    std::array<int, 16> seqHands {};
    std::vector<std::pair<float, float>> balls;
    float spinAngle = 0.0f;
    int spinSides = 6, seqCursor = 0, seqLength = 16;
    std::vector<int> drones;
    int holdBreak = 65;
    // tempo, input, output
    double bpm = 120.0;
    Sync sync = Sync::BeatMatch;
    bool beat = false, downbeat = false, externalClock = false, sixteenth = false;
    double beatPos = 0.0; // beats since the tape started (the metronome's swing)
    float metronome = 0.0f;
    Input inputSource = Input::MicLine;
    bool inputOn = false, inputStereo = true;
    double fmMHz = 98.0;
    std::vector<double> stations;
    juce::String stationName;
    float inputLevel = 0.0f, threshold = 0.0f, gain = 0.5f;
    std::array<bool, 4> eraseKeys {};
    int outputMode = 0, side = 0;
    bool mixRecording = false, fmLive = false;
    double mixSeconds = 0.0;
    // com
    Settings settings;
    bool bleAdvertising = false;
    // the level meter (or the battery, while help is held)
    float meter = 0.0f;
    bool showBattery = false;
};

class Machine
{
public:
    Machine();
    void prepare (double sampleRate, int maxBlock);
    double getSampleRate() const { return sr; }
    void setLibrary (Library* l) { library = l; }

    // ---- the panel
    void press (Key k);
    void release (Key k);
    bool isDown (Key k) const { return down[(size_t) k]; }
    /** The keyboard: key index 0..23 (F to E). */
    void keyDown (int key, float velocity = 0.8f);
    void keyUp (int key);
    void turn (int encoder, int delta);
    void tap (int encoder);
    /** g-force (the mouse with option): tilt -1..1, and a shake. */
    void setTilt (float t) { gforce = juce::jlimit (-1.0f, 1.0f, t); }
    void shake();
    /** A note from outside (MIDI), already on the instrument's channel. */
    void midiNote (int note, float velocity, bool on);
    void midiMessage (const juce::MidiMessage& m);

    // ---- what's shown
    void describe (DisplayState& d) const;
    Mode getMode() const { return mode; }
    Page getPage() const { return page; }
    juce::String helpText() const { return helpOverlay; }

    // ---- audio
    struct AudioIO
    {
        const float* micL = nullptr;  // the computer's input (mic / line)
        const float* micR = nullptr;
        const float* usbL = nullptr;  // a second pair (USB audio)
        const float* usbR = nullptr;
        float* outL = nullptr;
        float* outR = nullptr;
    };
    void process (int n, const AudioIO& io, juce::MidiBuffer* midiIn, juce::MidiBuffer* midiOut);
    /** Message thread, regularly: files finished takes, saves mixdowns and snapshots, times pop-ups. */
    void idle();

    // ---- state (the engine saves and loads it; the tests read it)
    Sounds sounds;
    MixerState mixer;
    TempoState tempo;
    InputState input;
    Settings settings;
    TapeMachine tape;
    std::array<Reel, kTapes> reels; // (the tape in the machine is tape.reel; the others wait here)
    int tapeIndex = 0;
    std::array<Sequencer, 2> seqs; // synth, drum
    Mode instrument = Mode::Synth; // what the keyboard plays in tape and mixer mode
    std::array<int, 2> octave {};
    std::array<SynthSound, kSlots> savedSynth; // (what revert preset goes back to)
    std::array<DrumKit, kSlots> savedDrum;
    bool settingsDirty = false;
    juce::String popupText() const { return popup; }
    /** The radio's stations: audio files on their frequencies (the engine finds them; none = Keytape's own). */
    struct Station
    {
        double mhz = 98.0;
        AudioPtr audio;
        juce::String name;
    };
    std::vector<Station> stations;

    SynthSound& synth() { return sounds.synth[(size_t) sounds.synthSlot]; }
    DrumKit& drum() { return sounds.drum[(size_t) sounds.drumSlot]; }
    const SynthSound& synth() const { return sounds.synth[(size_t) sounds.synthSlot]; }
    const DrumKit& drum() const { return sounds.drum[(size_t) sounds.drumSlot]; }
    void selectTape (int index);
    void loadFactoryDrumSamples();

private:
    friend struct MachineScreens;
    void modeKey (Mode m);
    void trackKey (int t);
    void slotKey (int s);
    void trick (int k, bool on);
    void transport (Key k);
    void editKey (Key k);
    void arrowKey (int dir, bool on);
    void message (const juce::String& text, double seconds = 1.2);
    void confirmBrowser();
    void openBrowser (Page p);
    Module* activeModule (bool& isFx, bool& isLfo);
    void playNote (int note, float velocity, bool on);
    bool seqActive() const { return seqs[instrument == Mode::Drum ? 1 : 0].enabled; }
    Sequencer& seq() { return seqs[instrument == Mode::Drum ? 1 : 0]; }
    const Sequencer& seq() const { return seqs[instrument == Mode::Drum ? 1 : 0]; }
    void describeParams (DisplayState& d) const;
    juce::String helpFor (Key k) const;
    juce::String screenHelp() const;
    void renderMetronome (int n, float* L, float* R);

    double sr = 48000.0;
    int maxBlock = 512;
    Library* library = nullptr;
    Mode mode = Mode::Synth;
    Page page = Page::Engine, returnPage = Page::Engine;
    std::array<bool, (size_t) Key::Count> down {};
    std::array<int64_t, (size_t) Key::Count> downAt {};
    int64_t clock = 0;
    std::array<bool, kKeys> keysHeld {};
    std::array<int, kKeys> keyNote {};
    bool usedWhileDown = false; // (a key held with another: help + key, memo + encoder)
    juce::String helpOverlay, popup;
    int64_t popupUntil = 0;
    bool zoomed = false;
    // browsers
    int browseA = 0, browseB = 0, browseSlot = 0;
    int guidePage = 0, eraseCount = 0;
    std::array<bool, kTracks> eraseKeys {};
    // memos: a parameter and its value
    struct Memo
    {
        bool set = false;
        Page page = Page::TapePage;
        int encoder = 0;
        bool shifted = false;
        float value = 0;
    };
    std::array<Memo, 2> memos;
    // clipboards for lift and drop between modes
    juce::var soundClipboard;
    bool soundClipboardDrum = false;
    int lastLift = 0; // 0 none, 1 a sound, 2 tape
    // settings screen
    int settingsItem = 0, settingsParam = 0;
    // com
    bool ctrlMode = false, bleAdvertising = false;
    int outputMode = 0, side = 0;
    bool mixRecording = false, fmLive = false;
    int64_t mixLength = 0;
    juce::AudioBuffer<float> mixBuffer;
    bool mixPending = false;
    // the instruments and the mix
    SynthHost synthHost;
    DrumHost drumHost;
    dsp::Effect masterFx;
    dsp::MasterEq eq;
    dsp::Drive drive;
    std::array<dsp::TapeColour, kTracks> colour;
    std::vector<float> bufL, bufR, instL, instR, inL, inR, inMono, recL, recR, outL, outR;
    std::array<std::array<std::vector<float>, 2>, kTracks> trackBuf;
    std::vector<NoteEvent> seqEvents;
    std::vector<std::pair<int, std::pair<int, float>>> pendingNotes; // (note, on/off, velocity) from the message thread
    std::array<float, 256> scope {};
    int scopePos = 0;
    std::array<float, kTracks> trackMeters {};
    float meterLevel = 0.0f, inputLevel = 0.0f, sumLevel = 0.0f, gforce = 0.0f;
    std::array<float, 4> cc {};
    // tempo
    double beatPhase = 0.0;
    bool beatFlash = false, downbeatFlash = false;
    std::vector<int64_t> taps;
    double clockTicks = 0.0;
    int64_t lastClockAt = -1;
    double externalBpm = 0.0;
    bool externalClock = false;
    float clickEnv = 0.0f, clickPhase = 0.0f, clickPitch = 1000.0f;
    double syncPulsePhase = 0.0;
    // radio: a band of noise with stations
    dsp::Svf radioFilter;
    dsp::Noise radioNoise;
    double radioPh = 0.0;
    // samples recorded from the input into a sampler (input + key)
    bool sampling = false;
    int samplingKey = -1;
    juce::AudioBuffer<float> sampleBuf;
    int sampleLen = 0;
    bool sampleArmed = false, sampleTriggered = false;
    bool factoryKitPlaced = false;
    AudioPtr defaultSample, factoryDrumSample;
};
} // namespace keytape
