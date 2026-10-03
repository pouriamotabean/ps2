#pragma once
#include <JuceHeader.h>

// Shared palette — near-black "boutique plugin" look (think Baby Audio):
// almost-black background, a single glowing cyan/mint accent that carries
// the selected state, section labels and the verdict. This revision matches
// the user-supplied pixel/colour spec ("PREDICT SPOTIFY -- UI DESIGN SPEC"):
// a cooler blue-graphite base (was a flatter near-black) and a slightly
// more restrained teal accent (was a punchier cyan); warn/bad stay amber/
// red since those carry real severity meaning, not decoration.
namespace PSColours
{
    inline const juce::Colour bg        (0xff081012);
    inline const juce::Colour panel     (0xff0b1618);
    inline const juce::Colour raised    (0xff0d1a1c);
    inline const juce::Colour raised2   (0xff132224);
    inline const juce::Colour border    (0xff1d3032);
    inline const juce::Colour text      (0xffe7eef0);
    inline const juce::Colour textDim   (0xff87989c);
    inline const juce::Colour textDimmer(0xff526164); // disabled-level contrast, per spec
    inline const juce::Colour accent    (0xff20d7c3);
    inline const juce::Colour accentHi  (0xff2be2cf);
    inline const juce::Colour gold      (0xff2be2cf); // decorative section-label teal (not a severity colour -- see warn)
    inline const juce::Colour good      (0xff2be2cf);
    inline const juce::Colour warn      (0xffe0a53a);
    inline const juce::Colour bad       (0xffe0607a);
}
