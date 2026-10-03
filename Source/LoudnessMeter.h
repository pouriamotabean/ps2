#pragma once
#include <JuceHeader.h>

// ITU-R BS.1770-4 integrated loudness (LUFS) + an oversampled true-peak
// estimate. This is a practical, standards-faithful implementation of the
// K-weighting filters and the gated-integration algorithm used by Spotify,
// EBU R128, etc. True peak is estimated via 4x anti-aliased oversampling
// (JUCE's juce::dsp::Oversampling) rather than the exact BS.1770 Annex-2
// polyphase filter — close enough for safe gain-staking decisions.
class LoudnessMeter
{
public:
    struct Result
    {
        double integratedLUFS = -70.0; // gated integrated loudness
        double truePeakDbTP   = -100.0;
    };

    // Measures a whole buffer (mono or stereo) at the given sample rate.
    static Result measure (const juce::AudioBuffer<float>& buffer, double sampleRate);

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        float process (float xIn)
        {
            double x = (double) xIn;
            double y = b0 * x + z1;
            z1 = b1 * x + z2 - a1 * y;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
    };

    static Biquad makeStage1Shelf (double fs);
    static Biquad makeStage2HighPass (double fs);
    static double measureTruePeakDbTP (const juce::AudioBuffer<float>& buffer);
};
