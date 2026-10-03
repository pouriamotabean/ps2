#pragma once
#include <JuceHeader.h>
#include "SpotifyProcessor.h"
#include "SpectrumDisplay.h"

// Everything numeric lives here: the LUFS / sample-peak / true-peak table,
// stereo field table, target/gain/limiter line, file info, and the
// spectrum graph. This whole panel is hidden behind a collapsible toggle
// in MainComponent -- by default the app only shows the plain-language
// verdict and the waveform, and a curious/technical user can expand this
// to see the numbers behind it.
class DetailsPanel : public juce::Component
{
public:
    DetailsPanel();

    void setReport (const SpotifyProcessor::Report& report);
    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;

    // reduced(18,12) margins + text block + "SPECTRUM" label row + spectrum graph
    static constexpr int kTextBlockHeight = 270; // file info + target/gain/limiter + tables + diff note
    static constexpr int kPreferredHeight = 12 + kTextBlockHeight + 16 + 160 + 12;

private:
    SpotifyProcessor::Report report;
    bool hasReport = false;
    SpectrumDisplay spectrumDisplay;

    void drawRow (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                  const juce::String& lufs, const juce::String& sp, const juce::String& tp,
                  bool bold, juce::Colour labelColour) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DetailsPanel)
};
