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

    // Moves the playhead line to a 0..1 fraction of the track's length, or
    // pass a negative value to hide it. Called continuously during
    // playback so the line actually tracks the transport.
    void setPlayheadPosition (float normalized);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    // Fired with a 0..1 fraction of the track's length whenever the user
    // clicks or drags on the waveform, so playback can jump there right
    // away instead of waiting for the transport to reach that point.
    std::function<void (float)> onSeek;

private:
    void seekFromMouse (const juce::MouseEvent& e);
    std::vector<float> inputMin, inputMax, outputMin, outputMax;
    bool hasData = false;
    float playheadPosition = -1.0f;

    void drawStrip (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                     const std::vector<float>& mn, const std::vector<float>& mx, juce::Colour colour) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
