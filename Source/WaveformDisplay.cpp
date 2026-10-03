#include "WaveformDisplay.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

WaveformDisplay::WaveformDisplay() = default;

void WaveformDisplay::setData (const std::vector<float>& inMin, const std::vector<float>& inMax,
                                const std::vector<float>& outMin, const std::vector<float>& outMax)
{
    inputMin = inMin; inputMax = inMax;
    outputMin = outMin; outputMax = outMax;
    hasData = ! inputMin.empty() && ! outputMin.empty();
    repaint();
}

void WaveformDisplay::clear()
{
    hasData = false;
    repaint();
}

void WaveformDisplay::drawStrip (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                                  const std::vector<float>& mn, const std::vector<float>& mx, juce::Colour colour) const
{
    g.setFont (PSFonts::ui (10.0f, true));
    g.setColour (PSColours::textDim);
    g.drawText (label, area.removeFromTop (14.0f), juce::Justification::centredLeft);

    const float midY = area.getCentreY();
    g.setColour (PSColours::border.withAlpha (0.5f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());

    if (mn.empty())
        return;

    const int numCols = (int) mn.size();
    const float halfH = area.getHeight() * 0.5f;

    g.setColour (colour);
    for (int i = 0; i < numCols; ++i)
    {
        const float x = area.getX() + area.getWidth() * ((float) i / (float) numCols);
        const float yTop = midY - juce::jlimit (0.0f, 1.0f, mx[(size_t) i]) * halfH;
        const float yBot = midY - juce::jlimit (-1.0f, 0.0f, mn[(size_t) i]) * halfH;
        g.drawVerticalLine ((int) x, juce::jmin (yTop, yBot), juce::jmax (yTop, yBot) + 1.0f);
    }
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    PSSkin::drawGlowRoundedRect (g, bounds, 10.0f, PSColours::raised.brighter (0.04f), PSColours::raised.darker (0.08f), PSColours::raised, 0.0f);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto area = getLocalBounds().toFloat().reduced (14.0f, 10.0f);

    if (! hasData)
    {
        g.setFont (PSFonts::ui (12.5f, true));
        g.setColour (PSColours::gold);
        g.drawText ("WAVEFORM", area.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setFont (PSFonts::ui (12.5f, false));
        g.setColour (PSColours::textDim);
        g.drawText ("Process a file to see the waveform.", area, juce::Justification::centred);
        return;
    }

    const float stripH = (area.getHeight() - 6.0f) * 0.5f;
    auto topStrip = area.removeFromTop (stripH);
    area.removeFromTop (6.0f);
    auto botStrip = area;

    drawStrip (g, topStrip, "ORIGINAL", inputMin, inputMax, PSColours::textDim.withAlpha (0.9f));
    drawStrip (g, botStrip, "PROCESSED", outputMin, outputMax, PSColours::accentHi);
}
