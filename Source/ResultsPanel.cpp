#include "ResultsPanel.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

ResultsPanel::ResultsPanel() = default;

void ResultsPanel::setPlaceholder (const juce::String& text)
{
    hasReport = false;
    isError = false;
    placeholderText = text;
    repaint();
}

// Plain-language summary shown first and, by default, ONLY: "did I master
// this right for Spotify, and if not, what's wrong" -- the thing the user
// actually wants to know, not a screen full of numbers they have to
// interpret themselves. All the numbers behind this verdict live in
// DetailsPanel, which stays collapsed unless the user asks to see it.
//
// Deliberately NOT style-based: this never judges a number against a
// textbook target (e.g. "true peak should be -1dBTP"). A hot master with
// true peak at -0.1dBTP is not flagged just for being hot -- plenty of
// engineers master that way on purpose and it sounds fine. What IS flagged
// is actual, measurable damage: real clipping in the delivered file, real
// inter-sample overs, or limiting heavy enough to audibly reshape the
// transients. Severity scales with how bad that damage actually is.
enum class VerdictSeverity { good, info, warn, bad, critical };

struct VerdictItem { juce::String text; VerdictSeverity severity; };

static juce::Colour verdictColour (VerdictSeverity s)
{
    switch (s)
    {
        case VerdictSeverity::good:     return PSColours::good;
        case VerdictSeverity::critical: return PSColours::bad;
        case VerdictSeverity::bad:      return PSColours::bad;
        case VerdictSeverity::warn:     return PSColours::warn;
        case VerdictSeverity::info:
        default:                        return PSColours::textDim;
    }
}

static juce::String verdictPrefix (VerdictSeverity s)
{
    switch (s)
    {
        case VerdictSeverity::good:     return "OK  -  ";
        case VerdictSeverity::critical: return "GO REDO THIS  -  ";
        case VerdictSeverity::bad:      return "NEEDS WORK  -  ";
        case VerdictSeverity::warn:     return "NOTE  -  ";
        case VerdictSeverity::info:
        default:                        return "";
    }
}

// A lower streaming-quality tier inherently loses more to the codec --
// that's correct, expected behaviour (it's genuinely what that Spotify
// tier sounds like), not a sign of bad mastering. So the same dB of
// sample-peak/true-peak overshoot or HF loss is "normal" at Low and
// "unusual" at V.High. These scale the post-codec thresholds below by
// tier so only loss beyond what that tier's own codec physics already
// explains gets flagged -- NOT the limiter-reduction check, which happens
// before encoding and has nothing to do with the chosen quality tier.
static double codecToleranceScale (int qualityKbps)
{
    if (qualityKbps <= 24)  return 2.5;  // Low
    if (qualityKbps <= 96)  return 1.6;  // Normal
    if (qualityKbps <= 160) return 1.2;  // High
    return 1.0;                          // V.High -- baseline, tightest
}

