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
    const float radius = juce::jmin (16.0f, bounds.getHeight() * 0.3f); // spec: buttons ~15-17px radius

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
    const float glowAlpha = isAccented && ! isTransparent ? 0.45f : 0.0f;

    PSSkin::drawGlowRoundedRect (g, bounds, radius, top, bottom, base, glowAlpha);

    if (! isTransparent)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    }
}

static PSSkin::Icon iconForButtonText (const juce::String& text)
{
    if (text.startsWith ("Choose WAV")) return PSSkin::Icon::waveform;
    if (text == "Process")              return PSSkin::Icon::play;
    if (text.startsWith ("Save As"))    return PSSkin::Icon::save;
    return PSSkin::Icon::none;
}

void PSLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                     bool /*isHighlighted*/, bool /*isDown*/)
{
    const auto icon = iconForButtonText (button.getButtonText());
    const auto textColour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                         : juce::TextButton::textColourOffId)
                                   .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f);

    auto bounds = button.getLocalBounds().toFloat();
    const int yIndent = juce::jmin (4, button.proportionOfHeight (0.3f));

    if (icon == PSSkin::Icon::none)
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour (textColour);
        g.drawFittedText (button.getButtonText(), bounds.reduced (6.0f, (float) yIndent).toNearestInt(),
                           juce::Justification::centred, 2);
        return;
    }

    // Icon on the left, label next to it -- both treated as one centred
    // group so the pair sits in the middle of the button, matching the
    // reference mockup's icon+label buttons.
    const float iconBoxSize = bounds.getHeight() * 0.42f;
    const float gap = 10.0f;

    const auto font = getTextButtonFont (button, button.getHeight());
    g.setFont (font);
    const float textWidth = juce::GlyphArrangement::getStringWidth (font, button.getButtonText());
    const float groupWidth = iconBoxSize + gap + textWidth;
    const float startX = bounds.getCentreX() - groupWidth * 0.5f;

    juce::Rectangle<float> iconBox (startX, bounds.getCentreY() - iconBoxSize * 0.5f, iconBoxSize, iconBoxSize);
    PSSkin::drawIcon (g, iconBox, icon, textColour);

    juce::Rectangle<float> textArea (iconBox.getRight() + gap, bounds.getY(),
                                      bounds.getRight() - (iconBox.getRight() + gap) - 6.0f, bounds.getHeight());
    g.setColour (textColour);
    g.drawFittedText (button.getButtonText(), textArea.toNearestInt(), juce::Justification::centredLeft, 1);
}
