#include "SpotifyProcessor.h"

int SpotifyProcessor::qualityToOggIndex (Quality q, int numAvailableOptions)
{
    const int wantedKbps = qualityToKbps (q);

    juce::OggVorbisAudioFormat ogg;
    juce::StringArray options = ogg.getQualityOptions();

    int bestIndex = 0;
    int bestDiff = std::numeric_limits<int>::max();
    bool foundNumeric = false;

    for (int i = 0; i < options.size(); ++i)
    {
        juce::String s = options[i];
        juce::String digits;
        for (auto ch : s)
            if (juce::CharacterFunctions::isDigit (ch))
                digits += ch;
            else if (digits.isNotEmpty())
                break;

        if (digits.isNotEmpty())
        {
            int kbps = digits.getIntValue();
            int diff = std::abs (kbps - wantedKbps);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestIndex = i;
                foundNumeric = true;
            }
        }
    }

    if (foundNumeric)
        return bestIndex;

    const int n = juce::jmax (1, numAvailableOptions);
    const double t = juce::jlimit (0.0, 1.0, (double) (wantedKbps - 24) / (double) (320 - 24));
    return juce::jlimit (0, n - 1, (int) std::round (t * (double) (n - 1)));
}

double SpotifyProcessor::measureSamplePeakDb (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return -100.0;

    // The 2-argument overload (startSample, numSamples) scans ALL channels
    // internally and returns the maximum magnitude across them.
    const float peak = buffer.getMagnitude (0, numSamples);
    if (peak <= 0.0f)
        return -100.0;
    return 20.0 * std::log10 ((double) peak);
}

double SpotifyProcessor::applyLimiterToChannels (float* const* channelData, int numChannels, int numSamples,
                                                  double sampleRate, double ceilingDb, double attackMs, double releaseMs)
{
    if (numChannels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return 0.0;

    const double threshold = std::pow (10.0, ceilingDb / 20.0);
    const double attackCoeff  = std::exp (-1.0 / (0.001 * attackMs  * sampleRate));
    const double releaseCoeff = std::exp (-1.0 / (0.001 * releaseMs * sampleRate));

    double envelopeGain = 1.0;
    double maxReductionDb = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        double framePeak = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
            framePeak = juce::jmax (framePeak, (double) std::abs (channelData[ch][i]));

        const double desiredGain = (framePeak > threshold && framePeak > 0.0)
                                        ? (threshold / framePeak) : 1.0;

        if (desiredGain < envelopeGain)
            envelopeGain = attackCoeff * envelopeGain + (1.0 - attackCoeff) * desiredGain;
        else
            envelopeGain = releaseCoeff * envelopeGain + (1.0 - releaseCoeff) * desiredGain;

        envelopeGain = juce::jlimit (0.0, 1.0, envelopeGain);

        for (int ch = 0; ch < numChannels; ++ch)
            channelData[ch][i] = (float) (channelData[ch][i] * envelopeGain);

        if (envelopeGain > 0.0)
        {
            const double reductionDb = -20.0 * std::log10 (envelopeGain);
            maxReductionDb = juce::jmax (maxReductionDb, reductionDb);
        }
    }

    return maxReductionDb;
}

