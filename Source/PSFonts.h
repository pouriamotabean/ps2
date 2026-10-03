#pragma once
#include <JuceHeader.h>

// Custom typefaces for the whole app (compiled in via BinaryData, see
// CMakeLists.txt's PSBinaryData target):
//  - Inter for UI text, labels and headers.
//  - JetBrains Mono (true regular + bold weights, not synthetic) for
//    anything numeric -- LUFS/dB/percentage readouts -- so columns of
//    numbers actually line up instead of each digit having a different
//    width, the way a normal proportional font would render them.
// Both are open-source (SIL Open Font License) and safe to redistribute.
// If a typeface ever fails to load, every helper here falls back to the
// platform default font, so the UI never breaks over a missing asset.
namespace PSFonts
{
    juce::Font ui (float size, bool bold = false);
    juce::Font mono (float size, bool bold = false);
}
