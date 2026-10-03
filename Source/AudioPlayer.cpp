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
    statusLabel.setText ("Process a file to enable A/B playback.", juce::dontSendNotification);
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
    const int w = (row.getWidth() - gap * 3) / 4;
    playOriginalButton.setBounds (row.removeFromLeft (w));
    row.removeFromLeft (gap);
    playProcessedButton.setBounds (row.removeFromLeft (w));
    row.removeFromLeft (gap);
    playDifferenceButton.setBounds (row.removeFromLeft (w));
    row.removeFromLeft (gap);
    stopButton.setBounds (row);

    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (18));
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
    statusLabel.setText ("Process a file to enable A/B playback.", juce::dontSendNotification);
}

void AudioPlayer::loadIntoTransport (const juce::File& file, float gain)
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
    transportSource.setPosition (0.0);
}

void AudioPlayer::playOriginal()
{
    if (! originalFile.existsAsFile())
        return;
    loadIntoTransport (originalFile, originalGain);
    transportSource.start();
    currentlyPlaying = Playing::original;
    statusLabel.setText ("Playing: Original (level-matched)", juce::dontSendNotification);
}

void AudioPlayer::playProcessed()
{
    if (! processedFile.existsAsFile())
        return;
    loadIntoTransport (processedFile, 1.0f);
    transportSource.start();
    currentlyPlaying = Playing::processed;
    statusLabel.setText ("Playing: Simulated (processed)", juce::dontSendNotification);
}

void AudioPlayer::playDifference()
{
    if (! differenceFile.existsAsFile())
        return;
    loadIntoTransport (differenceFile, 1.0f);
    transportSource.start();
    currentlyPlaying = Playing::difference;
    statusLabel.setText ("Playing: Difference (boosted +" + juce::String (differenceBoostDb, 1)
                              + " dB to be audible)", juce::dontSendNotification);
}

void AudioPlayer::stop()
{
    transportSource.stop();
    currentlyPlaying = Playing::none;
}

void AudioPlayer::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! transportSource.isPlaying() && currentlyPlaying != Playing::none)
    {
        currentlyPlaying = Playing::none;
        juce::MessageManager::callAsync ([this] { statusLabel.setText ("Finished.", juce::dontSendNotification); });
    }
}