double SpotifyProcessor::applyTruePeakLimiter (juce::AudioBuffer<float>& buffer, double sampleRate,
                                                double ceilingDb, double attackMs, double releaseMs)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return 0.0;

    constexpr int oversampleStages = 2; // 2^2 = 4x
    juce::dsp::Oversampling<float> oversampler ((size_t) numChannels, oversampleStages,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    oversampler.initProcessing ((size_t) numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = oversampler.processSamplesUp (block);

    const int numChUp = (int) upBlock.getNumChannels();
    const int numSampUp = (int) upBlock.getNumSamples();
    std::vector<float*> ptrs ((size_t) numChUp);
    for (int ch = 0; ch < numChUp; ++ch)
        ptrs[(size_t) ch] = upBlock.getChannelPointer ((size_t) ch);

    const double oversampledRate = sampleRate * (double) oversampler.getOversamplingFactor();
    const double reductionDb = applyLimiterToChannels (ptrs.data(), numChUp, numSampUp, oversampledRate,
                                                         ceilingDb, attackMs, releaseMs);

    oversampler.processSamplesDown (block); // downsamples the limited signal back into `buffer`
    oversampler.reset();

    return reductionDb;
}

void SpotifyProcessor::measureStereo (const juce::AudioBuffer<float>& buffer,
                                       double& midDb, double& sideDb, double& correlation)
{
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples <= 0)
    {
        midDb = measureSamplePeakDb (buffer);
        sideDb = -100.0;
        correlation = 1.0;
        return;
    }

    const float* L = buffer.getReadPointer (0);
    const float* R = buffer.getReadPointer (1);

    double sumMidSq = 0.0, sumSideSq = 0.0, sumLR = 0.0, sumLL = 0.0, sumRR = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const double l = L[i], r = R[i];
        const double mid = (l + r) * 0.5, side = (l - r) * 0.5;
        sumMidSq += mid * mid;
        sumSideSq += side * side;
        sumLR += l * r;
        sumLL += l * l;
        sumRR += r * r;
    }

    const double midRms = std::sqrt (sumMidSq / (double) numSamples);
    const double sideRms = std::sqrt (sumSideSq / (double) numSamples);
    midDb = midRms > 0.0 ? 20.0 * std::log10 (midRms) : -100.0;
    sideDb = sideRms > 0.0 ? 20.0 * std::log10 (sideRms) : -100.0;

    const double denom = std::sqrt (sumLL * sumRR);
    correlation = denom > 0.0 ? juce::jlimit (-1.0, 1.0, sumLR / denom) : 1.0;
}

std::vector<float> SpotifyProcessor::computeAverageSpectrumDb (const juce::AudioBuffer<float>& buffer, int fftOrder)
{
    const int fftSize = 1 << fftOrder;
    const int numBins = fftSize / 2;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples < fftSize || numChannels <= 0)
        return std::vector<float> ((size_t) numBins, -100.0f);

    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);

    std::vector<float> mono ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            s += buffer.getSample (ch, i);
        mono[(size_t) i] = s / (float) numChannels;
    }

    std::vector<float> accum ((size_t) numBins, 0.0f);
    std::vector<float> fftData ((size_t) fftSize * 2, 0.0f);
    const int hop = fftSize / 2;
    int frameCount = 0;

    for (int start = 0; start + fftSize <= numSamples; start += hop)
    {
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        std::copy (mono.begin() + start, mono.begin() + start + fftSize, fftData.begin());
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int bin = 0; bin < numBins; ++bin)
            accum[(size_t) bin] += fftData[(size_t) bin] * fftData[(size_t) bin];

        ++frameCount;
    }

    if (frameCount == 0)
        return std::vector<float> ((size_t) numBins, -100.0f);

    std::vector<float> result ((size_t) numBins);
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float meanSq = accum[(size_t) bin] / (float) frameCount;
        result[(size_t) bin] = meanSq > 0.0f ? 20.0f * std::log10 (std::sqrt (meanSq)) : -100.0f;
    }
    return result;
}

void SpotifyProcessor::computeWaveformPeaks (const juce::AudioBuffer<float>& buffer, int numColumns,
                                              std::vector<float>& outMin, std::vector<float>& outMax)
{
    outMin.assign ((size_t) numColumns, 0.0f);
    outMax.assign ((size_t) numColumns, 0.0f);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    for (int col = 0; col < numColumns; ++col)
    {
        int start = (int) ((juce::int64) col * numSamples / numColumns);
        int end   = (int) ((juce::int64) (col + 1) * numSamples / numColumns);
        end = juce::jmax (end, start + 1);
        end = juce::jmin (end, numSamples);

        float mn = 0.0f, mx = 0.0f;
        for (int i = start; i < end; ++i)
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float v = buffer.getSample (ch, i);
                mn = juce::jmin (mn, v);
                mx = juce::jmax (mx, v);
            }

        outMin[(size_t) col] = mn;
        outMax[(size_t) col] = mx;
    }
}

