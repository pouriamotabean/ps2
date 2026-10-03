#include "MainComponent.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

MainComponent::MainComponent()
    : targetControl ({ { "Loud", "-11 LUFS" }, { "Normal", "-14 LUFS" }, { "Quiet", "-19 LUFS" } }),
      qualityControl ({ { "Low", "24 kbps" }, { "Normal", "96 kbps" }, { "High", "160 kbps" }, { "V.High", "320 kbps" } })
{
    setLookAndFeel (&lookAndFeel);
    setSize (960, 600); // final size recomputed by updateHeight() below

    titleLogo.setImage (PSSkin::logoImage());
    titleLogo.setImagePlacement (juce::RectanglePlacement (juce::RectanglePlacement::xLeft
                                                             | juce::RectanglePlacement::yMid
                                                             | juce::RectanglePlacement::onlyReduceInSize));
    addAndMakeVisible (titleLogo);

    subtitleLabel.setText ("Predict Spotify", juce::dontSendNotification);
    subtitleLabel.setFont (PSFonts::ui (15.0f, false));
    subtitleLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    addAndMakeVisible (subtitleLabel);

    loadButton.onClick = [this] { chooseInputFile(); };
    addAndMakeVisible (loadButton);

    inputFileLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    inputFileLabel.setJustificationType (juce::Justification::centredLeft);
    inputFileLabel.setFont (PSFonts::ui (14.0f, false));
    addAndMakeVisible (inputFileLabel);

    auto setupSectionLabel = [] (juce::Label& l)
    {
        l.setFont (PSFonts::ui (12.0f, true));
        l.setColour (juce::Label::textColourId, PSColours::gold);
    };
    setupSectionLabel (targetSectionLabel);
    setupSectionLabel (qualitySectionLabel);
    setupSectionLabel (abSectionLabel);
    setupSectionLabel (waveformSectionLabel);
    addAndMakeVisible (targetSectionLabel);
    addAndMakeVisible (qualitySectionLabel);
    addAndMakeVisible (abSectionLabel);
    addAndMakeVisible (waveformSectionLabel);

    targetControl.setSelectedIndex (1, juce::dontSendNotification);  // Normal
    qualityControl.setSelectedIndex (3, juce::dontSendNotification); // Very High
    addAndMakeVisible (targetControl);
    addAndMakeVisible (qualityControl);

    processButton.onClick = [this] { runProcessing(); };
    processButton.setColour (juce::TextButton::buttonColourId, PSColours::accent);
    processButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (processButton);

    saveAsButton.onClick = [this] { chooseSaveLocation(); };
    saveAsButton.setEnabled (false); // enabled once a run succeeds
    addAndMakeVisible (saveAsButton);

    addAndMakeVisible (resultsPanel);
    waveformDisplay.onSeek = [this] (float normalizedX) { audioPlayer.seekToNormalizedPosition (normalizedX); };
    addAndMakeVisible (waveformDisplay);
    addAndMakeVisible (audioPlayer);

    detailsToggleButton.onClick = [this] { toggleDetails(); };
    detailsToggleButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    detailsToggleButton.setColour (juce::TextButton::textColourOffId, PSColours::accentHi);
    addAndMakeVisible (detailsToggleButton);
    addChildComponent (detailsPanel); // added but not visible until expanded

    setWantsKeyboardFocus (false);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setFont (PSFonts::ui (13.0f, false));
    statusLabel.setColour (juce::Label::textColourId, PSColours::accentHi);
    addAndMakeVisible (statusLabel);

    creditLabel.setFont (PSFonts::ui (11.0f, false));
    creditLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    creditLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (creditLabel);

    updateHeight();

    startTimerHz (30); // drives the waveform playhead; see timerCallback()
}

void MainComponent::timerCallback()
{
    waveformDisplay.setPlayheadPosition (audioPlayer.getNormalizedPosition());
}

