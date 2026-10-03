#pragma once
#include <JuceHeader.h>

// Two stacked overview waveforms (min/max peaks per column): original on
// top, processed below, at matching horizontal scale so a listener can
// see where levels or transients changed.
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();

    void setData (const std::vector<float>& inMin, const std::vector<float>& inMax,
                  const std::vector<float>& outMin, const std::vector<float>& outMax);
    void clear();

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> inputMin, inputMax, outputMin, outputMax;
    bool hasData = false;

    void drawStrip (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                     const std::vector<float>& mn, const std::vector<float>& mx, juce::Colour colour) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