static std::vector<VerdictItem> buildVerdict (const SpotifyProcessor::Report& r)
{
    std::vector<VerdictItem> items;
    const double tol = codecToleranceScale (r.qualityKbps);

    // The processing chain already enforces a hard "never exceed 0 dBFS
    // going into the encoder" safety net (see SpotifyProcessor::process,
    // step 2), so the signal handed to the codec never clips. Any peak
    // measured AFTER the real Ogg Vorbis decode is therefore pure lossy-
    // codec reconstruction overshoot -- a normal, expected artifact of any
    // lossy codec (Spotify's own encoding included), not something the
    // user's master did wrong. A sliver of overshoot is universal and
    // inaudible; only a genuinely large overshoot (relative to what this
    // quality tier normally produces) is worth a flag.
    if (r.outputSamplePeak > 1.0 * tol)
        items.push_back ({ "The delivered file measures a real sample-peak over ("
                                + juce::String (r.outputSamplePeak, 1) + " dBFS) after the codec round trip - "
                                + "this is large enough to risk audible distortion.",
                            VerdictSeverity::critical });
    else if (r.outputSamplePeak > 0.3 * tol)
        items.push_back ({ "The delivered file has a small sample-peak over ("
                                + juce::String (r.outputSamplePeak, 1) + " dBFS) after the codec round trip - "
                                + "likely inaudible, but a bit more than usual for this quality tier.",
                            VerdictSeverity::warn });
    // Below that: normal lossy-codec reconstruction overshoot for this tier, not worth a note.

    // Inter-sample ("true") peak overs in the delivered file. A small
    // overshoot after a lossy encode is expected and not a real clipping
    // risk on virtually all modern playback gear; only a clearly large
    // overshoot is flagged.
    if (r.outputTruePeak > 2.0 * tol)
        items.push_back ({ "True peak in the delivered file is "
                                + juce::String (r.outputTruePeak, 1)
                                + " dBTP - likely to clip on real playback hardware.",
                            VerdictSeverity::critical });
    else if (r.outputTruePeak > 1.0 * tol)
        items.push_back ({ "True peak in the delivered file is "
                                + juce::String (r.outputTruePeak, 1)
                                + " dBTP - some risk of clipping on certain playback gear.",
                            VerdictSeverity::bad });
    else if (r.outputTruePeak > 0.3 * tol)
        items.push_back ({ "True peak in the delivered file is "
                                + juce::String (r.outputTruePeak, 1)
                                + " dBTP - a bit more than usual for this quality tier, but low risk.",
                            VerdictSeverity::warn });
    // Below that: normal lossy-codec reconstruction overshoot for this tier, not worth a note.

    // Limiter gain reduction: how much the loudness boost actually reshaped
    // the dynamics. Thresholds are about audible severity, not taste.
    if (r.limiterEngaged)
    {
        if (r.limiterGainReductionDb > 15.0)
            items.push_back ({ "The limiter had to pull " + juce::String (r.limiterGainReductionDb, 1)
                                    + " dB - the output is very likely audibly squashed for this target.",
                                VerdictSeverity::critical });
        else if (r.limiterGainReductionDb > 8.0)
            items.push_back ({ "The limiter pulled " + juce::String (r.limiterGainReductionDb, 1)
                                    + " dB - dynamics/transients are being significantly reshaped for this target.",
                                VerdictSeverity::bad });
        else if (r.limiterGainReductionDb > 3.0)
            items.push_back ({ "Moderate limiting happened (" + juce::String (r.limiterGainReductionDb, 1)
                                    + " dB) - worth an A/B listen, may be fine.",
                                VerdictSeverity::warn });
        // Below 3dB: not worth mentioning -- effectively inaudible.
    }

    // High-frequency energy lost to the codec, above 10kHz, averaged from
    // the spectrum data already measured for the Spectrum view. A little
    // HF rolloff is normal for any lossy codec, so only flag real loss.
    if (r.spectrumFftSize > 0 && r.sourceSampleRate > 0.0
        && ! r.inputSpectrumDb.empty() && r.inputSpectrumDb.size() == r.outputSpectrumDb.size())
    {
        const double binHz = r.sourceSampleRate / (double) r.spectrumFftSize;
        const int startBin = juce::jlimit (0, (int) r.inputSpectrumDb.size() - 1, (int) (10000.0 / binHz));
        double sumDiff = 0.0;
        int count = 0;
        for (int b = startBin; b < (int) r.inputSpectrumDb.size(); ++b)
        {
            sumDiff += (double) r.outputSpectrumDb[(size_t) b] - (double) r.inputSpectrumDb[(size_t) b];
            ++count;
        }
        if (count > 0)
        {
            const double avgDiff = sumDiff / (double) count;
            if (avgDiff < -6.0 * tol)
                items.push_back ({ "Frequencies above 10kHz lost about " + juce::String (-avgDiff, 1)
                                        + " dB to the codec - likely audibly duller.",
                                    VerdictSeverity::bad });
            else if (avgDiff < -3.0 * tol)
                items.push_back ({ "Frequencies above 10kHz lost about " + juce::String (-avgDiff, 1)
                                        + " dB to the codec - a bit more than usual for this quality tier.",
                                    VerdictSeverity::warn });
            // Smaller than that is normal rolloff for this quality tier -- not worth a note.
        }
    }

    if (items.empty())
        items.push_back ({ "No real damage detected - this master holds up fine for Spotify at this target.",
                            VerdictSeverity::good });

    return items;
}

static VerdictSeverity overallSeverity (const std::vector<VerdictItem>& items)
{
    auto worst = VerdictSeverity::good;
    for (auto& item : items)
        if ((int) item.severity > (int) worst)
            worst = item.severity;
    return worst;
}

// Three icon shapes cover five severities: good gets a check, a minor
// note/warning gets an exclamation mark, and anything serious (needs work
// or go-redo-this) gets a cross -- the specific wording per line still
// carries the exact severity, the icon just gives an at-a-glance read.
// Drawn directly with Path/DropShadow rather than a baked image, so the
// glow colour always matches the live theme exactly.
static juce::Colour iconColourForOverall (VerdictSeverity s)
{
    switch (s)
    {
        case VerdictSeverity::good:     return PSColours::good;
        case VerdictSeverity::info:
        case VerdictSeverity::warn:     return PSColours::warn;
        case VerdictSeverity::bad:
        case VerdictSeverity::critical:
        default:                        return PSColours::bad;
    }
}