void MainComponent::toggleDetails()
{
    detailsExpanded = ! detailsExpanded;
    detailsPanel.setVisible (detailsExpanded);
    if (detailsExpanded)
    {
        detailsPanel.toFront (false); // float above everything else, not just whatever was added after it
        detailsToggleButton.toFront (false); // ...but the close control stays clickable above it
    }
    detailsToggleButton.setButtonText (detailsExpanded ? "Hide details  v" : "More details  >");
    resized();
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
    {
        juce::File f (path);
        auto ext = f.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac")
            return true;
    }
    return false;
}

void MainComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    isDragHover = true;
    repaint();
}

void MainComponent::fileDragExit (const juce::StringArray&)
{
    isDragHover = false;
    repaint();
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    isDragHover = false;

    for (auto& path : files)
    {
        juce::File f (path);
        auto ext = f.getFileExtension().toLowerCase();
        if (f.existsAsFile() && (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac"))
        {
            loadInputFile (f);
            break;
        }
    }
    repaint();
}

MainComponent::~MainComponent()
{
    setLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (PSColours::bg.brighter (0.02f), 0.0f, 0.0f,
                                              PSColours::bg.darker (0.2f), 0.0f, (float) getHeight(), false));
    g.fillRect (getLocalBounds());

    auto panelBounds = getLocalBounds().reduced (18).withTrimmedTop (88).toFloat();
    PSSkin::drawGlowRoundedRect (g, panelBounds, 14.0f, PSColours::panel.brighter (0.03f),
                                  PSColours::panel.darker (0.1f), PSColours::panel, 0.0f);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (panelBounds, 14.0f, 1.0f);

    auto markBounds = juce::Rectangle<float> (18.0f, 26.0f, 8.0f, 34.0f);
    juce::ColourGradient grad (PSColours::accentHi, markBounds.getX(), markBounds.getY(),
                                PSColours::accent, markBounds.getX(), markBounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (markBounds, 3.0f);

    if (isDragHover)
    {
        g.setColour (PSColours::accentHi.withAlpha (0.9f));
        g.drawRoundedRectangle (panelBounds.reduced (2.0f), 14.0f, 2.5f);
        g.setFont (PSFonts::ui (18.0f, true));
        g.setColour (PSColours::accentHi);
        g.drawText ("Drop audio file to load", panelBounds, juce::Justification::centred);
    }
}

// Fixed native/design width -- the whole UI is scaled as one unit by the
// host window (see Main.cpp's ScaleHost), not reflowed, so layout never
// needs to track the window's actual current width/height.
static constexpr int kNativeWidth = 960;
static constexpr int kLeftColumnWidth = 300;
static constexpr int kColumnGap = 24;

// Height of the left column's own content (load row through status
// label), used both to lay it out and, via jmax with the right column,
// to size the shared row both columns sit in.
static constexpr int kLeftColumnHeight =
      40 + 6 + 18 + 18        // load button + file label + gap
    + 15 + 6 + 52 + 16        // target section
    + 15 + 6 + 52 + 18        // quality section
    + 44 + 10 + 16;           // process/save row + gap + status label

static constexpr int kRightColumnHeight =
      20 + 6 + 150             // waveform header (+ "More details" link) + waveform
    + 16 + 15 + 6 + 64;        // A/B listen

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (18);

    auto header = area.removeFromTop (70);
    header.removeFromLeft (20);
    titleLogo.setBounds (header.removeFromTop (44).withWidth (160));
    subtitleLabel.setBounds (header);

    area.removeFromTop (18);
    auto inner = area.reduced (22, 18);

    // The details overlay floats over this whole area (see below) instead
    // of pushing the layout down, so it's captured before anything below
    // consumes "inner" -- otherwise its bounds would shrink to whatever
    // happened to be left over. Trimmed down from the very top so it never
    // covers the "More details" toggle itself (which sits right there,
    // and needs to stay clickable so you can close the overlay again).
    auto overlayBounds = inner.withTrimmedTop (40);

    // --- Two columns side by side: controls on the left, the visual
    // (waveform + A/B) on the right -- a wide, plugin-like layout instead
    // of one long vertical stack. Both columns share one row whose height
    // is whichever column is taller, so nothing gets clipped.
    auto columnsArea = inner.removeFromTop (juce::jmax (kLeftColumnHeight, kRightColumnHeight));
    auto left  = columnsArea.removeFromLeft (kLeftColumnWidth);
    columnsArea.removeFromLeft (kColumnGap);
    auto right = columnsArea;

    // Left column -------------------------------------------------------
    loadButton.setBounds (left.removeFromTop (40));
    left.removeFromTop (6);
    inputFileLabel.setBounds (left.removeFromTop (18));
    left.removeFromTop (18);

    targetSectionLabel.setBounds (left.removeFromTop (15));
    left.removeFromTop (6);
    targetControl.setBounds (left.removeFromTop (52));
    left.removeFromTop (16);

    qualitySectionLabel.setBounds (left.removeFromTop (15));
    left.removeFromTop (6);
    qualityControl.setBounds (left.removeFromTop (52));
    left.removeFromTop (18);

    auto actionRow = left.removeFromTop (44);
    processButton.setBounds (actionRow.removeFromLeft (juce::roundToInt ((float) actionRow.getWidth() * 0.6f)));
    actionRow.removeFromLeft (10);
    saveAsButton.setBounds (actionRow);
    left.removeFromTop (10);
    statusLabel.setBounds (left.removeFromTop (16));

    // Right column --------------------------------------------------------
    auto waveHeader = right.removeFromTop (20);
    // The toggle's font scales with its own bounds height (see
    // PSLookAndFeel::getTextButtonFont), so it's given a taller box than
    // the 20px label row to read clearly -- centred on that row rather
    // than confined to it.
    auto toggleArea = waveHeader.removeFromRight (170).withSizeKeepingCentre (170, 40);
    detailsToggleButton.setBounds (toggleArea);
    waveformSectionLabel.setBounds (waveHeader);
    right.removeFromTop (6);
    waveformDisplay.setBounds (right.removeFromTop (150));

    right.removeFromTop (16);
    abSectionLabel.setBounds (right.removeFromTop (15));
    right.removeFromTop (6);
    audioPlayer.setBounds (right.removeFromTop (64));

    // The verdict gets the full width below both columns -- it's the
    // headline result and needs room to actually read, not a squeezed
    // single column.
    inner.removeFromTop (18);
    resultsPanel.setBounds (inner.removeFromTop (resultsPanel.getPreferredHeight()));

    // Everything numeric lives behind the "More details" link above,
    // closed by default. When open it floats on top of everything else in
    // this panel (see toggleDetails(), which brings it to front) rather
    // than pushing the window taller -- a card dropped over the content,
    // not a reflow.
    if (detailsExpanded)
        detailsPanel.setBounds (overlayBounds.removeFromTop (
            juce::jmin (DetailsPanel::kPreferredHeight, overlayBounds.getHeight())));

    creditLabel.setBounds (getLocalBounds().removeFromBottom (24));
}

void MainComponent::updateHeight()
{
    // Mirrors the block order in resized(): header, the two-column row
    // (sized to whichever column is taller), the verdict, and the credit
    // line. The details panel is a floating overlay (see resized()) and
    // deliberately does NOT affect this -- toggling it never resizes the
    // window.
    int h = 18                              // top margin
          + 70 + 18                         // header + gap
          + 18                              // inner reduced top
          + juce::jmax (kLeftColumnHeight, kRightColumnHeight)
          + 18 + resultsPanel.getPreferredHeight()
          + 18                              // inner reduced bottom margin
          + 24;                             // credit label

    setSize (kNativeWidth, h);

    if (onNativeSizeChanged)
        onNativeSizeChanged();
}

void MainComponent::chooseInputFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select a finished WAV to simulate...",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
            loadInputFile (file);
    });
}

