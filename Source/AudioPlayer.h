#pragma once
#include <JuceHeader.h>

// Level-matched A/B playback: "Original" is played back with the same
// gain the processor applied to reach the target loudness, so a plain
// loudness difference doesn't bias the comparison -- only the codec /
// limiter artifacts should be what you notice.
class AudioPlayer : public juce::Component,
                     private juce::ChangeListener
{
public:
    AudioPlayer();
    ~AudioPlayer() override;

    // differenceFile may be an invalid/non-existent File if no difference
    // signal was produced -- the "C" button is simply disabled in that case.
    void setSources (const juce::File& originalFile, const juce::File& processedFile,
                      float originalLevelMatchGain,
                      const juce::File& differenceFile = {}, double differenceBoostDb = 0.0);
    void reset();

    // Stops playback and releases the current file's reader/handle, while
    // leaving the stored source paths and enabled buttons alone (unlike
    // reset()). Callers that are about to overwrite one of those files on
    // disk should call this first, or the overwrite can fail while a
    // handle is still open (e.g. on Windows).
    void stop();

    // Jumps playback to a position given as a 0..1 fraction of the
    // currently-loaded file's length (e.g. from a click on the waveform).
    // No-op if nothing is loaded yet.
    void seekToNormalizedPosition (float normalizedX);

    // Current playback position as a 0..1 fraction of the loaded file's
    // length, for driving a moving playhead on the waveform -- or -1 if
    // nothing is loaded yet (no playhead to show). Reflects the position
    // even while paused/stopped, so the playhead freezes in place rather
    // than disappearing.
    float getNormalizedPosition() const;

    void resized() override;

private:
    enum class Playing { none, original, processed, difference };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void playOriginal();
    void playProcessed();
    void playDifference();
    void switchTo (const juce::File& file, float gain, Playing which, const juce::String& statusText);
    void loadIntoTransport (const juce::File& file, float gain, double startPositionSeconds);

    juce::AudioFormatManager formatManager;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::TimeSliceThread readAheadThread { "PS audio playback" };

    juce::File originalFile, processedFile, differenceFile;
    float originalGain = 1.0f;
    double differenceBoostDb = 0.0;

    juce::TextButton playOriginalButton { "A   Original" };
    juce::TextButton playProcessedButton { "B   Simulated" };
    juce::TextButton playDifferenceButton { "C   Difference" };
    juce::TextButton stopButton { "Stop" };
    juce::Label statusLabel;

    Playing currentlyPlaying = Playing::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPlayer)
};
