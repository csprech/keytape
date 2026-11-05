# Research: the portable synthesizer with a 4-track tape (teenage engineering OP–1 field)

These notes describe the OP–1 field. They are the reference for **Keytape KT-4**, an original desktop instrument built for research. Keytape reproduces the OP–1 field's architecture, controls and behaviour. It doesn't use teenage engineering's names, logos, artwork or screen graphics: its engines, effects, modulators and sequencers have their own names (see 9), and its screens are drawn from scratch. "OP–1" and "teenage engineering" are trademarks of teenage engineering AB and appear here only to identify the instrument studied.

**Sources:**
- the OP–1 field user guide (firmware 1.4.5, 150 pages), the guide's MIDI reference online, and the original OP–1 manual (v2.2), for the behaviour the two share: tape editing, the sequencers and the tricks;
- the product photographs and the guide's drawings, for the look;
- the Sound On Sound review, for details of how it sounds and behaves.

## 1. What it is

A portable synthesizer built around a 4-track stereo tape.

**Main modes** (four keys):
- **synth:** play the keyboard with the selected synth sound;
- **drum:** the keys play a drum kit;
- **tape:** record onto the tape and layer takes;
- **mixer:** mix the four tracks, with the master EQ, master effect and master out.

**Highlights** (guide p.4):
- USB audio and MIDI, as host and as device;
- a 4-track 32-bit stereo tape, with up to 8 tapes;
- a stereo sampler and a stereo drum machine;
- 7 sequencers, a 3-band EQ and 8 stereo effects;
- an FM radio receiver and transmitter, an accelerometer, MIDI over Bluetooth LE, and a speaker and microphone.

**Specs** (p.132–133):
- audio in: 3.5 mm stereo, 13 kΩ, 0–31 dB analog gain, 8 dBu max;
- audio out: 3.5 mm stereo with headset microphone support, 8 dBu max;
- battery: about 24 h;
- the keybed isn't velocity sensitive, but velocity arriving over MIDI is respected (SOS).

## 2. The look

- **Body:** a natural anodized aluminium slab, with off-white keycaps in a grid of square cells.
- **Top left:** the speaker grille, then the volume knob. A blank square cap sits beside the knob; the guide gives it no function. Under them are help (a speech bubble) and tempo (a metronome).
- **Display:** a color display in the top row.
- **Encoders:** four of them, in their own square cells. Their caps are colored: **blue**, **ochre**, **gray** and **orange**. Each is knurled and endless, and works as a push button too.
- **Top right:** the input key (a red needle), then COM.
- **Second row:**
  - the mode keys: synth (a wave), drum, tape (reels) and mixer (bars);
  - the track keys T1–T4 (large numerals);
  - 1–8, whose sub-labels are the tape tricks: IN, OUT, loop, break (reels), reverse (a mirrored R), chop (dots), M1 and M2;
  - the sequencer key (dots).
- **Left block:**
  - lift (up arrow), drop (down arrow) and split (scissors);
  - record (with a red dot), play and stop;
  - the left and right arrows, and shift.
- **Keyboard:** 24 keys from F to E, with tall rounded white keys and round black keys.
- **Right edge:** the product name runs vertically, with a level meter as a vertical LED strip beside it.
- **Key legends:** gray line icons. Only record and input carry red.

## 3. Keys and encoders (p.10–16)

**Help and shift:**
- **help:** shows a quick help overlay for the screen. Help with another key explains that key; held while playing, it shows the note; held on its own, it shows the battery on the level meter. Shift + help opens the built-in guide.
- **shift:** gives the secondary functions. With an encoder it fine-tunes; with a key it opens a browser or does that key's shift function.

**T1–T4:**

| | synth / drum | tape | mixer |
|---|---|---|---|
| T1 | engine | track 1 | mixer |
| T2 | envelope | track 2 | equalizer |
| T3 | FX | track 3 | master FX |
| T4 | LFO | track 4 | master out |

**1–8:**
- In synth or drum mode, 1–8 select the preset sounds or kits. Shift + 1–8 opens the preset browser: blue picks the engine, ochre the preset.
- In tape and mixer mode, 1–8 are the tape tricks (see 6).

**Shift + T1–T4:**
- In synth or drum mode, they open the browsers for engine, envelope (play mode), FX and LFO.
- In mixer mode, shift + T1–T4 mute tracks 1–4.

**Encoders:** each turns the parameter of its color on the display. Shift + turn is the second parameter, or fine tuning. A tap means confirm, return to default, or zoom (sampler).

**The arrows:** octave down and up in synth and drum mode; rewind and fast forward in tape mode.

