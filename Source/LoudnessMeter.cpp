#include "LoudnessMeter.h"

LoudnessMeter::Biquad LoudnessMeter::makeStage1Shelf (double fs)
{
    // Pre-filter: high-frequency shelving, per ITU-R BS.1770 / libebur128.
    const double f0 = 1681.9744509555319;
    const double G  = 3.99984385397;
    const double Q  = 0.7071752369554193;

    const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
    const double Vh = std::pow (10.0, G / 20.0);
    const double Vb = std::pow (Vh, 0.4996667741545416);

    const double a0 = 1.0 + K / Q + K * K;

    Biquad f;
    f.b0 = (Vh + Vb * K / Q + K * K) / a0;
    f.b1 = 2.0 * (K * K - Vh) / a0;
    f.b2 = (Vh - Vb * K / Q + K * K) / a0;
    f.a1 = 2.0 * (K * K - 1.0) / a0;
    f.a2 = (1.0 - K / Q + K * K) / a0;
    return f;
}

LoudnessMeter::Biquad LoudnessMeter::makeStage2HighPass (double fs)
{
    // RLB weighting high-pass, per ITU-R BS.1770 / libebur128.
    const double f0 = 38.13547087613982;
    const double Q  = 0.5003270373238773;

    const double K  = std::tan (juce::MathConstants<double>::pi * f0 / fs);
    const double a0 = 1.0 + K / Q + K * K;

    Biquad f;
    f.b0 = 1.0 / a0;
    f.b1 = -2.0 / a0;
    f.b2 = 1.0 / a0;
    f.a1 = 2.0 * (K * K - 1.0) / a0;
    f.a2 = (1.0 - K / Q + K * K) / a0;
    return f;
}

double LoudnessMeter::measureTruePeakDbTP (const juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || numCh <= 0)
        return -100.0;

    juce::dsp::Oversampling<float> oversampler ((size_t) numCh, 2 /*=> 4x*/,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler.initProcessing ((size_t) numSamples);

    juce::AudioBuffer<float> work (buffer);
    juce::dsp::AudioBlock<float> block (work);

    auto oversampledBlock = oversampler.processSamplesUp (block);

    float peak = 0.0f;
    for (size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        const float* d = oversampledBlock.getChannelPointer (ch);
        for (size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
            peak = juce::jmax (peak, std::abs (d[i]));
    }

    oversampler.reset();
    if (peak <= 0.0f)
        return -100.0;
    return 20.0 * std::log10 ((double) peak);
}

LoudnessMeter::Result LoudnessMeter::measure (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    Result result;
    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numCh <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return result;

    // K-weight each channel independently.
    std::vector<std::vector<float>> weighted (numCh, std::vector<float> ((size_t) numSamples));
    for (int ch = 0; ch < numCh; ++ch)
    {
        Biquad s1 = makeStage1Shelf (sampleRate);
        Biquad s2 = makeStage2HighPass (sampleRate);
        const float* in = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            weighted[(size_t) ch][(size_t) i] = s2.process (s1.process (in[i]));
    }

    // 400ms blocks, 100ms step (75% overlap).
    const int blockLen = (int) std::round (0.4 * sampleRate);
    const int stepLen  = (int) std::round (0.1 * sampleRate);
    if (blockLen <= 0 || stepLen <= 0 || numSamples < blockLen)
    {
        // Too short for gated blocks: fall back to a single block over
        // the whole signal so short preview clips still get a sensible
        // reading instead of the floor value.
        double sum = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            double chSum = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                double v = weighted[(size_t) ch][(size_t) i];
                chSum += v * v;
            }
            sum += chSum / (double) numSamples;
        }
        result.integratedLUFS = -0.691 + 10.0 * std::log10 (juce::jmax (1.0e-12, sum));
        result.truePeakDbTP = measureTruePeakDbTP (buffer);
        return result;
    }

    std::vector<double> blockEnergies;
    for (int start = 0; start + blockLen <= numSamples; start += stepLen)
    {
        double sum = 0.0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            double chSum = 0.0;
            const float* w = weighted[(size_t) ch].data();
            for (int i = 0; i < blockLen; ++i)
            {
                double v = w[start + i];
                chSum += v * v;
            }
            sum += chSum / (double) blockLen; // channel weight G=1.0 for L/R
        }
        blockEnergies.push_back (sum);
    }

    // Absolute gate: -70 LUFS.
    const double absThresholdEnergy = std::pow (10.0, (-70.0 + 0.691) / 10.0);

    std::vector<double> passedAbs;
    for (double e : blockEnergies)
        if (e > absThresholdEnergy)
            passedAbs.push_back (e);

    if (passedAbs.empty())
    {
        result.integratedLUFS = -70.0;
        result.truePeakDbTP = measureTruePeakDbTP (buffer);
        return result;
    }

    double meanAbs = 0.0;
    for (double e : passedAbs) meanAbs += e;
    meanAbs /= (double) passedAbs.size();

    const double relativeLoudness = -0.691 + 10.0 * std::log10 (juce::jmax (1.0e-12, meanAbs));
    const double relThresholdEnergy = std::pow (10.0, (relativeLoudness - 10.0 + 0.691) / 10.0);

    std::vector<double> passedRel;
    for (double e : passedAbs)
        if (e > relThresholdEnergy)
            passedRel.push_back (e);

    double finalMean;
    if (! passedRel.empty())
    {
        finalMean = 0.0;
        for (double e : passedRel) finalMean += e;
        finalMean /= (double) passedRel.size();
    }
    else
    {
        finalMean = meanAbs;
    }

    result.integratedLUFS = -0.691 + 10.0 * std::log10 (juce::jmax (1.0e-12, finalMean));
    result.truePeakDbTP = measureTruePeakDbTP (buffer);
    return result;
}
