#include "PSLookAndFeel.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

PSLookAndFeel::PSLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, PSColours::bg);
    setColour (juce::TextButton::buttonColourId, PSColours::raised);
    setColour (juce::TextButton::textColourOffId, PSColours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

juce::Font PSLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return PSFonts::ui (juce::jmin (16.0f, (float) buttonHeight * 0.42f), true);
}

void PSLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                           bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = juce::jmin (10.0f, bounds.getHeight() * 0.3f);

    auto base = backgroundColour;
    if (isDown)              base = base.darker (0.2f);
    else if (isHighlighted)  base = base.brighter (0.1f);

    // A button whose colour was explicitly set away from the plain
    // "raised" default (the big accent CTA, a selected A/B source button)
    // reads as the accented / glowing control; ordinary buttons stay flat.
    const bool isAccented = backgroundColour != PSColours::raised;
    const bool isTransparent = backgroundColour.getAlpha() == 0;

    auto top    = isAccented ? base.brighter (0.18f) : base.brighter (0.06f);
    auto bottom = isAccented ? base.darker (0.12f)   : base.darker (0.06f);
    const float glowAlpha = isAccented && ! isTransparent ? 0.35f : 0.0f;

    PSSkin::drawGlowRoundedRect (g, bounds, radius, top, bottom, base, glowAlpha);

    if (! isTransparent)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    }
}