## 4. Synth mode (p.18–61)

A **sound** has four modules:
- **T1:** the engine;
- **T2:** the envelope;
- **T3:** the FX, with an on/off toggle (press T3 again);
- **T4:** the LFO, with an on/off toggle (press T4 again).

Shift + T1 changes only the engine; shift + 1–8 loads a whole preset. Holding a sound key for three seconds saves a snapshot of it into the snapshot folder.

**The engines, with their encoders.** Shifted values are after "/".

| original | Keytape | blue | ochre | gray | orange |
|---|---|---|---|---|---|
| cluster | SWARM | wave number | wave envelope | spread | unitor |
| digital | BITS | wave shaper | octave | detune and ring modulation | digitalness |
| dimension | SHAPE | waveform: noise, pulse, square, saw, saw+sub, back to noise | modulation (stereo width) | filter cutoff | filter resonance |
| dna | GENE | filter | wave number | wave modifier | noise |
| drwave | WARP | wave type and length | filter | phase | chorus |
| dsynth | DUAL | frequency / env crossfader | waveform / frequency | envelope / waveform | cross modulation / filter cutoff |
| fm | FM | fm amount | frequency | topology | detune |
| phase | PHASE | phase shift | distortion amount | phase filter | phase tilt |
| pulse | PULSE | filter | amplitude | second pulse | modulation |
| sampler | SAMPLER | start / direction | loop in / fine tune | loop out / loop fade | end / gain |
| string | STRING | tension | decay | detune | impulse |
| voltage | VOLT | modulation | ground noise | phase filter | detune |
| vocoder | VOCODER | waveform | formant | bands | mix |

- **Sampler:** a chromatic stereo sampler with a loop region. It plays from the start, loops while the key is held and plays to the end after release. Tapping an encoder zooms around its point. Input + key samples from the input source: the key is held while recording, with threshold and gain set on the input screen.
- **Vocoder:** the selected input source is the modulator.

**The envelope (T2)** is attack, decay, sustain and release. Shift shows the play mode settings:
- play mode: poly, mono, legato or unison;
- portamento;
- bend range;
- volume.

**FX (T3):** one effect at a time. The same 8 effects are the master effects in the mixer. Their encoders:

| original | Keytape | blue | ochre | gray | orange |
|---|---|---|---|---|---|
| cwo | COMB | frequency | delay | feedback | sideband |
| delay | DELAY | range | speed | feedback | level |
| grid | CUBE | x size | y size | z feedback | mix |
| mother | HALL | distance | gate | color | mix |
| nitro | SWEEP | frequency | filter follow | feedback | frequency |
| phone | LOFI | tone | gsm | baud | telemetry |
| punch | THUMP | frequency | punch | rounds | power |
| spring | SPRING | tone | turns | damping | mix |

**LFO (T4):** it modulates one parameter of the engine, envelope, FX or main.

| original | Keytape | controls |
|---|---|---|
| random | RANDOM | speed, amount, destination, envelope |
| element | SENSE | source, amount, destination, parameter |
| midi | MIDI | CC 1–4 to four destinations |
| tremolo | TREMOLO | speed (fine), pitch amount, volume level, envelope; shapes sine, saw, exp, square, blip |
| value | VALUE | shape (tap blue: square, ramp, saw, sine), speed, amount, destination, parameter; synced to notes or free at the tempo |
| velocity | VELOCITY | destination amount, volume amount, destination, parameter |

The SENSE sources are:
- the input (mic, line, radio or USB);
- g-force (tilting or shaking the unit);
- the envelope;
- sum (the main output).

## 5. Drum mode (p.62–69)

A **kit** has four modules:
- **T1:** the drum engine;
- **T2:** the drum envelope;
- **T3:** FX;
- **T4:** LFO.

Shift + 1–8 loads a kit.

**Drum sampler:**
- one stereo sample, sliced across the 24 keys;
- per key: in point, out point, tuning, play mode, direction, panning, attack and gain;
- shift gives fine tuning, zoom, and channels stereo L+R or stacked A+B;
- to lay out a key, press it, set its in and out points, and press it again.

**Drum synth** (original "dbox"; Keytape KIT):
- a dual-oscillator drum synth, with a different sound on every key;
- oscillator 1: pitch, waveform, envelope and cross modulation;
- shift gives oscillator 2: pitch, waveform, envelope and filter cutoff.

**Drum envelope:** a transient shaper with attack, gain, release and timing.

## 6. Tape mode (p.70–77; original manual 8.x)

