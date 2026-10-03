#include "DetailsPanel.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"

DetailsPanel::DetailsPanel()
{
    addAndMakeVisible (spectrumDisplay);
}

void DetailsPanel::setReport (const SpotifyProcessor::Report& r)
{
    report = r;
    hasReport = true;
    spectrumDisplay.setData (report.inputSpectrumDb, report.outputSpectrumDb,
                              report.sourceSampleRate, report.spectrumFftSize);
    repaint();
}

void DetailsPanel::clear()
{
    hasReport = false;
    spectrumDisplay.clear();
    repaint();
}

void DetailsPanel::resized()
{
    // The numeric text block (file info, target/gain/limiter, tables,
    // difference note) is hand-painted in paint() above; the spectrum
    // graph is a real child component placed right below it. Both use
    // the same top offset so the label drawn in paint() lines up with
    // this component's bounds.
    auto area = getLocalBounds().reduced (18, 12);
    area.removeFromTop (kTextBlockHeight);
    area.removeFromTop (16); // "SPECTRUM" label row, drawn in paint()
    spectrumDisplay.setBounds (area.removeFromTop (160));
}

void DetailsPanel::drawRow (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                             const juce::String& lufs, const juce::String& sp, const juce::String& tp,
                             bool bold, juce::Colour labelColour) const
{
    auto labelArea = area.removeFromLeft (juce::roundToInt ((float) area.getWidth() * 0.34f));
    auto lufsArea  = area.removeFromLeft (juce::roundToInt ((float) area.getWidth() * 0.40f));
    auto spArea    = area.removeFromLeft (juce::roundToInt ((float) area.getWidth() * 0.5f));
    auto tpArea    = area;

    g.setFont (PSFonts::ui (12.5f, bold));
    g.setColour (labelColour);
    g.drawText (label, labelArea, juce::Justification::centredLeft);

    g.setFont (PSFonts::mono (12.0f, bold));
    g.setColour (bold ? PSColours::text : PSColours::text.withAlpha (0.92f));
    g.drawText (lufs, lufsArea, juce::Justification::centredLeft);
    g.drawText (sp,   spArea,   juce::Justification::centredLeft);
    g.drawText (tp,   tpArea,   juce::Justification::centredLeft);
}

