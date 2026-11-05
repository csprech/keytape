// The files: tapes written and read back (takes, sounds on tape, loop, style), snapshots, mixdowns, the radio's
// stations, and sounds with their samples in the saved state.

#include "Rig.h"
#include "engine/Folder.h"

namespace keytape::test
{
class FolderTests : public juce::UnitTest
{
public:
    FolderTests() : juce::UnitTest ("files", "keytape") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("keytape-folder-test-" + juce::String (juce::Random::getSystemRandom().nextInt()));
        root.deleteRecursively();

        beginTest ("a tape goes to its WAVs and comes back take for take");
        {
            Folder f (root);
            f.setSampleRate (kSr);
            Reel reel;
            reel.name = "song 3";
            reel.style = TapeStyle::Cassette;
            reel.loopIn = 1000;
            reel.loopOut = 50000;
            reel.loopOn = true;
            reel.bpmAtNormal = 96.0;
            auto audio = std::make_shared<Audio>();
            audio->sampleRate = kSr;
            audio->data.setSize (2, 24000);
            for (int i = 0; i < 24000; ++i)
            {
                audio->data.setSample (0, i, sine (i, 300.0f, 0.4f));
                audio->data.setSample (1, i, sine (i, 450.0f, 0.3f));
            }
            Take a;
            a.audio = audio;
            a.start = 4800;
            a.length = 24000;
            Take b = a;
            b.start = 60000;
            b.offset = 12000;
            b.length = 12000;
            reel.tracks[1].takes = { a, b };
            Take s;
            s.audio = audio;
            s.start = 0;
            s.length = 100;
            s.soundData = soundToJson (factorySynthPresets (Engine::Fm).front().sound);
            reel.tracks[3].takes = { s };
            f.saveTape (2, reel);
            f.flush();
            expect (root.getChildFile ("tapes/tape 3/track 2.wav").existsAsFile());
            expect (! root.getChildFile ("tapes/tape 3/track 1.wav").existsAsFile(), "an empty track has no file");
            const auto back = f.loadTape (2);
            expectEquals (back.name, juce::String ("song 3"));
            expect (back.style == TapeStyle::Cassette && back.loopOn && back.loopIn == 1000 && back.loopOut == 50000);
            expectWithinAbsoluteError (back.bpmAtNormal, 96.0, 1.0e-9);
            expectEquals ((int) back.tracks[1].takes.size(), 2);
            expectEquals ((int) back.tracks[3].takes.size(), 1);
            expect (back.tracks[3].takes[0].soundData.isObject(), "the sound on tape came back");
            // the audio is the same where the takes are
            int hint = -1;
            float err = 0.0f;
            for (int i = 0; i < 24000; i += 97)
                err = std::max (err, std::abs (back.tracks[1].read (4800.0 + i, 0, hint) - sine (i, 300.0f, 0.4f)));
            for (int i = 0; i < 12000; i += 97)
                err = std::max (err, std::abs (back.tracks[1].read (60000.0 + i, 1, hint) - sine (12000 + i, 450.0f, 0.3f)));
            expect (err < 1.0e-5f, "the takes' audio: " + juce::String (err));
            expect (f.loadTape (5).empty(), "a tape never saved is empty");
        }

        beginTest ("snapshots, mixdowns, and sounds with their samples");
        {
            Folder f (root);
            auto s = factorySynthPresets (Engine::Sampler).front().sound;
            auto a = std::make_shared<Audio>();
            a->data.setSize (2, 4800);
            for (int i = 0; i < 4800; ++i)
                a->data.setSample (0, i, sine (i)), a->data.setSample (1, i, sine (i));
            s.sample = a;
            s.eng.p[2] = 0.42f;
            f.saveSnapshot (false, &s, nullptr);
            f.saveSnapshot (false, &s, nullptr);
            const auto names = f.snapshots (false);
            expectEquals (names.size(), 2);
            SynthSound back;
            expect (f.loadSnapshot (false, names[1], &back, nullptr));
            expectWithinAbsoluteError (back.eng.p[2], 0.42f, 1.0e-6f);
            expect (back.sample != nullptr && back.sample->length() == 4800, "the sample came with it");
            DrumKit k = factoryKits (DrumEngine::Kit)[1].kit;
            f.saveSnapshot (true, nullptr, &k);
            expectEquals (f.snapshots (true).size(), 1);
            juce::AudioBuffer<float> mix (2, 9600);
            mix.clear();
            f.saveMixdown (1, mix, 4800, kSr);
            f.flush();
            const auto mixes = root.getChildFile ("mixdowns").findChildFiles (juce::File::findFiles, false, "side b*.wav");
            expectEquals (mixes.size(), 1);
            if (mixes.size() == 1)
            {
                const auto m = f.readAudio (mixes[0]);
                expect (m != nullptr && m->length() == 4800);
            }
        }

        beginTest ("the radio folder's files become stations across the band");
        {
            Folder f (root);
            for (int i = 0; i < 3; ++i)
            {
                juce::AudioBuffer<float> b (2, 4800);
                for (int n = 0; n < 4800; ++n)
                    b.setSample (0, n, sine (n, 200.0f + 100.0f * i)), b.setSample (1, n, b.getSample (0, n));
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> os (root.getChildFile ("radio/station " + juce::String (i + 1) + ".wav").createOutputStream().release());
                auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (kSr).withNumChannels (2).withBitsPerSample (16));
                w->writeFromAudioSampleBuffer (b, 0, 4800);
            }
            const auto st = f.loadStations();
            expectEquals ((int) st.size(), 3);
            if (st.size() == 3)
            {
                expect (st[0].mhz >= 87.5 && st[2].mhz <= 108.0 && st[0].mhz < st[1].mhz && st[1].mhz < st[2].mhz);
                expectEquals (st[0].name, juce::String ("station 1"));
                // tuned to one, the radio plays it
                Rig r;
                r.m.stations = st;
                r.m.input.source = Input::Radio;
                r.m.input.fmMHz = st[1].mhz;
                r.m.input.on = true;
                r.tapKey (Key::Tape);
                expect (r.run (0.3) > 0.1f, "the station is heard");
            }
        }
        root.deleteRecursively();
    }
};

static FolderTests folderTests;
} // namespace keytape::test
