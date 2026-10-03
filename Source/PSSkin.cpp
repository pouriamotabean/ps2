#include "PSSkin.h"
#include "PSTheme.h"
#include "BinaryData.h"

namespace PSSkin
{
    static juce::Image loadFrom (const char* data, int size)
    {
        if (data == nullptr || size <= 0)
            return {};
        return juce::ImageCache::getFromMemory (data, size);
    }

    const juce::Image& backgroundTexture()
    {
        static juce::Image img = loadFrom (PSBinaryData::bg_png, PSBinaryData::bg_pngSize);
        return img;
    }

    const juce::Image& panelTexture()
    {
        static juce::Image img = loadFrom (PSBinaryData::panel_png, PSBinaryData::panel_pngSize);
        return img;
    }

    const juce::Image& trackTexture()
    {
        static juce::Image img = loadFrom (PSBinaryData::track_png, PSBinaryData::track_pngSize);
        return img;
    }

    const juce::Image& selectedTexture()
    {
        static juce::Image img = loadFrom (PSBinaryData::selected_png, PSBinaryData::selected_pngSize);
        return img;
    }

    const juce::Image& buttonTexture (ButtonState state)
    {
        static juce::Image normalImg  = loadFrom (PSBinaryData::button_normal_png,  PSBinaryData::button_normal_pngSize);
        static juce::Image hoverImg   = loadFrom (PSBinaryData::button_hover_png,   PSBinaryData::button_hover_pngSize);
        static juce::Image pressedImg = loadFrom (PSBinaryData::button_pressed_png, PSBinaryData::button_pressed_pngSize);

        switch (state)
        {
            case ButtonState::hover:   return hoverImg;
            case ButtonState::pressed: return pressedImg;
            case ButtonState::normal:
            default:                   return normalImg;
        }
    }

    const juce::Image& flagGoodIcon()
    {
        static juce::Image img = loadFrom (PSBinaryData::flag_good_png, PSBinaryData::flag_good_pngSize);
        return img;
    }

    const juce::Image& flagWarnIcon()
    {
        static juce::Image img = loadFrom (PSBinaryData::flag_warn_png, PSBinaryData::flag_warn_pngSize);
        return img;
    }

    const juce::Image& flagCriticalIcon()
    {
        static juce::Image img = loadFrom (PSBinaryData::flag_critical_png, PSBinaryData::flag_critical_pngSize);
        return img;
    }

    const juce::Image& logoImage()
    {
        static juce::Image img = loadFrom (PSBinaryData::logo_png, PSBinaryData::logo_pngSize);
        return img;
    }

    void drawTexturedRoundedRect (juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float cornerRadius, const juce::Image& image,
                                   juce::Colour fallbackColour)
    {
        if (image.isValid())
        {
            juce::Path p;
            p.addRoundedRectangle (bounds, cornerRadius);
            g.saveState();
            g.reduceClipRegion (p);
            g.drawImage (image, bounds);
            g.restoreState();
        }
        else
        {
            g.setColour (fallbackColour);
            g.fillRoundedRectangle (bounds, cornerRadius);
        }
    }

    void drawStretchedBackground (juce::Graphics& g, juce::Rectangle<int> bounds,
                                   const juce::Image& image, juce::Colour fallbackColour)
    {
        if (image.isValid())
            g.drawImage (image, bounds.toFloat());
        else
            g.fillAll (fallbackColour);
    }

    void drawGlowRoundedRect (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                               juce::Colour topColour, juce::Colour bottomColour,
                               juce::Colour glowColour, float glowAlpha)
    {
        juce::Path p;
        p.addRoundedRectangle (bounds, cornerRadius);

        if (glowAlpha > 0.0f)
        {
            juce::DropShadow glow (glowColour.withAlpha (glowAlpha),
                                    juce::jmax (4, (int) (bounds.getHeight() * 0.9f)), {});
            glow.drawForPath (g, p);
        }

        juce::ColourGradient grad (topColour, bounds.getX(), bounds.getY(),
                                    bottomColour, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (p);
    }
}
