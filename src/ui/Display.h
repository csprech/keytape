#pragma once

// The colour screen. Keytape's own screens, drawn from the machine's description on a 320 x 160 grid: each shows
// what its sound does (a live trace, or the shape the parameters draw) with the four encoders' parameters along
// the bottom in their colours (blue, ochre, gray, orange).

#include "../core/Machine.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace keytape::ui
{
/** Paints the screen into `area` (keeps 2:1, black around it). */
void paintDisplay (juce::Graphics& g, juce::Rectangle<float> area, const DisplayState& d);

/** The built-in guide's pages (shift + help): titles and text. */
const juce::StringArray& guideTitles();
juce::String guideText (int page);

class DisplayView : public juce::Component
{
public:
    void setState (const DisplayState& s)
    {
        state = s;
        repaint();
    }
    const DisplayState& getState() const { return state; }
    void paint (juce::Graphics& g) override;

private:
    DisplayState state;
};
} // namespace keytape::ui