int SpotifyProcessor::estimateCodecDelaySamples (const juce::AudioBuffer<float>& bufferA,
                                                   const juce::AudioBuffer<float>& bufferB)
{
    constexpr int maxLag = 4096; // generous upper bound for any practical codec algorithmic delay
    const int available = juce::jmin (bufferA.getNumSamples(), bufferB.getNumSamples());
    if (available < maxLag * 4 || bufferA.getNumChannels() <= 0 || bufferB.getNumChannels() <= 0)
        return 0; // too short to reliably estimate; skip alignment

    const int windowSamples = juce::jmin (available, 1 << 17); // ~3s @44.1kHz: plenty for a stable peak

    int fftOrder = 1;
    while ((1 << fftOrder) < windowSamples * 2) // headroom so maxLag stays well clear of wraparound
        ++fftOrder;
    fftOrder = juce::jlimit (12, 20, fftOrder);
    const int fftSize = 1 << fftOrder;

    auto monoSum = [] (const juce::AudioBuffer<float>& buf, int n, std::vector<float>& out)
    {
        out.assign ((size_t) n, 0.0f);
        const int ch = buf.getNumChannels();
        for (int c = 0; c < ch; ++c)
        {
            const float* p = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                out[(size_t) i] += p[i];
        }
        if (ch > 1)
            for (auto& v : out) v /= (float) ch;
    };

    std::vector<float> monoA, monoB;
    monoSum (bufferA, windowSamples, monoA);
    monoSum (bufferB, windowSamples, monoB);

    juce::dsp::FFT fft (fftOrder);
    std::vector<float> dataA ((size_t) fftSize * 2, 0.0f);
    std::vector<float> dataB ((size_t) fftSize * 2, 0.0f);
    std::copy (monoA.begin(), monoA.end(), dataA.begin());
    std::copy (monoB.begin(), monoB.end(), dataB.begin());

    fft.performRealOnlyForwardTransform (dataA.data());
    fft.performRealOnlyForwardTransform (dataB.data());

    // Cross-power spectrum: A * conj(B). Its inverse transform is the
    // (circular) cross-correlation of the two real signals.
    std::vector<float> dataC ((size_t) fftSize * 2, 0.0f);
    for (int bin = 0; bin < fftSize; ++bin)
    {
        const float reA = dataA[(size_t) bin * 2],     imA = dataA[(size_t) bin * 2 + 1];
        const float reB = dataB[(size_t) bin * 2],     imB = dataB[(size_t) bin * 2 + 1];
        dataC[(size_t) bin * 2]     = reA * reB + imA * imB;
        dataC[(size_t) bin * 2 + 1] = imA * reB - reA * imB;
    }

    fft.performRealOnlyInverseTransform (dataC.data());

    // Search only a small window of lags near zero (both directions,
    // the negative side wrapping to the end of the circular result) --
    // any real codec's algorithmic delay is tiny next to fftSize.
    int bestLagMag = 0;
    float bestScore = -1.0f;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        const float v = std::abs (dataC[(size_t) lag]);
        if (v > bestScore) { bestScore = v; bestLagMag = lag; }

        if (lag > 0)
        {
            const float vNeg = std::abs (dataC[(size_t) (fftSize - lag)]);
            if (vNeg > bestScore) { bestScore = vNeg; bestLagMag = lag; }
        }
    }

    if (bestLagMag == 0)
        return 0;

    // Don't trust the sign convention of the correlation index -- confirm
    // it empirically by actually measuring which direction of shift (or no
    // shift at all) leaves the least residual energy.
    auto residualEnergy = [&] (int shift) -> double
    {
        const int dropA = juce::jmax (0, -shift);
        const int dropB = juce::jmax (0, shift);
        const int n = juce::jmin (windowSamples - dropA, windowSamples - dropB);
        if (n <= 0)
            return std::numeric_limits<double>::max();

        double sum = 0.0;
        const int chN = juce::jmin (bufferA.getNumChannels(), bufferB.getNumChannels());
        for (int ch = 0; ch < chN; ++ch)
        {
            const float* a = bufferA.getReadPointer (ch) + dropA;
            const float* b = bufferB.getReadPointer (ch) + dropB;
            for (int i = 0; i < n; ++i)
            {
                const double d = (double) b[i] - (double) a[i];
                sum += d * d;
            }
        }
        return sum;
    };

    const double eZero = residualEnergy (0);
    const double ePos  = residualEnergy (bestLagMag);
    const double eNeg  = residualEnergy (-bestLagMag);

    int chosen = 0;
    double best = eZero;
    // Only switch away from "no shift" if it clearly helps -- avoids acting
    // on a spurious peak when the two signals are already well aligned.
    if (ePos < best * 0.8) { best = ePos; chosen = bestLagMag; }
    if (eNeg < best * 0.8) { best = eNeg; chosen = -bestLagMag; }

    return chosen;
}