- **Tapes:** each has 4 stereo tracks, 6 minutes per track at normal speed, and 32-bit audio. There are 8 tapes in memory.
- **Track keys:** T1–T4 select the track.
- **Recording:**
  - Hold record and play, or press record then play.
  - Recording overdubs: it mixes into what's already on the track.
  - Shift + record arms the tape without the reels moving. Recording then follows the blue encoder, like a hand-turned reel.

**Encoders:**
- **blue:** scrub; shift slides a take;
- **ochre:** loop out point; shift sets the loop in point;
- **gray:** tape speed; shift sets it in steps ("tape speed %");
- **orange:** recording level; shift sets the loop out point.

**Edit keys:**
- **lift:** cuts the take under the head into memory. It's also how a take is deleted.
- **drop:** pastes the take in memory at the head. Repeated presses paste repeatedly.
- **split:** splits the take at the head.
- **shift + lift:** lifts all four tracks.
- **shift + split:** joins two nearby takes.
- **region lift:** with loop points set, lift takes just the region.

**Transport:**
- The arrows rewind and fast-forward.
- Stop + left goes to the start; stop + right goes to the end of the last take.
- Shift + arrows jump between bar markers (with beat match). While looping, they move the loop by a bar.
- Shift + play plays in reverse.

**Tape tricks (1–8):**
- **1:** loop in;
- **2:** loop out;
- **3:** loop on/off (shift + 3: loop the current take);
- **4:** break, a tape stop (a running loop keeps time in the background);
- **5:** reverse;
- (The key legends on the field's photos settle the order: 4 shows the reels, 5 a mirrored R.)
- **6:** chop, a tempo-locked repeat;
- **7 and 8:** memo 1 and 2. Hold the key and turn an encoder to memorise a parameter value. Press the key to recall it.

**More shift functions** (p.74–75):
- **shift + record:** arm recording;
- **shift + play:** play in reverse;
- **shift + stop:** tape grid resolution;
- **shift + lift:** lift all four tracks within the active loop;
- **shift + split:** join two nearby takes;
- **shift + left / right:** jump between beat markers;
- **shift + synth or drum:** revert preset (discard changes to the active sound);
- **shift + tape:** the tape browser, which also erases a tape.

**Tape browser:** a tape style for each tape, with rename, load, new and erase.

**Tape styles:**
- **studio 4-track:** 15 inches a second, clean;
- **vintage 4-track:** 7½ inches a second, warm and saturated;
- **porta 4-track:** cassette at 3¾ inches a second;
- **disc mini:** magneto-optical, with lossy, psychoacoustic compression.

**Speed and quality:** speed changes pitch and time as on real tape. The faster the tape, the better the recording.

**Lift and drop between modes:**
- Lift a synth or drum sound, then drop it onto the tape: this stores the sound as data.
- Lift that take back off the tape and drop it into a sound slot to recall the sound.

## 7. Mixer (p.78–85)

- **T1 mixer:** the four track levels (0–99), pans (shift) and mutes (shift + T1–T4).
- **T2 equalizer:** low, mid, high and amount, each with a toggle (tap).
- **T3 master FX:** one of the 8 effects on the mix. It isn't recorded to tape, but it is in the output mixdown.
- **T4 master out:** left and right levels (shift links them), drive and release. Drive compresses; at high levels it distorts, and with long release it pumps.
- **Shift + mixer:** the sound path screen. A warning shows if any critical level is zero.

## 8. Other screens

**Sequencers** (p.86–99):
- Shift + sequencer opens the browser, and sequencer toggles it on or off.
- Synth and drum each have their own sequencer memory.
- The key transposes the sequence; C plays it at its original pitch.

| original | Keytape | how it works |
|---|---|---|
| arpeggio | ARP | note value, trigger mode (none, once, each, all, trig), trigger pattern, hold; shift: note length, type (0–3), pause or skip, swing |
| endless | CHAIN | shift + keys enter notes step by step (128 at most); shift + right inserts a rest and shift + left deletes the last note. Note value, manual (crank), swing, trigger pattern; shift: rotate pattern, hold, direction (forward, reverse, random) |
| finger | HANDS | a sequence per key, two players; cursor, erase, swing, length, hold, play mode (join, replace, fill in) |
| hold | HOLD | the keyboard split into a drone area: clear, break point, mono or poly, transpose, hold |
| pattern | GRID | 16 steps: shift + key inserts at the cursor; erase, swing, length, live mode (hold), move the section, direction (forward, reverse, cycle) |
| sketch | DRAW | draw pitch against time with blue and ochre, move with gray and orange; divider, grid, hold. Shaking erases |
| tombola | SPIN | drop notes into a spinning drum: speed and direction, opening, heaviness (gravity), bounciness (mass); crank by hand |

**Tempo** (tempo key):
- **bpm:** blue, with fine tuning on shift; tap tempo on the key; the arrows nudge it.
- **sync** (ochre):
  - **free:** no sync;
  - **beat match:** master; tape speed is linked to the tempo, and there are bar markers;
  - **midi sync:** follows an external clock;
  - **PO sync:** a sync pulse on the left output and the mix on the right; shift + ochre gives 1/16 sync.
- **gray:** tape speed.
- **orange:** the metronome level; a tap sets its sound.

**Settings** (shift + tempo): the clock, system (brightness, region, power-off delay) and tuning (global detune).

**Input** (shift + input):
- **source** (blue): the microphone or line in, FM radio, USB audio, or the output fed back into the input (resample);
- **stereo or mono** (ochre, for line in), or the FM frequency (a tap scans);
- **threshold** (gray) and **gain** (orange).

Input on its own toggles the source on, or samples in a sampler context.

**Output:**
- **mixdown:** T4 records, T2 stops, T3 prepares the next recording. Side A and side B are 6 minutes each.
- **FM transmit:** broadcasts the output over FM.

**COM** (shift + output):
- **T1 midi:** channel (blue), clock (ochre), notes (gray) and other messages (orange);
- **T2 ctrl:** turns it into a MIDI controller keyboard (channel, relative or absolute encoders, octave);
- **T3 list:** the MIDI devices;
- **T4 mtp / disk:** file access from the computer;
- **blue:** advertises it as a Bluetooth LE MIDI device; **orange:** USB charging on or off.

**MIDI implementation** (guide online):
- **Incoming:**
  - notes;
  - pitch bend (synth);
  - program change: 0–7 select synth slots and 8–15 drum slots;
  - clock, start, stop and continue (which also control tape playback), and song position.
- **CC:**
  - **mixer:** 7, 9 and 10 on channels 1–4 are track level, mute and pan.
  - **sound:**
    - 46–49: engine 1–4 (in drum mode: key pitch, in, out, play mode);
    - 50–53: envelope (drum: attack, gain, release, smooth);
    - 54–57: FX;
    - 58–61: LFO;
    - 62/63: randomize and reset;
    - 64: sustain.
  - **master:** 70–73 master FX; 74–77 master compressor; 90–92 EQ.
  - **tape and tempo:**
    - 78: record level;
    - 79: octave;
    - 80: tempo (0–5: 40–50 BPM, 6–120: 52–166, 121–127: 168–180);
    - 81: metronome;
    - 82–88: tape previous and next bar, start, end, loop in, loop out and loop toggle;
    - 104/105: tape stop and play.
  - **modes and slots:** 93 mode (synth or drum); 102 on channels 1–8 selects slot 1–8.
  - **notes:** 123: all notes off.

## 9. Decisions for the reproduction

1. **A headless instrument** (core/Machine): every key, encoder (turn, tap, shift) and screen, with the display described as data. Tests play it like a person would.
2. **Original names and graphics:**
   - The engines, effects, modulators and sequencers keep the original's controls and roles but get their own names (tables above). Generic words such as fm, phase, pulse, sampler, string, vocoder, delay and spring stay.
   - The screens are a visual language of Keytape's own. Each screen shows the sound it makes (a live trace of the output, or the shape the parameters draw), with the four parameters in their encoder colors.
3. **The sounds are synthesized by Keytape's own engines.** No teenage engineering presets or samples are used, and the factory kits and presets are original.
4. **The tape is real tape behaviour:**
   - four stereo tracks of takes, overdubbed in place;
   - varispeed, where slower recordings lose quality;
   - reverse, loop, break, chop, lift, drop, split, join, slide, scrub and arm with a hand-turned reel;
   - the four tape styles' coloring.
5. **On the computer:**
   - **Line in and the microphone** are the computer's audio input.
   - **USB audio** is a second input pair.
   - **The FM radio receiver** is a band of noise, plus stations: audio files in a "radio" folder, each on its own frequency. A tap scans for them.
   - **FM transmit** can't broadcast; its screen works, but nothing goes on air.
   - **The accelerometer** is the mouse: with option held, moving it tilts the unit, and shaking it shakes the unit.
   - **The battery** shows as full on USB power.
   - **Bluetooth MIDI** isn't reproduced.
   - **The blank square cap** has no function, as in the guide.
6. **Files:**
   - Tapes, mixdowns, sounds and snapshots live in a folder, `Music/Keytape`, which stands for the MTP / disk view.
   - Each tape keeps its tracks as WAV files.