void MainComponent::loadInputFile (const juce::File& file)
{
    inputFile = file;
    lastOutputFile = juce::File {};
    saveAsButton.setEnabled (false);
    inputFileLabel.setText (file.getFileName(), juce::dontSendNotification);
    resultsPanel.setPlaceholder ("Ready. Press \"Process\" to continue.");
    detailsPanel.clear();
    waveformDisplay.clear();
    audioPlayer.reset();
    updateHeight();
}

void MainComponent::runProcessing()
{
    if (! inputFile.existsAsFile())
    {
        statusLabel.setText ("Choose an input WAV first.", juce::dontSendNotification);
        return;
    }
    if (isProcessing.load())
        return;

    // Re-processing the same input overwrites the same temp output path
    // every time -- release any file handle the player still holds on it
    // from a previous A/B listen first, or the overwrite can fail on
    // Windows (can't write to a file that's still open for playback).
    audioPlayer.stop();

    // Process immediately -- no save dialog up front. The result is
    // written to a temp file (always writable, no permissions surprises);
    // "Save As..." below copies it wherever you actually want it once you
    // like what you hear, instead of forcing you to pick a destination
    // before you even know if the run will be worth keeping.
    auto outFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("PS_" + inputFile.getFileNameWithoutExtension() + "_spotify_sim.wav");

    auto target  = (SpotifyProcessor::Target)  targetControl.getSelectedIndex();
    auto quality = (SpotifyProcessor::Quality) qualityControl.getSelectedIndex();

    isProcessing = true;
    processButton.setEnabled (false);
    saveAsButton.setEnabled (false);
    statusLabel.setText ("Processing...", juce::dontSendNotification);

    auto inFile = inputFile;
    std::thread worker ([this, inFile, outFile, target, quality]
    {
        SpotifyProcessor processor;
        auto report = processor.process (inFile, outFile, target, quality);

        juce::MessageManager::callAsync ([this, report, outFile]
        {
            isProcessing = false;
            processButton.setEnabled (true);
            showReport (report, outFile);
        });
    });
    worker.detach();
}