double SpotifyProcessor::buildBoostedDifference (const juce::AudioBuffer<float>& bufferA,
                                                   const juce::AudioBuffer<float>& bufferB,
                                                   double targetPeakDb,
                                                   juce::AudioBuffer<float>& outDiff,
                                                   int& outAlignmentSamples)
{
    const int shift = estimateCodecDelaySamples (bufferA, bufferB);
    outAlignmentSamples = shift;

    const int dropA = juce::jmax (0, -shift);
    const int dropB = juce::jmax (0, shift);

    const int numChannels = juce::jmin (bufferA.getNumChannels(), bufferB.getNumChannels());
    const int numSamples  = juce::jmin (bufferA.getNumSamples() - dropA, bufferB.getNumSamples() - dropB);

    if (numChannels <= 0 || numSamples <= 0)
    {
        outDiff.setSize (0, 0);
        return 0.0;
    }

    outDiff.setSize (numChannels, numSamples);

    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* a = bufferA.getReadPointer (ch) + dropA;
        const float* b = bufferB.getReadPointer (ch) + dropB;
        float* d = outDiff.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            d[i] = b[i] - a[i];
            peak = juce::jmax (peak, std::abs (d[i]));
        }
    }

    if (peak <= 0.0f || ! std::isfinite (peak))
        return 0.0;

    const double targetPeakLinear = std::pow (10.0, targetPeakDb / 20.0);
    double boostDb = 20.0 * std::log10 (targetPeakLinear / (double) peak);
    boostDb = juce::jlimit (0.0, 60.0, boostDb); // never attenuate; cap absurd boosts on near-silent diffs

    const float boostLinear = (float) std::pow (10.0, boostDb / 20.0);
    outDiff.applyGain (boostLinear);

    return boostDb;
}

