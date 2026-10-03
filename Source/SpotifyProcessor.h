#pragma once
#include <JuceHeader.h>
#include "LoudnessMeter.h"

// Simulates the *audible consequences* of Spotify-style loudness
// normalization and lossy codec processing:
// 1. Loudness-normalize to one of Spotify's published targets. When a
//    boost is needed, the full gain is applied and a deterministic
//    limiter (not just a gain cap) protects the true peak, matching how
//    Spotify's own docs describe preventing clipping on boosted tracks.
// 2. Run the result through a REAL Ogg Vorbis encode -> decode round trip
//    at one of Spotify's four documented quality tiers.
// This is a playback-oriented simulation, not a bit-for-bit recreation of
// Spotify's private backend — wording throughout should stay honest about that.
class SpotifyProcessor
{
public:
    enum class Target { Loud, Normal, Quiet };
    enum class Quality { Low24, Normal96, High160, VeryHigh320 };

    struct Report
    {
        bool success = false;
        juce::String errorMessage;

        // Source file info.
        double sourceSampleRate = 0.0;
        int sourceBitsPerSample = 0;
        int sourceNumChannels = 0;
        double durationSeconds = 0.0;

        // Loudness / peak at each stage.
        double inputLUFS = 0.0, inputTruePeak = 0.0, inputSamplePeak = 0.0;
        double afterGainLUFS = 0.0, afterGainTruePeak = 0.0, afterGainSamplePeak = 0.0;
        double outputLUFS = 0.0, outputTruePeak = 0.0, outputSamplePeak = 0.0;

        double appliedGainDb = 0.0;
        double targetLUFS = 0.0;

        bool limiterEngaged = false;
        double limiterGainReductionDb = 0.0; // max reduction applied, in dB

        int qualityKbps = 0;

        // Stereo field: Mid/Side RMS level and L/R correlation (-1..1).
        // Measured on the pristine original and on the final post-codec
        // output, so a width/mono-compatibility change is visible.
        bool isStereo = false;
        double inputMidDb = -100.0, inputSideDb = -100.0, inputCorrelation = 1.0;
        double outputMidDb = -100.0, outputSideDb = -100.0, outputCorrelation = 1.0;

        // Averaged magnitude spectrum in dB, one array for the pristine
        // original and one for the final post-codec output. Bin i
        // corresponds to i * sourceSampleRate / spectrumFftSize Hz.
        std::vector<float> inputSpectrumDb, outputSpectrumDb;
        int spectrumFftSize = 0;

        // Downsampled min/max waveform peaks (one pair per column) for
        // drawing an overview waveform without re-reading the files.
        std::vector<float> inputWaveformMin, inputWaveformMax;
        std::vector<float> outputWaveformMin, outputWaveformMax;

        // "What got lost": (post-codec output) minus (pre-codec, after
        // gain/limiter signal), sample-by-sample, written as its own WAV
        // next to outputFile. The raw difference is usually far too quiet
        // to hear, so it's boosted by differenceBoostDb to a sane listening
        // level -- that boost is disclosed rather than hidden. This is a
        // simple sample-domain subtraction, not a delay-compensated/
        // phase-aligned diff, so a very small amount of what it shows can
        // be the codec's own internal (sub-millisecond) delay rather than
        // an actual audible artifact.
        bool differenceAvailable = false;
        juce::File differenceFile;
        double differenceBoostDb = 0.0;
        int differenceAlignmentSamples = 0; // codec delay compensated before diffing, if any
    };

    static double targetToLUFS (Target t)
    {
        switch (t)
        {
            case Target::Loud:   return -11.0;
            case Target::Quiet:  return -19.0;
            case Target::Normal: default: return -14.0;
        }
    }

    // Ceiling the limiter protects when a boost is applied, matching
    // Spotify's documented clip-prevention behaviour on boosted tracks.
    static constexpr double kLimiterCeilingDbTP = -1.0;
    static constexpr double kLimiterAttackMs = 5.0;
    static constexpr double kLimiterReleaseMs = 100.0;

    static int qualityToKbps (Quality q)
    {
        switch (q)
        {
            case Quality::Low24:     return 24;
            case Quality::Normal96:  return 96;
            case Quality::High160:   return 160;
            case Quality::VeryHigh320: default: return 320;
        }
    }

    // Runs the whole pipeline. inputFile must be a WAV/AIFF/FLAC Juce can
    // read. outputFile will be written as a 24-bit WAV, and only once the
    // entire pipeline has succeeded (nothing partial is ever written).
    // Never throws — all failures come back through Report::success/errorMessage.
    Report process (const juce::File& inputFile, const juce::File& outputFile,
                     Target target, Quality quality);

private:
    static int qualityToOggIndex (Quality q, int numAvailableOptions);

    // Runs the attack/release envelope limiter directly over raw channel
    // pointers (so it can operate on either a plain buffer or an
    // oversampled scratch block). Returns the max gain reduction in dB.
    static double applyLimiterToChannels (float* const* channelData, int numChannels, int numSamples,
                                           double sampleRate, double ceilingDb, double attackMs, double releaseMs);

    // Limits true (inter-sample) peaks, not just sample peaks: the buffer
    // is oversampled 4x, limited at that rate, then downsampled back in
    // place. Returns the max gain reduction applied, in dB.
    static double applyTruePeakLimiter (juce::AudioBuffer<float>& buffer, double sampleRate,
                                         double ceilingDb, double attackMs, double releaseMs);

    static double measureSamplePeakDb (const juce::AudioBuffer<float>& buffer);

    static void measureStereo (const juce::AudioBuffer<float>& buffer,
                                double& midDb, double& sideDb, double& correlation);

    static std::vector<float> computeAverageSpectrumDb (const juce::AudioBuffer<float>& buffer, int fftOrder);

    static void computeWaveformPeaks (const juce::AudioBuffer<float>& buffer, int numColumns,
                                       std::vector<float>& outMin, std::vector<float>& outMax);

    // Estimates the small (sub-millisecond-scale) sample delay that the
    // codec's own algorithmic latency introduces between bufferA (pre-codec)
    // and bufferB (post-codec), via FFT cross-correlation on a mono-summed
    // window, with the sign resolved empirically (by actually comparing
    // residual energy for each candidate shift) rather than trusted from a
    // correlation-index convention. Returns 0 if no shift reliably helps.
    // Positive = bufferB should have samples dropped from its start to align;
    // negative = bufferA should.
    static int estimateCodecDelaySamples (const juce::AudioBuffer<float>& bufferA,
                                           const juce::AudioBuffer<float>& bufferB);

    // Builds (bufferB - bufferA) sample-by-sample, after compensating for
    // the codec's own algorithmic delay (see estimateCodecDelaySamples) and
    // trimming to the overlapping region, then boosts the result so its
    // peak lands near targetPeakDb -- raw codec differences are typically
    // 30-60dB too quiet to hear otherwise. Returns the boost actually
    // applied, in dB; the alignment shift used is written to outAlignmentSamples.
    static double buildBoostedDifference (const juce::AudioBuffer<float>& bufferA,
                                           const juce::AudioBuffer<float>& bufferB,
                                           double targetPeakDb,
                                           juce::AudioBuffer<float>& outDiff,
                                           int& outAlignmentSamples);

    static constexpr int kSpectrumFftOrder = 11; // 2048
    static constexpr int kWaveformColumns = 900;
};