void MainComponent::chooseSaveLocation()
{
    if (! lastOutputFile.existsAsFile())
        return;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save the Spotify-simulated WAV as...",
        inputFile.getParentDirectory().getChildFile (inputFile.getFileNameWithoutExtension() + "_spotify_sim.wav"),
        "*.wav");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
    {
        auto dest = fc.getResult();
        if (dest == juce::File {})
            return;

        if (lastOutputFile.copyFileTo (dest))
            statusLabel.setText ("Saved to \"" + dest.getFileName() + "\".", juce::dontSendNotification);
        else
            statusLabel.setText ("Could not save to that location.", juce::dontSendNotification);
    });
}

void MainComponent::showReport (const SpotifyProcessor::Report& report, const juce::File& outFile)
{
    resultsPanel.setReport (report);

    if (report.success)
    {
        lastOutputFile = outFile;
        saveAsButton.setEnabled (true);
        statusLabel.setText ("Done - press \"Save As...\" to keep this result.", juce::dontSendNotification);

        // Level-match the original for a fair A/B comparison: play it
        // back with the same gain the processor applied.
        const float levelMatchGain = (float) std::pow (10.0, report.appliedGainDb / 20.0);
        audioPlayer.setSources (inputFile, outFile, levelMatchGain,
                                 report.differenceAvailable ? report.differenceFile : juce::File {},
                                 report.differenceBoostDb);

        detailsPanel.setReport (report);
        waveformDisplay.setData (report.inputWaveformMin, report.inputWaveformMax,
                                   report.outputWaveformMin, report.outputWaveformMax);
    }
    else
    {
        lastOutputFile = juce::File {};
        saveAsButton.setEnabled (false);
        statusLabel.setText ("Error.", juce::dontSendNotification);
        audioPlayer.reset();
        detailsPanel.clear();
        waveformDisplay.clear();
    }

    updateHeight();
}