SpotifyProcessor::Report SpotifyProcessor::process (const juce::File& inputFile, const juce::File& outputFile,
                                                      Target target, Quality quality)
{
    Report report;
    report.targetLUFS = targetToLUFS (target);
    report.qualityKbps = qualityToKbps (quality);

    try
    {
        if (! inputFile.existsAsFile())
        {
            report.errorMessage = "Input file does not exist.";
            return report;
        }

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (inputFile));
        if (reader == nullptr)
        {
            report.errorMessage = "Could not read input file (unsupported or corrupt): " + inputFile.getFullPathName();
            return report;
        }

        const double sampleRate = reader->sampleRate;
        const int numChannels = (int) reader->numChannels;
        const int numSamples = (int) reader->lengthInSamples;

        if (sampleRate <= 0.0 || numChannels <= 0 || numSamples <= 0)
        {
            report.errorMessage = "Input file has invalid sample rate, channel count, or no audio data.";
            return report;
        }

        report.sourceSampleRate = sampleRate;
        report.sourceBitsPerSample = (int) reader->bitsPerSample;
        report.sourceNumChannels = numChannels;
        report.durationSeconds = (double) numSamples / sampleRate;

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        if (! reader->read (&buffer, 0, numSamples, 0, true, true))
        {
            report.errorMessage = "Failed to read audio data from the input file.";
            return report;
        }
        reader.reset();

        // --- Step 1: measure input loudness -----------------------------
        auto inMeasure = LoudnessMeter::measure (buffer, sampleRate);
        report.inputLUFS = inMeasure.integratedLUFS;
        report.inputTruePeak = inMeasure.truePeakDbTP;
        report.inputSamplePeak = measureSamplePeakDb (buffer);

        // Stereo field, spectrum and waveform are captured from the
        // PRISTINE original buffer, before gain/limiter/codec touch it.
        report.isStereo = numChannels >= 2;
        measureStereo (buffer, report.inputMidDb, report.inputSideDb, report.inputCorrelation);
        report.spectrumFftSize = 1 << kSpectrumFftOrder;
        report.inputSpectrumDb = computeAverageSpectrumDb (buffer, kSpectrumFftOrder);
        computeWaveformPeaks (buffer, kWaveformColumns, report.inputWaveformMin, report.inputWaveformMax);

        // --- Step 2: Spotify-style gain match + limiter -------------------
        const double diff = report.targetLUFS - report.inputLUFS;
        const double appliedGain = diff; // always apply the full target gain
        report.appliedGainDb = appliedGain;

        const float linearGain = (float) std::pow (10.0, appliedGain / 20.0);
        buffer.applyGain (linearGain);

        if (appliedGain > 0.0)
        {
            // Boosting: protect the TRUE (inter-sample) peak with a real
            // limiter that runs at 4x oversampled rate, rather than just
            // pre-scaling the gain by an estimate or limiting on sample
            // peaks alone.
            const double reduction = applyTruePeakLimiter (buffer, sampleRate,
                                                             kLimiterCeilingDbTP, kLimiterAttackMs, kLimiterReleaseMs);
            report.limiterEngaged = reduction > 0.01;
            report.limiterGainReductionDb = reduction;
        }

        // Final safety net: never let sample peaks exceed 0 dBFS going
        // into the encoder, across ALL channels.
        float samplePeakLinear = buffer.getMagnitude (0, numSamples);
        if (! std::isfinite (samplePeakLinear))
        {
            report.errorMessage = "Processing produced invalid (NaN/Inf) audio data.";
            return report;
        }
        if (samplePeakLinear > 1.0f)
            buffer.applyGain (1.0f / samplePeakLinear);

        auto afterGainMeasure = LoudnessMeter::measure (buffer, sampleRate);
        report.afterGainLUFS = afterGainMeasure.integratedLUFS;
        report.afterGainTruePeak = afterGainMeasure.truePeakDbTP;
        report.afterGainSamplePeak = measureSamplePeakDb (buffer);

        // --- Step 3: real Ogg Vorbis encode -> decode round trip --------
        juce::File tempOgg = juce::File::createTempFile ("ps_spotify_sim.ogg");
        juce::OggVorbisAudioFormat oggFormat;

        {
            std::unique_ptr<juce::OutputStream> os (tempOgg.createOutputStream());
            if (os == nullptr)
            {
                report.errorMessage = "Could not create a temporary file for the codec round trip.";
                tempOgg.deleteFile();
                return report;
            }

            const int qualityIndex = qualityToOggIndex (quality, oggFormat.getQualityOptions().size());

            auto options = juce::AudioFormatWriterOptions()
                               .withSampleRate (sampleRate)
                               .withNumChannels (numChannels)
                               .withBitsPerSample (32)
                               .withQualityOptionIndex (qualityIndex);

            std::unique_ptr<juce::AudioFormatWriter> writer (oggFormat.createWriterFor (os, options));
            if (writer == nullptr)
            {
                report.errorMessage = "Could not create the Ogg Vorbis encoder.";
                tempOgg.deleteFile();
                return report;
            }

            if (! writer->writeFromAudioSampleBuffer (buffer, 0, numSamples))
            {
                report.errorMessage = "The Ogg Vorbis encoder failed while writing.";
                writer.reset();
                tempOgg.deleteFile();
                return report;
            }
            writer.reset(); // flush + close
        }

        std::unique_ptr<juce::AudioFormatReader> oggReader (oggFormat.createReaderFor (
            new juce::FileInputStream (tempOgg), true));
        if (oggReader == nullptr)
        {
            report.errorMessage = "Could not decode the Ogg Vorbis round-trip file.";
            tempOgg.deleteFile();
            return report;
        }

        const int decodedChannels = (int) oggReader->numChannels;
        const int decodedSamples = (int) oggReader->lengthInSamples;
        if (decodedChannels <= 0 || decodedSamples <= 0)
        {
            report.errorMessage = "The decoded audio from the codec round trip is empty.";
            oggReader.reset();
            tempOgg.deleteFile();
            return report;
        }

        juce::AudioBuffer<float> decoded (decodedChannels, decodedSamples);
        if (! oggReader->read (&decoded, 0, decodedSamples, 0, true, true))
        {
            report.errorMessage = "Failed to read back the decoded audio.";
            oggReader.reset();
            tempOgg.deleteFile();
            return report;
        }
        oggReader.reset();
        tempOgg.deleteFile();

        for (int ch = 0; ch < decodedChannels; ++ch)
            for (int i = 0; i < decodedSamples; ++i)
                if (! std::isfinite (decoded.getSample (ch, i)))
                {
                    report.errorMessage = "The codec round trip produced invalid (NaN/Inf) audio data.";
                    return report;
                }

        // --- Step 4: measure the real post-codec result ------------------
        auto outMeasure = LoudnessMeter::measure (decoded, sampleRate);
        report.outputLUFS = outMeasure.integratedLUFS;
        report.outputTruePeak = outMeasure.truePeakDbTP;
        report.outputSamplePeak = measureSamplePeakDb (decoded);

        measureStereo (decoded, report.outputMidDb, report.outputSideDb, report.outputCorrelation);
        report.outputSpectrumDb = computeAverageSpectrumDb (decoded, kSpectrumFftOrder);
        computeWaveformPeaks (decoded, kWaveformColumns, report.outputWaveformMin, report.outputWaveformMax);

        // --- Step 5: write the natural WAV output -------------------------
        // Written to a temp file first and only moved into place once
        // writing fully succeeds, so a failed write never leaves a partial
        // or corrupt file at the requested output path.
        juce::File tempWav = juce::File::createTempFile ("ps_spotify_sim_out.wav");

        {
            std::unique_ptr<juce::OutputStream> wavStream (tempWav.createOutputStream());
            if (wavStream == nullptr)
            {
                report.errorMessage = "Could not create a temporary file for the WAV output.";
                tempWav.deleteFile();
                return report;
            }

            juce::WavAudioFormat wavFormat;
            auto wavOptions = juce::AudioFormatWriterOptions()
                                   .withSampleRate (sampleRate)
                                   .withNumChannels (decodedChannels)
                                   .withBitsPerSample (24);

            std::unique_ptr<juce::AudioFormatWriter> wavWriter (wavFormat.createWriterFor (wavStream, wavOptions));
            if (wavWriter == nullptr)
            {
                report.errorMessage = "Could not create the WAV writer.";
                tempWav.deleteFile();
                return report;
            }

            if (! wavWriter->writeFromAudioSampleBuffer (decoded, 0, decodedSamples))
            {
                report.errorMessage = "Failed while writing the output WAV file.";
                wavWriter.reset();
                tempWav.deleteFile();
                return report;
            }
            wavWriter.reset();
        }

        outputFile.deleteFile();
        if (! tempWav.moveFileTo (outputFile))
        {
            report.errorMessage = "Could not move the finished WAV to the chosen output location.";
            tempWav.deleteFile();
            return report;
        }

        // --- Step 6: "what got lost" -- a boosted, audible difference ----
        // between the pre-codec (post gain/limiter) signal and the final
        // post-codec result. Non-fatal if this part fails: the main output
        // above has already succeeded.
        try
        {
            juce::AudioBuffer<float> diffBuffer;
            int alignmentSamples = 0;
            const double boostDb = buildBoostedDifference (buffer, decoded, -6.0, diffBuffer, alignmentSamples);

            if (diffBuffer.getNumSamples() > 0)
            {
                juce::File diffOut = outputFile.getSiblingFile (
                    outputFile.getFileNameWithoutExtension() + "_difference.wav");

                juce::File tempDiff = juce::File::createTempFile ("ps_spotify_sim_diff.wav");
                std::unique_ptr<juce::OutputStream> diffStream (tempDiff.createOutputStream());

                if (diffStream != nullptr)
                {
                    juce::WavAudioFormat wavFormat;
                    auto diffOptions = juce::AudioFormatWriterOptions()
                                            .withSampleRate (sampleRate)
                                            .withNumChannels (diffBuffer.getNumChannels())
                                            .withBitsPerSample (24);

                    std::unique_ptr<juce::AudioFormatWriter> diffWriter (wavFormat.createWriterFor (diffStream, diffOptions));
                    if (diffWriter != nullptr
                        && diffWriter->writeFromAudioSampleBuffer (diffBuffer, 0, diffBuffer.getNumSamples()))
                    {
                        diffWriter.reset();
                        diffOut.deleteFile();
                        if (tempDiff.moveFileTo (diffOut))
                        {
                            report.differenceAvailable = true;
                            report.differenceFile = diffOut;
                            report.differenceBoostDb = boostDb;
                            report.differenceAlignmentSamples = alignmentSamples;
                        }
                    }
                }
                tempDiff.deleteFile();
            }
        }
        catch (...)
        {
            // Difference file is a bonus, not the main deliverable -- stay silent and move on.
        }

        report.success = true;
        return report;
    }
    catch (const std::exception& e)
    {
        report.success = false;
        report.errorMessage = juce::String ("Unexpected error: ") + e.what();
        return report;
    }
    catch (...)
    {
        report.success = false;
        report.errorMessage = "Unexpected error during processing.";
        return report;
    }
}
