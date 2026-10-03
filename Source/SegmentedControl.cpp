#include "SegmentedControl.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

SegmentedControl::SegmentedControl (std::vector<Segment> segmentsIn)
    : segments (std::move (segmentsIn))
{
    setInterceptsMouseClicks (true, false);
}

void SegmentedControl::setSelectedIndex (int index, juce::NotificationType notify)
{
    if (index < 0 || index >= (int) segments.size() || index == selected)
        return;

    selected = index;
    repaint();

    if (notify != juce::dontSendNotification && onChange)
        onChange (selected);
}

juce::Rectangle<float> SegmentedControl::segmentBounds (int index) const
{
    auto b = getLocalBounds().toFloat();
    const float w = b.getWidth() / (float) segments.size();
    return { b.getX() + w * (float) index, b.getY(), w, b.getHeight() };
}

void SegmentedControl::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float radius = bounds.getHeight() * 0.5f;

    g.setColour (PSColours::raised);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    for (int i = 0; i < (int) segments.size(); ++i)
    {
        auto segB = segmentBounds (i);
        const bool isSelected = (i == selected);
        const bool isHovered  = (i == hovered && ! isSelected);

        if (isSelected)
        {
            auto pill = segB.reduced (3.0f);
            PSSkin::drawGlowRoundedRect (g, pill, juce::jmax (0.0f, radius - 3.0f),
                                          PSColours::accentHi, PSColours::accent,
                                          PSColours::accentHi, 0.55f);
        }
        else if (isHovered)
        {
            g.setColour (PSColours::raised2);
            g.fillRoundedRectangle (segB.reduced (3.0f), juce::jmax (0.0f, radius - 3.0f));
        }

        if (i > 0)
        {
            g.setColour (PSColours::border.withAlpha (0.5f));
            g.drawLine (segB.getX(), segB.getY() + 6.0f, segB.getX(), segB.getBottom() - 6.0f, 1.0f);
        }

        const auto& seg = segments[(size_t) i];
        auto textArea = segB.reduced (4.0f);

        if (seg.subLabel.isNotEmpty())
        {
            auto topHalf = textArea.removeFromTop (textArea.getHeight() * 0.56f);

            g.setFont (PSFonts::ui (14.5f, true));
            g.setColour (isSelected ? juce::Colours::white : PSColours::text);
            g.drawText (seg.label, topHalf, juce::Justification::centred);

            g.setFont (PSFonts::mono (11.5f, false));
            g.setColour (isSelected ? juce::Colours::white.withAlpha (0.85f) : PSColours::textDim);
            g.drawText (seg.subLabel, textArea, juce::Justification::centred);
        }
        else
        {
            g.setFont (PSFonts::ui (14.5f, true));
            g.setColour (isSelected ? juce::Colours::white : PSColours::text);
            g.drawText (seg.label, textArea, juce::Justification::centred);
        }
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < (int) segments.size(); ++i)
    {
        if (segmentBounds (i).contains (e.position))
        {
            setSelectedIndex (i);
            break;
        }
    }
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    int newHover = -1;
    for (int i = 0; i < (int) segments.size(); ++i)
    {
        if (segmentBounds (i).contains (e.position))
        {
            newHover = i;
            break;
        }
    }

    if (newHover != hovered)
    {
        hovered = newHover;
        repaint();
    }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}