void DetailsPanel::paint (juce::Graphics& g)
{
    // This panel floats as an overlay on top of the rest of the UI (see
    // MainComponent::toggleDetails()) rather than pushing the layout down,
    // so it needs to visibly separate itself from whatever's behind it: a
    // slightly brighter/more elevated fill than ordinary panels, a real
    // glow, and a crisper accent border.
    auto bounds = getLocalBounds().toFloat();
    PSSkin::drawGlowRoundedRect (g, bounds, 14.0f, PSColours::raised2.brighter (0.05f),
                                  PSColours::raised2.darker (0.08f), PSColours::accentHi, 0.35f);
    g.setColour (PSColours::accentHi.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 14.0f, 1.4f);

    auto area = getLocalBounds().reduced (18, 12);

    if (! hasReport)
    {
        g.setFont (PSFonts::ui (13.0f, false));
        g.setColour (PSColours::textDim);
        g.drawText ("Process a file to see the numbers behind the verdict.", area, juce::Justification::centredLeft);
        return;
    }

    // --- File info line -----------------------------------------------
    auto fileLine = area.removeFromTop (16);
    int mins = (int) (report.durationSeconds / 60.0);
    int secs = (int) report.durationSeconds % 60;
    juce::String fileInfo = juce::String (report.sourceSampleRate / 1000.0, 1) + " kHz / "
                           + juce::String (report.sourceBitsPerSample) + "-bit / "
                           + (report.sourceNumChannels >= 2 ? "Stereo" : "Mono") + " / "
                           + juce::String (mins) + ":" + juce::String (secs).paddedLeft ('0', 2);
    g.setFont (PSFonts::ui (11.5f, false));
    g.setColour (PSColours::textDim);
    g.drawText (fileInfo, fileLine, juce::Justification::centredLeft);

    area.removeFromTop (8);

    // --- Header: target + applied gain + limiter -----------------------
    auto top = area.removeFromTop (22);
    g.setFont (PSFonts::ui (13.0f, true));
    g.setColour (PSColours::gold);
    g.drawText ("TARGET  " + juce::String (report.targetLUFS, 1) + " LUFS",
                top.removeFromLeft (area.getWidth() * 0.4f), juce::Justification::centredLeft);

    auto gainColour = report.appliedGainDb > 0.01  ? PSColours::good
                     : report.appliedGainDb < -0.01 ? PSColours::warn
                                                      : PSColours::textDim;
    juce::String gainText = (report.appliedGainDb >= 0 ? "+" : "") + juce::String (report.appliedGainDb, 2) + " dB";
    g.setFont (PSFonts::mono (13.0f, true));
    g.setColour (gainColour);
    g.drawText (gainText, top, juce::Justification::centredRight);

    auto limiterLine = area.removeFromTop (16);
    g.setFont (PSFonts::ui (11.5f, false));
    g.setColour (report.limiterEngaged ? PSColours::warn : PSColours::textDim);
    juce::String limiterText = report.limiterEngaged
        ? "Limiter engaged: -" + juce::String (report.limiterGainReductionDb, 2) + " dB gain reduction (ceiling "
              + juce::String (SpotifyProcessor::kLimiterCeilingDbTP, 1) + " dBTP)"
        : "Limiter: not needed";
    g.drawText (limiterText, limiterLine, juce::Justification::centredLeft);

    area.removeFromTop (8);
    g.setColour (PSColours::border);
    g.drawLine ((float) area.getX(), (float) area.getY(), (float) area.getRight(), (float) area.getY(), 1.0f);
    area.removeFromTop (8);

    // --- Column headers ---------------------------------------------------
    drawRow (g, area.removeFromTop (16), "", "LUFS", "SAMPLE PK", "TRUE PK", true, PSColours::textDim);
    area.removeFromTop (3);

    const int rowH = 22;
    drawRow (g, area.removeFromTop (rowH), "Original",
             juce::String (report.inputLUFS, 1),
             juce::String (report.inputSamplePeak, 1),
             juce::String (report.inputTruePeak, 1) + " dBTP",
             false, PSColours::textDim);
    drawRow (g, area.removeFromTop (rowH), "After gain + limiter",
             juce::String (report.afterGainLUFS, 1),
             juce::String (report.afterGainSamplePeak, 1),
             juce::String (report.afterGainTruePeak, 1) + " dBTP",
             false, PSColours::textDim);
    drawRow (g, area.removeFromTop (rowH), "After " + juce::String (report.qualityKbps) + " kbps codec",
             juce::String (report.outputLUFS, 1),
             juce::String (report.outputSamplePeak, 1),
             juce::String (report.outputTruePeak, 1) + " dBTP",
             true, PSColours::accentHi);

    area.removeFromTop (10);
    g.setColour (PSColours::border);
    g.drawLine ((float) area.getX(), (float) area.getY(), (float) area.getRight(), (float) area.getY(), 1.0f);
    area.removeFromTop (8);

    // --- Stereo field ---------------------------------------------------
    if (report.isStereo)
    {
        drawRow (g, area.removeFromTop (16), "", "MID", "SIDE", "CORRELATION", true, PSColours::textDim);
        area.removeFromTop (3);
        drawRow (g, area.removeFromTop (rowH), "Original",
                 juce::String (report.inputMidDb, 1) + " dB",
                 juce::String (report.inputSideDb, 1) + " dB",
                 juce::String (report.inputCorrelation, 2),
                 false, PSColours::textDim);
        drawRow (g, area.removeFromTop (rowH), "Processed",
                 juce::String (report.outputMidDb, 1) + " dB",
                 juce::String (report.outputSideDb, 1) + " dB",
                 juce::String (report.outputCorrelation, 2),
                 true, PSColours::accentHi);
    }
    else
    {
        g.setFont (PSFonts::ui (11.5f, false));
        g.setColour (PSColours::textDim);
        g.drawText ("Mono source - stereo field metrics not applicable.", area.removeFromTop (18), juce::Justification::centredLeft);
    }

    area.removeFromTop (8);

    if (report.differenceAvailable)
    {
        g.setFont (PSFonts::ui (11.0f, true));
        g.setColour (PSColours::accentHi);
        juce::String alignNote = report.differenceAlignmentSamples != 0
            ? ", auto-aligned " + juce::String (std::abs (report.differenceAlignmentSamples)) + " samples"
            : "";
        g.drawText ("Difference file saved (" + report.differenceFile.getFileName() + ", boosted +"
                        + juce::String (report.differenceBoostDb, 1) + " dB" + alignNote
                        + ") - press C below to listen.",
                    area.removeFromTop (16), juce::Justification::centredLeft);
    }

    // The spectrum graph itself is a real child component (see resized());
    // just draw its label directly above it, using the same fixed offset
    // resized() uses so the two always line up regardless of how much of
    // the text block above actually got drawn.
    auto fullArea = getLocalBounds().reduced (18, 12);
    fullArea.removeFromTop (kTextBlockHeight);
    g.setFont (PSFonts::ui (11.5f, true));
    g.setColour (PSColours::gold);
    g.drawText ("SPECTRUM", fullArea.removeFromTop (16), juce::Justification::centredLeft);
}
