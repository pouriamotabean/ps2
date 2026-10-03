#pragma once
#include <JuceHeader.h>
#include "SpotifyProcessor.h"

// The one thing shown by default after processing: a plain-language
// verdict ("did I master this right for Spotify?") with a flag icon.
// Every number behind this lives in DetailsPanel instead, collapsed by
// default, so the main view stays minimal.
class ResultsPanel : public juce::Component
{
public:
    ResultsPanel();

    void setPlaceholder (const juce::String& text);
    void setReport (const SpotifyProcessor::Report& report);

    // How tall this panel needs to be to show its current content without
    // clipping (depends on placeholder / error / verdict line count).
    int getPreferredHeight() const;

    void paint (juce::Graphics& g) override;

private:
    bool hasReport = false;
    bool isError = false;
    int verdictLineCount = 1;
    juce::String placeholderText { "Load a WAV and press Process to see the verdict here." };
    SpotifyProcessor::Report report;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResultsPanel)
};
