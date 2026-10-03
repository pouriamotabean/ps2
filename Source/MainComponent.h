#pragma once
#include <JuceHeader.h>
#include "SpotifyProcessor.h"
#include "SegmentedControl.h"
#include "ResultsPanel.h"
#include "DetailsPanel.h"
#include "PSLookAndFeel.h"
#include "AudioPlayer.h"
#include "WaveformDisplay.h"

class MainComponent : public juce::Component,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Called whenever this component's own ideal ("native") size changes
    // (e.g. the verdict grew another line, or "More details" was toggled).
    // The host window uses this to resize itself and keep everything
    // scaled together rather than reflowing or clipping.
    std::function<void()> onNativeSizeChanged;

private:
    // Drives the moving playhead on the waveform during playback (see
    // AudioPlayer::getNormalizedPosition()). Cheap no-op when nothing is
    // loaded or playing.
    void timerCallback() override;

    void chooseInputFile();
    void loadInputFile (const juce::File& file);
    void runProcessing();
    void chooseSaveLocation();
    void showReport (const SpotifyProcessor::Report& report, const juce::File& outFile);
    void toggleDetails();
    void updateHeight();

    PSLookAndFeel lookAndFeel;

    // --- Header -------------------------------------------------------
    juce::ImageComponent titleLogo;
    juce::Label subtitleLabel;

    // --- Input ----------------------------------------------------------
    juce::TextButton loadButton { "Choose WAV..." };
    juce::Label inputFileLabel { {}, "No file selected" };

    // --- Target loudness ------------------------------------------------
    juce::Label targetSectionLabel { {}, "SPOTIFY LOUDNESS TARGET" };
    SegmentedControl targetControl;

    // --- Quality tier -----------------------------------------------------
    juce::Label qualitySectionLabel { {}, "STREAMING QUALITY" };
    SegmentedControl qualityControl;

    // --- Action / verdict ---------------------------------------------------
    // Process runs immediately (no save dialog up front) and writes the
    // result to a temp file; Save As... (enabled once that succeeds) is
    // how the user actually keeps/moves the output.
    juce::TextButton processButton { "Process" };
    juce::TextButton saveAsButton { "Save As..." };
    ResultsPanel resultsPanel;
    juce::Label statusLabel;

    // --- Waveform (always visible - this is the one visual the minimal
    // view keeps front and center) ------------------------------------------
    juce::Label waveformSectionLabel { {}, "WAVEFORM - BEFORE / AFTER" };
    WaveformDisplay waveformDisplay;

    // --- A/B playback -------------------------------------------------
    juce::Label abSectionLabel { {}, "A/B LISTEN (LEVEL-MATCHED)" };
    AudioPlayer audioPlayer;

    // --- Collapsible technical details (all the numbers + spectrum graph).
    // The toggle itself is a small link sitting right next to the waveform
    // header, closed by default -- everything numeric is one click away
    // without cluttering the default view.
    juce::TextButton detailsToggleButton { "More details  >" };
    DetailsPanel detailsPanel;
    bool detailsExpanded = false;

    juce::Label creditLabel { {}, "by Pouria Motabean" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::File inputFile;
    juce::File lastOutputFile; // set once a run succeeds; Save As... copies from here
    std::atomic<bool> isProcessing { false };
    bool isDragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
