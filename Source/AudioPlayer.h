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

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void playOriginal();
    void playProcessed();
    void playDifference();
    void stop();
    void loadIntoTransport (const juce::File& file, float gain);

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

    enum class Playing { none, original, processed, difference };
    Playing currentlyPlaying = Playing::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPlayer)
};
