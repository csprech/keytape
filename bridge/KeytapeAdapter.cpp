// Keytape's side of the GPUI bridge: its panel's parts, and what makes each look different.

#include "PanelBridge.h"
#include "../src/engine/Engine.h"
#include "../src/ui/Panel.h"

namespace keytape::ui
{
using panelbridge::mix;
using panelbridge::mixFloat;
using panelbridge::mixString;

struct GpuiAdapter : panelbridge::Adapter
{
    std::unique_ptr<AudioEngine> engine = std::make_unique<AudioEngine>();
    std::unique_ptr<Panel> panelComp = std::make_unique<Panel> (*engine);

    ~GpuiAdapter() override
    {
        Panel::keyHook = nullptr;
        panelComp.reset();
        engine.reset();
    }

    juce::String title() override { return "Keytape KT-4"; }
    juce::Component& panel() override { return *panelComp; }
    void setKeyHook (std::function<bool (int)> f) override { Panel::keyHook = std::move (f); }
    float outputLevel() override { return engine->getOutputLevel(); }
    void testSound (bool start) override
    {
        engine->withMachine ([start] (Machine& m) {
            if (start)
                m.keyDown (12, 0.8f);
            else
                m.keyUp (12);
        });
    }

    std::vector<std::pair<juce::String, juce::Component*>> parts() override
    {
        std::vector<std::pair<juce::String, juce::Component*>> p;
        for (auto* c : panelComp->getChildren())
        {
            juce::String kind = "static";
            if (dynamic_cast<CellKey*> (c) != nullptr || dynamic_cast<PianoKey*> (c) != nullptr)
                kind = "key";
            else if (dynamic_cast<Encoder*> (c) != nullptr || dynamic_cast<VolumeKnob*> (c) != nullptr)
                kind = "drag";
            else if (dynamic_cast<DisplayView*> (c) != nullptr || dynamic_cast<LevelMeter*> (c) != nullptr)
                kind = "display";
            p.push_back ({ kind, c });
        }
        return p;
    }

    bool live (juce::Component& c) override { return dynamic_cast<DisplayView*> (&c) != nullptr; }

    uint64_t custom (juce::Component& c) override
    {
        uint64_t h = 13;
        if (auto* k = dynamic_cast<CellKey*> (&c))
            return mix (mix (h, k->mouseHeld), k->shownDown);
        if (auto* k = dynamic_cast<PianoKey*> (&c))
            return mix (mix (h, k->mouseHeld), k->shownDown);
        if (auto* e = dynamic_cast<Encoder*> (&c))
            return mixFloat (mix (h, e->pressed), e->angle);
        if (auto* v = dynamic_cast<VolumeKnob*> (&c))
            return mixFloat (h, v->get ? v->get() : 0.0f);
        if (auto* m = dynamic_cast<LevelMeter*> (&c))
            return mix (mixFloat (h, m->level, 0.005f), m->battery);
        if (auto* d = dynamic_cast<DisplayView*> (&c))
        {
            // what the screen shows that changes from moment to moment (the rest is redrawn ten times a second)
            const auto& s = d->getState();
            h = mixString (mixString (mixString (mixString (h, s.title), s.popup), s.help), s.subtitle);
            h = mix (mix (mix (h, (uint64_t) s.page), (uint64_t) s.mode), s.shift);
            for (auto& p : s.params)
                h = mixFloat (mixString (h, p.text), p.value, 0.002f);
            for (float v : s.scope)
                h = mixFloat (h, v, 0.01f);
            h = mixFloat (mixFloat (h, (float) s.head, 0.02f), (float) s.beatPos, 0.05f);
            for (bool k : s.keysDown)
                h = mix (h, k);
            return mix (mixFloat (h, s.meter, 0.01f), (uint64_t) s.takes.size());
        }
        return 0;
    }
};
} // namespace keytape::ui

std::unique_ptr<panelbridge::Adapter> panelbridge::makeAdapter() { return std::make_unique<keytape::ui::GpuiAdapter>(); }
