#include "WaveformDisplay.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"
#include <cmath>

WaveformDisplay::WaveformDisplay() = default;

void WaveformDisplay::setData (const std::vector<float>& inMin, const std::vector<float>& inMax,
                                const std::vector<float>& outMin, const std::vector<float>& outMax)
{
    inputMin = inMin; inputMax = inMax;
    outputMin = outMin; outputMax = outMax;
    hasData = ! inputMin.empty() && ! outputMin.empty();
    setMouseCursor (hasData ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void WaveformDisplay::clear()
{
    hasData = false;
    playheadPosition = -1.0f;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void WaveformDisplay::setPlayheadPosition (float normalized)
{
    // Called continuously (~30x/sec) during playback -- skip the repaint
    // when the position hasn't meaningfully moved (paused/stopped) so this
    // isn't needlessly redrawing every frame for nothing.
    if (std::abs (normalized - playheadPosition) < 0.0005f)
        return;

    playheadPosition = normalized;
    repaint();
}

void WaveformDisplay::seekFromMouse (const juce::MouseEvent& e)
{
    if (! hasData || onSeek == nullptr)
        return;

    // Same reduced() margin paint() uses for the plotted area, so a click
    // lines up with what's actually drawn under the cursor.
    auto area = getLocalBounds().toFloat().reduced (14.0f, 10.0f);
    if (area.getWidth() <= 0.0f)
        return;

    const float normalized = juce::jlimit (0.0f, 1.0f, (float) (e.position.x - area.getX()) / area.getWidth());
    onSeek (normalized);
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e) { seekFromMouse (e); }
void WaveformDisplay::mouseDrag (const juce::MouseEvent& e) { seekFromMouse (e); }

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
    PSSkin::drawGlowRoundedRect (g, bounds, 18.0f, PSColours::raised.brighter (0.04f), PSColours::raised.darker (0.08f), PSColours::raised, 0.0f);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 18.0f, 1.0f);

    auto area = getLocalBounds().toFloat().reduced (14.0f, 10.0f);

    if (! hasData)
    {
        g.setFont (PSFonts::ui (12.5f, true));
        g.setColour (PSColours::gold);
        g.drawText ("WAVEFORM", area.removeFromTop (16.0f), juce::Justification::centredLeft);
        return;
    }

    const float stripH = (area.getHeight() - 6.0f) * 0.5f;
    auto topStrip = area.removeFromTop (stripH);
    area.removeFromTop (6.0f);
    auto botStrip = area;

    drawStrip (g, topStrip, "ORIGINAL", inputMin, inputMax, PSColours::textDim.withAlpha (0.9f));
    drawStrip (g, botStrip, "PROCESSED", outputMin, outputMax, PSColours::accentHi);

    // Playhead: a single bright line across both strips, in the same
    // coordinate space seekFromMouse() uses, so it lines up exactly with
    // where a click would land.
    if (playheadPosition >= 0.0f)
    {
        auto fullArea = getLocalBounds().toFloat().reduced (14.0f, 10.0f);
        const float x = fullArea.getX() + playheadPosition * fullArea.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawVerticalLine ((int) x, fullArea.getY(), fullArea.getBottom());

        juce::Path head;
        head.addTriangle (x - 4.0f, fullArea.getY(), x + 4.0f, fullArea.getY(), x, fullArea.getY() + 6.0f);
        g.setColour (juce::Colours::white);
        g.fillPath (head);
    }
}
