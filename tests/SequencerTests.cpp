// The seven sequencers, run on their own: notes in, notes out at the tempo.

#include "Rig.h"

namespace keytape::test
{
namespace
{
    /** Runs `seconds` at 120 bpm; returns the notes started, in order. */
    std::vector<int> runSeq (Sequencer& s, double seconds, std::vector<NoteEvent>* all = nullptr)
    {
        std::vector<int> ons;
        std::vector<NoteEvent> ev;
        for (int64_t done = 0; done < (int64_t) (seconds * kSr); done += kBlock)
        {
            ev.clear();
            s.process (kBlock, kSr, 120.0, ev);
            for (auto& e : ev)
            {
                if (e.on)
                    ons.push_back (e.note);
                if (all != nullptr)
                    all->push_back (e);
            }
        }
        return ons;
    }
} // namespace

class SequencerTests : public juce::UnitTest
{
public:
    SequencerTests() : juce::UnitTest ("sequencers", "keytape") {}

    void runTest() override
    {
        beginTest ("arp: plays the held chord up, in sixteenths, and stops when let go");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Arp;
            expect (s.keyDown (60, false) && s.keyDown (64, false) && s.keyDown (67, false));
            const auto ons = runSeq (s, 1.0);
            expect (ons.size() >= 7 && ons.size() <= 9, "eight sixteenths a second at 120: " + juce::String ((int) ons.size()));
            expect (ons.size() >= 3 && ons[0] == 60 && ons[1] == 64 && ons[2] == 67);
            s.keyUp (60, false);
            s.keyUp (64, false);
            s.keyUp (67, false);
            std::vector<NoteEvent> all;
            const auto after = runSeq (s, 0.5, &all);
            expect (after.empty());
            // note value: quarter notes
            s.turn (0, -5, false);
            expectEquals (s.paramText (0), juce::String ("1/4"));
            s.keyDown (60, false);
            expect (runSeq (s, 1.0).size() <= 3);
            s.keyUp (60, false);
            // hold latches the chord
            s.turn (3, 1, false);
            s.keyDown (62, false);
            s.keyUp (62, false);
            expect (! runSeq (s, 1.0).empty(), "held after release");
        }

        beginTest ("chain: notes entered with shift play in order, transposed by the key held");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Chain;
            for (int n : { 60, 62, 64 })
            {
                s.keyDown (n, true);
                s.keyUp (n, true);
            }
            expect (s.arrow (1, true), "a rest");
            expectEquals ((int) s.chain.size(), 4);
            expect (runSeq (s, 0.5).empty(), "nothing until a key is held");
            s.keyDown (65, false); // F: up five semitones
            const auto ons = runSeq (s, 0.6);
            expect (ons.size() >= 3 && ons[0] == 65 && ons[1] == 67 && ons[2] == 69, ons.empty() ? juce::String ("none") : juce::String (ons[0]));
            s.keyUp (65, false);
            s.arrow (-1, true);
            expectEquals ((int) s.chain.size(), 3, "shift + left deletes the last step");
        }

        beginTest ("hands: a pattern per white key, played while the key is held");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Hands;
            s.keyDown (kLowestNote, false); // the first white key selects its pattern
            s.keyUp (kLowestNote, false);
            s.handsCursor = 0;
            s.keyDown (72, true);
            s.keyUp (72, true);
            s.keyDown (74, true);
            s.keyUp (74, true);
            s.keyDown (kLowestNote, false);
            const auto ons = runSeq (s, 1.0);
            expect (std::count (ons.begin(), ons.end(), 72) >= 1 && std::count (ons.begin(), ons.end(), 74) >= 1);
            s.keyUp (kLowestNote, false);
        }

        beginTest ("hold: keys below the break point drone until pressed again");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Hold;
            expect (s.keyDown (55, false));
            s.keyUp (55, false);
            expect (! s.keyDown (72, false), "above the break point plays normally");
            const auto ons = runSeq (s, 0.2);
            expectEquals ((int) std::count (ons.begin(), ons.end(), 55), 1);
            s.keyDown (55, false);
            s.keyUp (55, false);
            expect (s.drones.empty());
        }

        beginTest ("grid: steps entered with shift, played while a key is held (or live)");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Grid;
            s.keyDown (60, true);
            s.keyUp (60, true);
            s.keyDown (64, true);
            s.keyUp (64, true);
            expectEquals (s.gridCursor, 2);
            s.keyDown (60, false);
            const auto ons = runSeq (s, 2.0);
            expectEquals ((int) std::count (ons.begin(), ons.end(), 60), 1, "one bar of sixteen steps in two seconds");
            expectEquals ((int) std::count (ons.begin(), ons.end(), 64), 1);
            s.keyUp (60, false);
            s.turn (3, 1, false); // live
            expect (! runSeq (s, 2.0).empty());
            s.tap (0, false);
            s.turn (3, -1, false);
            expect (s.grid[0].empty(), "tap blue clears");
        }

        beginTest ("draw: blue draws a line, the key plays it");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Draw;
            s.drawY = 5;
            s.turn (0, 16, false);
            expectEquals (s.drawX, 16);
            expectEquals (s.draw[10], 5);
            s.turn (1, 4, false);
            s.turn (0, 16, false);
            expectEquals (s.draw[20], 9);
            s.keyDown (60, false);
            const auto ons = runSeq (s, 2.5);
            s.keyUp (60, false);
            expect (ons.size() >= 2, "plays the drawn notes: " + juce::String ((int) ons.size()));
            s.shake();
            expect (std::all_of (s.draw.begin(), s.draw.end(), [] (int v) { return v < 0; }), "a shake erases");
        }

        beginTest ("spin: balls dropped in bounce off the walls and play");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Spin;
            s.keyDown (60, false);
            s.keyDown (67, false);
            expectEquals ((int) s.balls.size(), 2);
            const auto ons = runSeq (s, 4.0);
            expect (! ons.empty(), "the balls hit the walls");
            s.tap (0, false);
            expect (s.balls.empty());
        }

        beginTest ("stopping lets every note go");
        {
            Sequencer s;
            s.enabled = true;
            s.type = Seq::Arp;
            s.keyDown (60, false);
            std::vector<NoteEvent> all;
            runSeq (s, 0.1, &all);
            std::vector<NoteEvent> off;
            s.stopAll (off);
            int ons = 0, offs = 0;
            for (auto& e : all)
                (e.on ? ons : offs)++;
            for (auto& e : off)
                if (! e.on)
                    ++offs;
            expectEquals (ons, offs);
        }
    }
};

static SequencerTests sequencerTests;
} // namespace keytape::test
