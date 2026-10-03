#include "AudioPlayer.h"
#include "PSTheme.h"
#include "PSFonts.h"

AudioPlayer::AudioPlayer()
{
    formatManager.registerBasicFormats();
    readAheadThread.startThread (juce::Thread::Priority::normal);

    deviceManager.initialiseWithDefaultDevices (0, 2);
    deviceManager.addAudioCallback (&audioSourcePlayer);
    audioSourcePlayer.setSource (&transportSource);

    transportSource.addChangeListener (this);

    playOriginalButton.onClick = [this] { playOriginal(); };
    playProcessedButton.onClick = [this] { playProcessed(); };
    playDifferenceButton.onClick = [this] { playDifference(); };
    stopButton.onClick = [this] { stop(); };

    playOriginalButton.setEnabled (false);
    playProcessedButton.setEnabled (false);
    playDifferenceButton.setEnabled (false);
    stopButton.setEnabled (false);

    addAndMakeVisible (playOriginalButton);
    addAndMakeVisible (playProcessedButton);
    addAndMakeVisible (playDifferenceButton);
    addAndMakeVisible (stopButton);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setFont (PSFonts::ui (12.0f, false));
    statusLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    // Left blank until there's something real to report (playing/finished/
    // error) -- this label's job is live transport status, not an idle
    // instruction that's just visual clutter before anything's loaded.
    addAndMakeVisible (statusLabel);
}

AudioPlayer::~AudioPlayer()
{
    transportSource.removeChangeListener (this);
    transportSource.setSource (nullptr);
    audioSourcePlayer.setSource (nullptr);
    deviceManager.removeAudioCallback (&audioSourcePlayer);
    deviceManager.closeAudioDevice();
    readAheadThread.stopThread (2000);
}

void AudioPlayer::resized()
{
    auto area = getLocalBounds();
    auto row = area.removeFromTop (38);
    const int gap = 8;

    // Weighted, not equal, widths -- matches the design spec's "A/B button
    // row" proportions (A widest since it's the reference/default, Stop
    // narrowest since it's a single short word), rather than four
    // mechanically identical boxes.
    const float wA = 1.15f, wB = 1.0f, wC = 1.0f, wStop = 0.83f;
    const float totalW = wA + wB + wC + wStop;
    const int avail = row.getWidth() - gap * 3;
    const int wAi = juce::roundToInt ((float) avail * wA / totalW);
    const int wBi = juce::roundToInt ((float) avail * wB / totalW);
    const int wCi = juce::roundToInt ((float) avail * wC / totalW);

    playOriginalButton.setBounds (row.removeFromLeft (wAi));
    row.removeFromLeft (gap);
    playProcessedButton.setBounds (row.removeFromLeft (wBi));
    row.removeFromLeft (gap);
    playDifferenceButton.setBounds (row.removeFromLeft (wCi));
    row.removeFromLeft (gap);
    stopButton.setBounds (row); // takes the remainder, absorbing any rounding slack

    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (18));
}

void AudioPlayer::paint (juce::Graphics& g)
{
    // A thin accent underline below whichever source is currently playing,
    // instead of a filled capsule -- matches the design spec's "subtle
    // teal bottom accent line" for the active A/B state.
    const juce::TextButton* activeButton = nullptr;
    switch (currentlyPlaying)
    {
        case Playing::original:  activeButton = &playOriginalButton;  break;
        case Playing::processed: activeButton = &playProcessedButton; break;
        case Playing::difference: activeButton = &playDifferenceButton; break;
        case Playing::none:
        default: break;
    }

    if (activeButton != nullptr)
    {
        auto b = activeButton->getBounds().toFloat().reduced (10.0f, 0.0f);
        const float y = activeButton->getBottom() - 2.0f;
        juce::Colour c = PSColours::accentHi;

        juce::DropShadow glow (c.withAlpha (0.5f), 5, {});
        juce::Path line;
        line.startNewSubPath (b.getX(), y);
        line.lineTo (b.getRight(), y);
        glow.drawForPath (g, line);

        g.setColour (c);
        g.drawLine (b.getX(), y, b.getRight(), y, 2.0f);
    }
}

