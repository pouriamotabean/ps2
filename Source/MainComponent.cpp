#include "MainComponent.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

MainComponent::MainComponent()
    : targetControl ({ { "Loud", "-11 LUFS" }, { "Normal", "-14 LUFS" }, { "Quiet", "-19 LUFS" } }),
      qualityControl ({ { "Low", "24 kbps" }, { "Normal", "96 kbps" }, { "High", "160 kbps" }, { "V.High", "320 kbps" } })
{
    setLookAndFeel (&lookAndFeel);
    setSize (660, 760); // final height recomputed by updateHeight() below

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

    addAndMakeVisible (resultsPanel);
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
}

void MainComponent::toggleDetails()
{
    detailsExpanded = ! detailsExpanded;
    detailsPanel.setVisible (detailsExpanded);
    detailsToggleButton.setButtonText (detailsExpanded ? "Hide details  v" : "More details  >");
    updateHeight();
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

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (18);

    auto header = area.removeFromTop (70);
    header.removeFromLeft (20);
    titleLogo.setBounds (header.removeFromTop (44).withWidth (160));
    subtitleLabel.setBounds (header);

    area.removeFromTop (18);
    auto inner = area.reduced (22, 18);

    auto loadRow = inner.removeFromTop (40);
    loadButton.setBounds (loadRow.removeFromLeft (150));
    loadRow.removeFromLeft (14);
    inputFileLabel.setBounds (loadRow);

    inner.removeFromTop (18);

    targetSectionLabel.setBounds (inner.removeFromTop (15));
    inner.removeFromTop (6);
    targetControl.setBounds (inner.removeFromTop (52));

    inner.removeFromTop (16);

    qualitySectionLabel.setBounds (inner.removeFromTop (15));
    inner.removeFromTop (6);
    qualityControl.setBounds (inner.removeFromTop (52));

    inner.removeFromTop (18);
    processButton.setBounds (inner.removeFromTop (44));

    inner.removeFromTop (10);
    statusLabel.setBounds (inner.removeFromTop (16));

    // Verdict - the one piece of feedback shown by default, right after
    // the status line, with the waveform right underneath it.
    inner.removeFromTop (10);
    resultsPanel.setBounds (inner.removeFromTop (resultsPanel.getPreferredHeight()));

    inner.removeFromTop (14);
    auto waveHeader = inner.removeFromTop (20);
    detailsToggleButton.setBounds (waveHeader.removeFromRight (130));
    waveformSectionLabel.setBounds (waveHeader);
    inner.removeFromTop (6);
    waveformDisplay.setBounds (inner.removeFromTop (140));

    inner.removeFromTop (16);
    abSectionLabel.setBounds (inner.removeFromTop (15));
    inner.removeFromTop (6);
    audioPlayer.setBounds (inner.removeFromTop (64));

    // Everything numeric lives behind the "More details" link above,
    // closed by default -- when open it unfolds right here.
    if (detailsExpanded)
    {
        inner.removeFromTop (14);
        detailsPanel.setBounds (inner.removeFromTop (DetailsPanel::kPreferredHeight));
    }

    creditLabel.setBounds (getLocalBounds().removeFromBottom (24));
}

void MainComponent::updateHeight()
{
    // Mirrors the block order in resized(): header down through the A/B
    // player (always present), plus the details panel only when expanded,
    // plus the credit line.
    int h = 18            // top margin
          + 70 + 18        // header + gap
          + 18             // inner reduced top
          + 40 + 18        // load row + gap
          + 15 + 6 + 52 + 16   // target control
          + 15 + 6 + 52 + 18   // quality control
          + 44 + 10        // process button + gap
          + 16             // status label
          + 10 + resultsPanel.getPreferredHeight()
          + 14 + 20 + 6 + 140  // waveform header (+ "More details" link) + waveform
          + 16 + 15 + 6 + 64   // A/B listen
          + 18             // inner reduced bottom margin
          + 24;            // credit label

    if (detailsExpanded)
        h += 14 + DetailsPanel::kPreferredHeight;

    // Fixed native/design width -- the whole UI is scaled as one unit by
    // the host window (see Main.cpp's ScaleHost), not reflowed, so this
    // never needs to track the window's actual current width.
    setSize (660, h);

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
    inputFileLabel.setText (file.getFileName(), juce::dontSendNotification);
    resultsPanel.setPlaceholder ("Ready. Press \"Process & Save As...\" to continue.");
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

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save the Spotify-simulated WAV as...",
        inputFile.getParentDirectory().getChildFile (inputFile.getFileNameWithoutExtension() + "_spotify_sim.wav"),
        "*.wav");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
    {
        auto outFile = fc.getResult();
        if (outFile == juce::File {})
            return;

        auto target  = (SpotifyProcessor::Target)  targetControl.getSelectedIndex();
        auto quality = (SpotifyProcessor::Quality) qualityControl.getSelectedIndex();

        isProcessing = true;
        processButton.setEnabled (false);
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
    });
}

void MainComponent::showReport (const SpotifyProcessor::Report& report, const juce::File& outFile)
{
    resultsPanel.setReport (report);
    statusLabel.setText (report.success ? "Done." : "Error.", juce::dontSendNotification);

    if (report.success)
    {
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
        audioPlayer.reset();
        detailsPanel.clear();
        waveformDisplay.clear();
    }

    updateHeight();
}
