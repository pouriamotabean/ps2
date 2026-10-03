#pragma once
#include <JuceHeader.h>

// Loads the hand-picked skin texture images (compiled in via BinaryData, see
// CMakeLists.txt's PSBinaryData target) and provides small helpers to draw
// them into rounded panels / buttons / pills. The images supply only
// material/texture -- shape, rounding, clipping and borders all stay in
// code, drawn exactly to each component's real size. If an image ever fails
// to decode, every helper falls back to the original flat PSColours fill,
// so the UI never breaks because an asset is missing.
namespace PSSkin
{
    enum class ButtonState { normal, hover, pressed };

    const juce::Image& backgroundTexture();
    const juce::Image& panelTexture();
    const juce::Image& trackTexture();
    const juce::Image& selectedTexture();
    const juce::Image& buttonTexture (ButtonState state);

    // Status badge icons for the verdict summary: a clean white flag (all
    // good), a torn flag (something to look at), a burnt flag (go redo it).
    const juce::Image& flagGoodIcon();
    const juce::Image& flagWarnIcon();
    const juce::Image& flagCriticalIcon();

    // The "PS" wordmark used in the header, replacing the plain code-drawn
    // title text.
    const juce::Image& logoImage();

    // Stretches `image` to fill `bounds` and clips to a rounded rect of
    // `cornerRadius`. If image is invalid, fills with `fallbackColour` instead.
    void drawTexturedRoundedRect (juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float cornerRadius, const juce::Image& image,
                                   juce::Colour fallbackColour);

    // Fills the whole given area with `image`, stretched, no rounding/clip
    // (for full-window backgrounds). Falls back to a flat fill.
    void drawStretchedBackground (juce::Graphics& g, juce::Rectangle<int> bounds,
                                   const juce::Image& image, juce::Colour fallbackColour);

    // A flat, code-drawn rounded rect with a soft vertical gradient and an
    // optional soft outer glow (glowAlpha 0 = no glow) -- the "boutique
    // plugin" look: a selected control or the verdict box glows in the
    // accent colour instead of relying on a baked texture image, so the
    // glow colour always matches the current theme exactly.
    void drawGlowRoundedRect (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                               juce::Colour topColour, juce::Colour bottomColour,
                               juce::Colour glowColour, float glowAlpha);

    // Small leading glyphs drawn inside a few key buttons ("Choose WAV...",
    // "Process", "Save As...") for a more premium, iconography-rich look,
    // matching the reference mockup -- code-drawn so they always match the
    // live theme colour, same reasoning as drawGlowRoundedRect above.
    enum class Icon { none, waveform, play, save };
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> box, Icon icon, juce::Colour colour);
}