void AudioPlayer::setSources (const juce::File& originalFileIn, const juce::File& processedFileIn,
                               float originalLevelMatchGain,
                               const juce::File& differenceFileIn, double differenceBoostDbIn)
{
    originalFile = originalFileIn;
    processedFile = processedFileIn;
    differenceFile = differenceFileIn;
    originalGain = originalLevelMatchGain;
    differenceBoostDb = differenceBoostDbIn;

    playOriginalButton.setEnabled (true);
    playProcessedButton.setEnabled (true);
    playDifferenceButton.setEnabled (differenceFile.existsAsFile());
    stopButton.setEnabled (true);
    statusLabel.setText ("Ready - levels matched for a fair comparison.", juce::dontSendNotification);
}

void AudioPlayer::reset()
{
    stop();
    originalFile = {};
    processedFile = {};
    differenceFile = {};
    playOriginalButton.setEnabled (false);
    playProcessedButton.setEnabled (false);
    playDifferenceButton.setEnabled (false);
    stopButton.setEnabled (false);
    statusLabel.setText ({}, juce::dontSendNotification);
}

void AudioPlayer::loadIntoTransport (const juce::File& file, float gain, double startPositionSeconds)
{
    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource.reset();

    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr)
    {
        statusLabel.setText ("Could not open file for playback.", juce::dontSendNotification);
        return;
    }

    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    transportSource.setSource (readerSource.get(), 32768, &readAheadThread, reader->sampleRate);
    transportSource.setGain (gain);

    // Clamp to the new file's own length -- the three files (original,
    // processed, difference) aren't always exactly the same duration, so
    // a position near the end of a longer one could otherwise be past the
    // end of a shorter one.
    const double lengthSeconds = reader->sampleRate > 0.0
        ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;
    transportSource.setPosition (juce::jlimit (0.0, lengthSeconds, startPositionSeconds));
}

// Shared by playOriginal/playProcessed/playDifference: switching between
// A/B/C keeps the current playback position (and keeps playing) instead
// of jumping back to the start, so you can actually A/B a specific moment
// in the track rather than always comparing from 0:00.
void AudioPlayer::switchTo (const juce::File& file, float gain, Playing which, const juce::String& statusText)
{
    if (! file.existsAsFile())
        return;

    const double position = transportSource.getCurrentPosition();
    loadIntoTransport (file, gain, position);
    transportSource.start();
    currentlyPlaying = which;
    statusLabel.setText (statusText, juce::dontSendNotification);
    repaint(); // moves the active-state underline to the new source (see paint())
}

void AudioPlayer::playOriginal()
{
    switchTo (originalFile, originalGain, Playing::original, "Playing: Original (level-matched)");
}

void AudioPlayer::playProcessed()
{
    switchTo (processedFile, 1.0f, Playing::processed, "Playing: Simulated (processed)");
}

void AudioPlayer::playDifference()
{
    switchTo (differenceFile, 1.0f, Playing::difference,
              "Playing: Difference (boosted +" + juce::String (differenceBoostDb, 1) + " dB to be audible)");
}

void AudioPlayer::seekToNormalizedPosition (float normalizedX)
{
    if (readerSource == nullptr)
    {
        // Nothing loaded yet -- a click on the waveform before any A/B
        // button has been pressed starts the processed/simulated track
        // (what the waveform shows "live" against the original) right
        // from that point, rather than doing nothing.
        if (! processedFile.existsAsFile())
            return;
        switchTo (processedFile, 1.0f, Playing::processed, "Playing: Simulated (processed)");
    }

    const double lengthSeconds = transportSource.getLengthInSeconds();
    if (lengthSeconds <= 0.0)
        return;

    transportSource.setPosition (juce::jlimit (0.0, lengthSeconds, (double) normalizedX * lengthSeconds));
    transportSource.start();
}

float AudioPlayer::getNormalizedPosition() const
{
    if (readerSource == nullptr)
        return -1.0f;

    const double lengthSeconds = transportSource.getLengthInSeconds();
    if (lengthSeconds <= 0.0)
        return -1.0f;

    return (float) juce::jlimit (0.0, 1.0, transportSource.getCurrentPosition() / lengthSeconds);
}

void AudioPlayer::stop()
{
    transportSource.stop();
    currentlyPlaying = Playing::none;
    repaint(); // clears the active-state underline (see paint())
}

void AudioPlayer::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! transportSource.isPlaying() && currentlyPlaying != Playing::none)
    {
        currentlyPlaying = Playing::none;
        juce::MessageManager::callAsync ([this] { statusLabel.setText ("Finished.", juce::dontSendNotification); repaint(); });
    }
}
