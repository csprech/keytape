// The tape: recording and overdubbing takes, playback at speed and in reverse, looping, the tricks, and the edits
// (lift, drop, split, join, slide, erase).

#include "Rig.h"

namespace keytape::test
{
namespace
{
    struct Deck
    {
        TapeMachine tape;
        std::array<std::array<std::vector<float>, 2>, kTracks> buf;
        std::array<std::array<float*, 2>, kTracks> out;
        std::vector<float> inL, inR;
        Deck()
        {
            tape.prepare (kSr, kBlock);
            for (int t = 0; t < kTracks; ++t)
            {
                for (auto& c : buf[(size_t) t])
                    c.assign (kBlock, 0.0f);
                out[(size_t) t] = { buf[(size_t) t][0].data(), buf[(size_t) t][1].data() };
            }
            inL.assign (kBlock, 0.0f);
            inR.assign (kBlock, 0.0f);
        }
        /** Runs `seconds` with `in` as the input; returns the selected track's output (left). */
        /** (the input is a function of the tape's position, so passes over the same stretch line up) */
        std::vector<float> run (double seconds, std::function<float (int64_t)> in = {}, int listen = -1)
        {
            std::vector<float> got;
            for (int64_t done = 0; done < (int64_t) (seconds * kSr); done += kBlock)
            {
                const auto at = (int64_t) tape.position();
                for (int i = 0; i < kBlock; ++i)
                    inL[(size_t) i] = inR[(size_t) i] = in ? in (at + i) : 0.0f;
                tape.process (kBlock, inL.data(), inR.data(), out);
                const int t = listen >= 0 ? listen : tape.track;
                got.insert (got.end(), buf[(size_t) t][0].begin(), buf[(size_t) t][0].end());
            }
            return got;
        }
        static float peakOf (const std::vector<float>& v, size_t from = 0, size_t to = SIZE_MAX)
        {
            float p = 0;
            for (size_t i = from; i < std::min (to, v.size()); ++i)
                p = std::max (p, std::abs (v[i]));
            return p;
        }
    };
    float tone (int64_t i) { return sine (i, 330.0f, 0.5f); }
} // namespace

class TapeTests : public juce::UnitTest
{
public:
    TapeTests() : juce::UnitTest ("tape", "keytape") {}

