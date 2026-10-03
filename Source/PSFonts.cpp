#include "PSFonts.h"
#include "BinaryData.h"

namespace PSFonts
{
    static juce::Typeface::Ptr loadTypeface (const char* data, int size)
    {
        if (data == nullptr || size <= 0)
            return nullptr;
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    // JUCE's binary-data name mangling converts "." to "_" but strips
    // hyphens outright rather than converting them to underscores -- so
    // "Inter-Variable.ttf" becomes "InterVariable_ttf", not
    // "Inter_Variable_ttf".
    static const juce::Typeface::Ptr& interTypeface()
    {
        static juce::Typeface::Ptr tf = loadTypeface (PSBinaryData::InterVariable_ttf,
                                                        PSBinaryData::InterVariable_ttfSize);
        return tf;
    }

    static const juce::Typeface::Ptr& monoRegularTypeface()
    {
        static juce::Typeface::Ptr tf = loadTypeface (PSBinaryData::JetBrainsMonoRegular_ttf,
                                                        PSBinaryData::JetBrainsMonoRegular_ttfSize);
        return tf;
    }

    static const juce::Typeface::Ptr& monoBoldTypeface()
    {
        static juce::Typeface::Ptr tf = loadTypeface (PSBinaryData::JetBrainsMonoBold_ttf,
                                                        PSBinaryData::JetBrainsMonoBold_ttfSize);
        return tf;
    }

    juce::Font ui (float size, bool bold)
    {
        auto tf = interTypeface();
        if (tf == nullptr)
            return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));

        // Inter is embedded as a single (variable, default-weight) file, so
        // bold is applied as a synthetic embolden rather than swapping to a
        // separate bold file -- boldened() works regardless of typeface.
        auto f = juce::Font (juce::FontOptions (tf).withHeight (size));
        return bold ? f.boldened() : f;
    }

    juce::Font mono (float size, bool bold)
    {
        auto tf = bold ? monoBoldTypeface() : monoRegularTypeface();
        if (tf == nullptr)
            return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));

        return juce::Font (juce::FontOptions (tf).withHeight (size));
    }
}