static void drawVerdictIcon (juce::Graphics& g, juce::Rectangle<float> bounds, VerdictSeverity s)
{
    auto colour = iconColourForOverall (s);
    auto circle = bounds.reduced (2.0f);

    juce::Path circlePath;
    circlePath.addEllipse (circle);
    juce::DropShadow glow (colour.withAlpha (0.45f), juce::jmax (6, (int) (circle.getWidth() * 0.7f)), {});
    glow.drawForPath (g, circlePath);

    g.setColour (colour.withAlpha (0.12f));
    g.fillEllipse (circle);
    g.setColour (colour);
    g.drawEllipse (circle, 2.0f);

    auto inner = circle.reduced (circle.getWidth() * 0.28f);
    const float strokeW = juce::jmax (2.2f, inner.getWidth() * 0.18f);

    if (s == VerdictSeverity::good)
    {
        juce::Path tick;
        tick.startNewSubPath (inner.getX(), inner.getCentreY());
        tick.lineTo (inner.getX() + inner.getWidth() * 0.38f, inner.getBottom());
        tick.lineTo (inner.getRight(), inner.getY());
        g.strokePath (tick, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (s == VerdictSeverity::bad || s == VerdictSeverity::critical)
    {
        juce::Path cross;
        cross.startNewSubPath (inner.getTopLeft());
        cross.lineTo (inner.getBottomRight());
        cross.startNewSubPath (inner.getTopRight());
        cross.lineTo (inner.getBottomLeft());
        g.strokePath (cross, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else // info / warn
    {
        const float barW = juce::jmax (2.5f, inner.getWidth() * 0.18f);
        g.fillRoundedRectangle ({ inner.getCentreX() - barW * 0.5f, inner.getY(), barW, inner.getHeight() * 0.58f }, barW * 0.5f);
        g.fillRoundedRectangle ({ inner.getCentreX() - barW * 0.5f, inner.getBottom() - barW, barW, barW }, barW * 0.5f);
    }
}

void ResultsPanel::setReport (const SpotifyProcessor::Report& r)
{
    report = r;
    hasReport = true;
    isError = ! r.success;
    verdictLineCount = isError ? 1 : (int) buildVerdict (report).size();
    repaint();
}

int ResultsPanel::getPreferredHeight() const
{
    if (! hasReport)
        return 64;
    if (isError)
        return 110;

    const int verdictLineH = 20;
    const int iconMin = 44;
    const int boxH = juce::jmax (iconMin + 16, verdictLineH * verdictLineCount + 16);
    return boxH + 24; // top/bottom padding from reduced(18,12)
}

void ResultsPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    PSSkin::drawGlowRoundedRect (g, bounds, 14.0f, PSColours::raised.brighter (0.04f), PSColours::raised.darker (0.08f), PSColours::raised, 0.0f);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.0f);

    auto area = getLocalBounds().reduced (18, 12);

    if (! hasReport)
    {
        g.setFont (PSFonts::ui (14.0f, false));
        g.setColour (PSColours::textDim);
        g.drawFittedText (placeholderText, area, juce::Justification::centredLeft, 2);
        return;
    }

    if (isError)
    {
        g.setFont (PSFonts::ui (14.0f, true));
        g.setColour (PSColours::bad);
        g.drawFittedText ("Something went wrong:\n" + report.errorMessage, area, juce::Justification::centredLeft, 4);
        return;
    }

    auto verdictItems = buildVerdict (report);
    const int verdictLineH = 20;
    const int iconSize = juce::jmax (44, verdictLineH * (int) verdictItems.size());

    auto iconArea = area.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);
    area.removeFromLeft (14);

    drawVerdictIcon (g, iconArea.toFloat(), overallSeverity (verdictItems));

    // Centre the text block vertically within the panel.
    const int textBlockH = verdictLineH * (int) verdictItems.size();
    auto textArea = area.withSizeKeepingCentre (area.getWidth(), textBlockH);

    for (auto& item : verdictItems)
    {
        g.setFont (PSFonts::ui (13.5f, true));
        g.setColour (verdictColour (item.severity));
        g.drawFittedText (verdictPrefix (item.severity) + item.text,
                           textArea.removeFromTop (verdictLineH), juce::Justification::centredLeft, 1);
    }
}
