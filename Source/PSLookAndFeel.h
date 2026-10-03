#pragma once
#include <JuceHeader.h>

// Premium button treatment shared by the whole PQ/PD/PV/PS family:
// soft vertical gradient, subtle top highlight, subtle bottom shadow line.
class PSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PSLookAndFeel();

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Draws a small leading icon for a few recognised buttons (by their
    // exact button text -- "Choose WAV...", "Process", "Save As..."),
    // then the label text next to it; every other button falls back to
    // plain centred text exactly as LookAndFeel_V2 would draw it.
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
