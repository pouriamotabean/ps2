#pragma once
#include <JuceHeader.h>

// Shared palette — near-black "boutique plugin" look (think Baby Audio):
// almost-black background, a single glowing mint/teal accent that carries
// the selected state and the verdict, warm amber for section labels.
namespace PSColours
{
    inline const juce::Colour bg       (0xff05090a);
    inline const juce::Colour panel    (0xff0a1211);
    inline const juce::Colour raised   (0xff0e1a18);
    inline const juce::Colour raised2  (0xff152523);
    inline const juce::Colour border   (0xff203230);
    inline const juce::Colour text     (0xffeaf4f0);
    inline const juce::Colour textDim  (0xff7d948d);
    inline const juce::Colour accent   (0xff1f9d7c);
    inline const juce::Colour accentHi (0xff4be3b5);
    inline const juce::Colour gold     (0xffe0a53a);
    inline const juce::Colour good     (0xff4be3b5);
    inline const juce::Colour warn     (0xffe0a53a);
    inline const juce::Colour bad      (0xffe0607a);
}
