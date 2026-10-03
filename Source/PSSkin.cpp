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

    void drawIcon (juce::Graphics& g, juce::Rectangle<float> box, Icon icon, juce::Colour colour)
    {
        if (icon == Icon::none)
            return;

        g.setColour (colour);
        auto b = box.reduced (box.getWidth() * 0.08f, box.getHeight() * 0.08f);

        switch (icon)
        {
            case Icon::waveform:
            {
                // Four bars of varying height, like a tiny level meter --
                // echoes the "Choose WAV..." button's purpose at a glance.
                const int numBars = 4;
                const float gap = b.getWidth() * 0.12f;
                const float barW = (b.getWidth() - gap * (numBars - 1)) / (float) numBars;
                const float heights[numBars] = { 0.45f, 0.9f, 0.65f, 1.0f };
                for (int i = 0; i < numBars; ++i)
                {
                    const float h = b.getHeight() * heights[(size_t) i];
                    juce::Rectangle<float> bar (b.getX() + i * (barW + gap), b.getBottom() - h, barW, h);
                    g.fillRoundedRectangle (bar, barW * 0.4f);
                }
                break;
            }
            case Icon::play:
            {
                juce::Path tri;
                tri.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
                g.fillPath (tri);
                break;
            }
            case Icon::save:
            {
                // A minimal "save" glyph: a downward arrow into a tray --
                // reads clearly at small sizes, unlike a detailed floppy disk.
                const float stemX = b.getCentreX();
                const float arrowTopY = b.getY();
                const float arrowTipY = b.getY() + b.getHeight() * 0.62f;
                const float headW = b.getWidth() * 0.34f;

                juce::Path arrow;
                arrow.startNewSubPath (stemX, arrowTopY);
                arrow.lineTo (stemX, arrowTipY);
                g.strokePath (arrow, juce::PathStrokeType (juce::jmax (1.6f, b.getWidth() * 0.14f),
                                                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                juce::Path head;
                head.addTriangle (stemX - headW, arrowTipY - headW * 0.85f,
                                   stemX + headW, arrowTipY - headW * 0.85f,
                                   stemX, arrowTipY + headW * 0.15f);
                g.fillPath (head);

                juce::Rectangle<float> tray (b.getX(), b.getBottom() - b.getHeight() * 0.14f,
                                              b.getWidth(), b.getHeight() * 0.14f);
                g.fillRoundedRectangle (tray, tray.getHeight() * 0.5f);
                break;
            }
            default: break;
        }
    }
}
