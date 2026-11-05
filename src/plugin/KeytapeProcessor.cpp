#include "KeytapeProcessor.h"
#include "../ui/Panel.h"

namespace keytape
{
namespace
{
    class Editor : public juce::AudioProcessorEditor
    {
    public:
        explicit Editor (KeytapeProcessor& p) : juce::AudioProcessorEditor (p), panel (p.getEngine())
        {
            addAndMakeVisible (panel);
            setSize (ui::Panel::kWidth, ui::Panel::kHeight);
        }
        void resized() override
        {
            const float sc = std::min (getWidth() / (float) ui::Panel::kWidth, getHeight() / (float) ui::Panel::kHeight);
            const float ox = (getWidth() - ui::Panel::kWidth * sc) * 0.5f, oy = (getHeight() - ui::Panel::kHeight * sc) * 0.5f;
            panel.setTransform (juce::AffineTransform::scale (sc).translated (ox, oy));
        }
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0d0d0e)); }

    private:
        ui::Panel panel;
    };
} // namespace

KeytapeProcessor::KeytapeProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Inputs", juce::AudioChannelSet::discreteChannels (kNumInputs), true)
                                .withOutput ("Outputs", juce::AudioChannelSet::stereo(), true))
{
    for (auto& p : plugged)
        p = false;
}

KeytapeProcessor::~KeytapeProcessor() = default;

void KeytapeProcessor::setInputPlugged (int channel, bool p)
{
    if (channel >= 0 && channel < kNumInputs)
        plugged[(size_t) channel] = p;
}

bool KeytapeProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainOutputChannelSet().size() == kNumOutputs
           && (layout.getMainInputChannelSet().isDisabled() || layout.getMainInputChannelSet().size() == kNumInputs);
}

void KeytapeProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    inputCopy.setSize (kNumInputs, juce::jmax (1, maxBlock));
    engine.prepareHosted (sampleRate, maxBlock);
}

void KeytapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    const int numIn = juce::jmin (kNumInputs, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n > inputCopy.getNumSamples())
        inputCopy.setSize (kNumInputs, n, false, false, true);
    // the inputs share the buffer with the outputs: copy them first
    for (int c = 0; c < numIn; ++c)
        inputCopy.copyFrom (c, 0, buffer, c, 0, n);
    const float* in[kNumInputs] {};
    for (int c = 0; c < kNumInputs; ++c)
        in[c] = c < numIn && plugged[(size_t) c] ? inputCopy.getReadPointer (c) : nullptr;
    // (a pair with only its left jack is mono)
    if (in[1] == nullptr)
        in[1] = in[0];
    if (in[3] == nullptr)
        in[3] = in[2];
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    float* outs[2] { buffer.getNumChannels() > 0 ? buffer.getWritePointer (0) : nullptr, buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr };
    if (outs[0] == nullptr || outs[1] == nullptr)
        return;
    engine.processHosted (in, outs, n, midi);
}

juce::AudioProcessorEditor* KeytapeProcessor::createEditor() { return new Editor (*this); }

void KeytapeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = engine.getState();
    juce::MemoryOutputStream mos (dest, false);
    mos.writeString (juce::JSON::toString (state, true));
}

void KeytapeProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream mis (data, (size_t) size, false);
    auto json = juce::JSON::parse (mis.readEntireStreamAsString());
    if (json.getProperty ("format", {}).toString() != "keytape-state")
        return;
    engine.setState (json);
}
} // namespace keytape
