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

    // Procedurally-generated grain, not a baked image, so it stays a crisp
    // per-pixel pattern at any scale the ScaleHost applies rather than
    // blurring like a stretched bitmap would. Regenerated only when the
    // component's own size actually changes.
    void generateNoiseImage();
    juce::Image noiseImage;

    PSLookAndFeel lookAndFeel;

    // --- Header -------------------------------------------------------
    // Icon mark, then the big title next to it, with a small tracked
    // caption underneath -- matches the user-supplied design spec's header
    // (icon + "Predict Spotify" + "MASTER . ANALYZE . STREAM" beneath).
    // The icon is drawn directly in paint() (see headerIconBounds) rather
    // than as a juce::ImageComponent -- the old one loaded a baked PNG,
    // which visibly blurred under the window's continuous AffineTransform
    // scaling; a code-drawn mark stays pixel-crisp at any size, same as
    // every other icon/glyph in this app (PSSkin::drawIcon and friends).
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Rectangle<int> headerIconBounds;      // the "PS" mark, top-left -- drawn directly in paint()
    juce::Rectangle<int> headerRightIconBounds; // small waveform glyph, top-right -- drawn directly in paint()
    juce::Rectangle<int> headerDividerBounds;   // full-width line under the header -- drawn directly in paint()
    juce::Rectangle<int> leftCardBounds;        // the left column's own card background -- drawn directly in paint()

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
    // juce::String(const char*) does NOT assume UTF-8 (confirmed against
    // JUCE source -- it explicitly documents CharPointer_UTF8 as required
    // for extended characters), which is what caused the "Â·" mojibake;
    // wrapping the literal this way is the fix.
    juce::Label waveformSectionLabel { {}, juce::String (juce::CharPointer_UTF8 ("WAVEFORM \xc2\xb7 BEFORE / AFTER")) };
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
