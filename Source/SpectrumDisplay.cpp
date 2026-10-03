#include "SpectrumDisplay.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

SpectrumDisplay::SpectrumDisplay() = default;

void SpectrumDisplay::setData (const std::vector<float>& inputDb, const std::vector<float>& outputDb,
                                double sampleRateIn, int fftSizeIn)
{
    inputSpectrum = inputDb;
    outputSpectrum = outputDb;
    sampleRate = sampleRateIn;
    fftSize = fftSizeIn;
    hasData = ! inputSpectrum.empty() && ! outputSpectrum.empty();
    repaint();
}

void SpectrumDisplay::clear()
{
    hasData = false;
    repaint();
}

float SpectrumDisplay::dbAtFrequency (const std::vector<float>& spectrum, float freqHz) const
{
    if (spectrum.empty() || sampleRate <= 0.0 || fftSize <= 0)
        return kMinDb;

    const float binHz = (float) sampleRate / (float) fftSize;
    float binPos = freqHz / binHz;
    binPos = juce::jlimit (0.0f, (float) spectrum.size() - 1.0f, binPos);

    const int b0 = (int) binPos;
    const int b1 = juce::jmin ((int) spectrum.size() - 1, b0 + 1);
    const float frac = binPos - (float) b0;
    return spectrum[(size_t) b0] + (spectrum[(size_t) b1] - spectrum[(size_t) b0]) * frac;
}

juce::Path SpectrumDisplay::buildPath (const std::vector<float>& spectrum, juce::Rectangle<float> area) const
{
    juce::Path p;
    const int steps = juce::jmax (2, (int) area.getWidth());
    const float logMin = std::log10 (kMinFreq);
    const float logMax = std::log10 (kMaxFreq);

    for (int i = 0; i < steps; ++i)
    {
        const float t = (float) i / (float) (steps - 1);
        const float freq = std::pow (10.0f, logMin + t * (logMax - logMin));
        const float db = juce::jlimit (kMinDb, kMaxDb, dbAtFrequency (spectrum, freq));
        const float x = area.getX() + t * area.getWidth();
        const float y = juce::jmap (db, kMinDb, kMaxDb, area.getBottom(), area.getY());

        if (i == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }
    return p;
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    PSSkin::drawGlowRoundedRect (g, bounds, 10.0f, PSColours::raised.brighter (0.04f), PSColours::raised.darker (0.08f), PSColours::raised, 0.0f);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto area = getLocalBounds().toFloat().reduced (14.0f, 12.0f);

    // Header / legend.
    auto header = area.removeFromTop (16.0f);
    g.setFont (PSFonts::ui (11.5f, true));
    g.setColour (PSColours::gold);
    g.drawText ("SPECTRUM", header.removeFromLeft (header.getWidth() * 0.5f), juce::Justification::centredLeft);

    g.setFont (PSFonts::ui (10.5f, false));
    auto legend = header;
    g.setColour (PSColours::textDim);
    g.drawText ("— Original", legend.removeFromLeft (legend.getWidth() * 0.5f), juce::Justification::centredRight);
    g.setColour (PSColours::accentHi);
    g.drawText ("— Processed", legend, juce::Justification::centredRight);

    area.removeFromTop (4.0f);

    if (! hasData)
    {
        g.setFont (PSFonts::ui (12.5f, false));
        g.setColour (PSColours::textDim);
        g.drawText ("Process a file to see the spectrum.", area, juce::Justification::centred);
        return;
    }

    // Grid: a few frequency gridlines with labels.
    const float gridFreqs[] = { 100.0f, 1000.0f, 10000.0f };
    const float logMin = std::log10 (kMinFreq);
    const float logMax = std::log10 (kMaxFreq);

    g.setFont (PSFonts::mono (9.5f, false));
    for (float f : gridFreqs)
    {
        const float t = (std::log10 (f) - logMin) / (logMax - logMin);
        const float x = area.getX() + t * area.getWidth();
        g.setColour (PSColours::border.withAlpha (0.6f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        g.setColour (PSColours::textDim);
        juce::String label = f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f);
        g.drawText (label, juce::Rectangle<float> (x - 15.0f, area.getBottom() - 12.0f, 30.0f, 12.0f),
                    juce::Justification::centred);
    }

    g.setColour (PSColours::textDim);
    g.drawRect (area, 1.0f);

    auto plotArea = area.reduced (0.0f, 0.0f).withTrimmedBottom (12.0f);

    g.setColour (PSColours::textDim.withAlpha (0.85f));
    g.strokePath (buildPath (inputSpectrum, plotArea), juce::PathStrokeType (1.4f));

    g.setColour (PSColours::accentHi);
    g.strokePath (buildPath (outputSpectrum, plotArea), juce::PathStrokeType (1.6f));
}
