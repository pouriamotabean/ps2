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

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};
