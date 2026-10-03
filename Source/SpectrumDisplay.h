#pragma once
#include <JuceHeader.h>

// Overlaid averaged-magnitude spectrum: original vs post-codec output,
// log-frequency x-axis (20 Hz - 20 kHz / Nyquist), dB y-axis. Shows at a
// glance where the codec/normalization chain lost or changed energy.
class SpectrumDisplay : public juce::Component
{
public:
    SpectrumDisplay();

    void setData (const std::vector<float>& inputDb, const std::vector<float>& outputDb,
                  double sampleRate, int fftSize);
    void clear();

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> inputSpectrum, outputSpectrum;
    double sampleRate = 44100.0;
    int fftSize = 2048;
    bool hasData = false;

    static constexpr float kMinFreq = 20.0f;
    static constexpr float kMaxFreq = 20000.0f;
    static constexpr float kMinDb = -90.0f;
    static constexpr float kMaxDb = 0.0f;

    float dbAtFrequency (const std::vector<float>& spectrum, float freqHz) const;
    juce::Path buildPath (const std::vector<float>& spectrum, juce::Rectangle<float> area) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};