    void runTest() override
    {
        beginTest ("record a take, play it back");
        {
            Deck d;
            d.tape.recLevel = 1.0f;
            d.tape.record();
            expect (d.tape.isRecording() && d.tape.isPlaying());
            d.run (1.0, tone);
            d.tape.stop();
            expectEquals ((int) d.tape.reel.tracks[0].takes.size(), 1);
            const auto& take = d.tape.reel.tracks[0].takes[0];
            expectEquals (take.start, (int64_t) 0);
            expectWithinAbsoluteError ((double) take.length, kSr, (double) kBlock * 2);
            d.tape.locate (0);
            d.tape.play();
            const auto back = d.run (1.2);
            expectWithinAbsoluteError (Deck::peakOf (back, 1000, 40000), 0.5f, 0.02f);
            expect (Deck::peakOf (back, 50000) < 0.001f, "silence after the take");
            expect (! d.tape.reel.empty() && d.tape.reel.lastEnd() == take.end());
        }

        beginTest ("overdub: a second pass keeps the first");
        {
            Deck d;
            d.tape.recLevel = 1.0f;
            d.tape.record();
            d.run (1.0, [] (int64_t i) { return sine (i, 200.0f, 0.3f); });
            d.tape.stop();
            d.tape.locate (0);
            d.tape.record();
            d.run (1.0, [] (int64_t i) { return sine (i, 200.0f, 0.3f); });
            d.tape.stop();
            d.tape.locate (0);
            d.tape.play();
            const auto back = d.run (0.9);
            expectWithinAbsoluteError (Deck::peakOf (back, 2000), 0.6f, 0.03f);
            expectEquals ((int) d.tape.reel.tracks[0].takes.size(), 1, "the overdub replaces what it covers");
        }

        beginTest ("tracks are separate");
        {
            Deck d;
            d.tape.track = 2;
            d.tape.record();
            d.run (0.5, tone);
            d.tape.stop();
            expect (d.tape.reel.tracks[0].takes.empty() && d.tape.reel.tracks[2].takes.size() == 1);
            d.tape.locate (0);
            d.tape.play();
            expect (Deck::peakOf (d.run (0.4, {}, 0)) < 1.0e-6f);
            d.tape.locate (0);
            expect (Deck::peakOf (d.run (0.4, {}, 2)) > 0.3f);
        }

        beginTest ("speed and reverse");
        {
            Deck d;
            d.tape.play();
            d.run (1.0);
            expectWithinAbsoluteError (d.tape.position(), kSr, (double) kBlock);
            d.tape.speed = 0.5;
            d.run (1.0);
            expectWithinAbsoluteError (d.tape.position(), kSr * 1.5, (double) kBlock);
            d.tape.speed = 1.0;
            d.tape.setReversePlay (true);
            d.run (1.0);
            expectWithinAbsoluteError (d.tape.position(), kSr * 0.5, (double) kBlock);
            d.tape.setReversePlay (false);
            d.tape.setTrickReverse (true);
            d.run (0.25);
            expectWithinAbsoluteError (d.tape.position(), kSr * 0.25, (double) kBlock);
            d.tape.setTrickReverse (false);
            d.run (1.0);
            expect (d.tape.isPlaying());
            d.tape.setReversePlay (true);
            d.run (3.0);
            expect (! d.tape.isPlaying() && d.tape.position() == 0.0, "reverse stops at the start");
        }

        beginTest ("the slower the tape when recording, the lower and longer it plays at normal speed");
        {
            Deck d;
            d.tape.speed = 0.5;
            d.tape.recLevel = 1.0f;
            d.tape.record();
            d.run (1.0, tone);
            d.tape.stop();
            const auto& take = d.tape.reel.tracks[0].takes[0];
            expectWithinAbsoluteError ((double) take.length, kSr * 0.5, (double) kBlock * 2);
            d.tape.speed = 2.0;
            d.tape.record();
            d.run (0.5, tone);
            d.tape.stop();
            expectWithinAbsoluteError ((double) d.tape.reel.lastEnd(), kSr * 1.5, (double) kBlock * 4);
        }

        beginTest ("loop: in, out, on; the head goes round; a loop builds up layers when recording");
        {
            Deck d;
            d.tape.gridResolution = 0;
            d.tape.locate ((int64_t) (kSr * 1.0));
            d.tape.setLoopIn();
            d.tape.locate ((int64_t) (kSr * 1.5));
            d.tape.setLoopOut();
            d.tape.toggleLoop();
            expect (d.tape.reel.loopOn);
            d.tape.locate ((int64_t) (kSr * 1.0));
            d.tape.play();
            d.run (2.0);
            expect (d.tape.position() >= kSr * 1.0 && d.tape.position() < kSr * 1.5, "stays in the loop");
            d.tape.stop();
            d.tape.locate ((int64_t) (kSr * 1.0));
            d.tape.recLevel = 1.0f;
            d.tape.record();
            d.run (1.0, [] (int64_t i) { return sine (i, 100.0f, 0.2f); }); // two passes of the same tone in phase
            d.tape.stop();
            d.tape.locate ((int64_t) (kSr * 1.0));
            d.tape.play();
            const auto back = d.run (0.4);
            expect (Deck::peakOf (back) > 0.3f, "two passes are louder than one: " + juce::String (Deck::peakOf (back)));
            d.tape.toggleLoop();
            expect (! d.tape.reel.loopOn);
            // the grid snaps loop points to bars
            d.tape.reel.loopIn = d.tape.reel.loopOut = -1;
            d.tape.gridResolution = 1;
            d.tape.locate ((int64_t) (d.tape.barLength() * 1.1));
            d.tape.setLoopIn();
            expectEquals (d.tape.reel.loopIn, d.tape.barLength());
            d.tape.locate (d.tape.barLength() * 3);
            d.tape.setLoopOut();
            d.tape.moveLoopByBar (1);
            expectEquals (d.tape.reel.loopIn, 2 * d.tape.barLength());
        }

        beginTest ("break slows the tape and comes back in time; chop repeats a slice");
        {
            Deck d;
            d.tape.play();
            d.run (0.5);
            d.tape.setBreak (true);
            const double before = d.tape.position();
            d.run (1.0);
            const double moved = d.tape.position() - before;
            expect (moved < kSr * 0.9 && moved > 0, "slowed: " + juce::String (moved));
            d.tape.setBreak (false);
            expectWithinAbsoluteError (d.tape.position(), kSr * 1.5, (double) kBlock * 2);
            d.tape.setChop (true, 120.0);
            const double start = d.tape.position();
            d.run (1.0);
            expect (d.tape.position() >= start && d.tape.position() < start + kSr * 0.126, "chop stays in its sixteenth");
            d.tape.setChop (false, 120.0);
            expectWithinAbsoluteError (d.tape.position(), start + kSr, (double) kBlock * 2);
        }

        beginTest ("lift, drop, split, join, slide");
        {
            Deck d;
            d.tape.recLevel = 1.0f;
            d.tape.record();
            d.run (1.0, tone);
            d.tape.stop();
            d.tape.locate ((int64_t) (kSr * 0.5));
            expect (d.tape.split());
            expectEquals ((int) d.tape.reel.tracks[0].takes.size(), 2);
            expect (d.tape.join());
            expectEquals ((int) d.tape.reel.tracks[0].takes.size(), 1);
            expect (d.tape.lift());
            expect (d.tape.reel.tracks[0].takes.empty() && d.tape.clipboard[0].has_value());
            d.tape.locate ((int64_t) (kSr * 2.0));
            expect (d.tape.drop());
            expectEquals (d.tape.reel.tracks[0].takes[0].start, (int64_t) (kSr * 2.0));
            expectWithinAbsoluteError (d.tape.position(), kSr * 2.0 + (double) d.tape.reel.tracks[0].takes[0].length, 2.0);
            expect (d.tape.drop(), "drop again lays another copy end to end");
            expectEquals ((int) d.tape.reel.tracks[0].takes.size(), 2);
            expect (d.tape.clipboardMix() != nullptr);
            // slide moves the take under the head
            d.tape.locate ((int64_t) (kSr * 2.2));
            const auto start = d.tape.reel.tracks[0].takes[0].start;
            d.tape.slide (-5);
            expect (d.tape.reel.tracks[0].takes[0].start < start);
            // lift all takes the four tracks
            d.tape.track = 1;
            d.tape.locate (0);
            d.tape.record();
            d.run (3.0, tone);
            d.tape.stop();
            d.tape.locate ((int64_t) (kSr * 2.2));
            expect (d.tape.liftAll());
            expect (d.tape.clipboardAll);
            // region lift: with the loop set, lift takes just the loop's stretch
            d.tape.gridResolution = 0;
            d.tape.track = 1;
            d.tape.locate ((int64_t) (kSr * 0.5));
            d.tape.setLoopIn();
            d.tape.locate ((int64_t) (kSr * 1.0));
            d.tape.setLoopOut();
            expect (d.tape.lift());
            expectWithinAbsoluteError ((double) d.tape.clipboard[1]->length, kSr * 0.5, 2.0);
            d.tape.eraseAll();
            expect (d.tape.reel.empty());
        }

        beginTest ("jump by bars; armed recording moves only when scrubbed");
        {
            Deck d;
            d.tape.jumpBar (1);
            expectEquals ((int64_t) d.tape.position(), d.tape.barLength());
            d.tape.jumpBar (1);
            d.tape.jumpBar (-1);
            expectEquals ((int64_t) d.tape.position(), d.tape.barLength());
            d.tape.locate (0);
            d.tape.arm();
            expect (d.tape.isArmed() && ! d.tape.isPlaying());
            d.run (0.5, tone);
            expectEquals (d.tape.position(), 0.0);
            d.tape.scrub (10);
            d.run (0.5, tone);
            expect (d.tape.position() > 0.0);
            d.tape.stop();
            expectEquals ((int) d.tape.reel.tracks[0].takes.size(), 1, "the scrubbed stretch was recorded");
        }

        beginTest ("a sound saved to tape as data");
        {
            Deck d;
            auto s = factorySynthPresets (Engine::Fm).front().sound;
            d.tape.dropSoundData (soundToJson (s), (int64_t) kSr / 2);
            d.tape.locate (100);
            const auto* take = d.tape.activeTake();
            expect (take != nullptr && take->soundData.isObject());
        }
    }
};

static TapeTests tapeTests;
} // namespace keytape::test
