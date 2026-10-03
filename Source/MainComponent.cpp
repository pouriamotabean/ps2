#include "MainComponent.h"
#include "PSTheme.h"
#include "PSSkin.h"
#include "PSFonts.h"
#include <cmath>
#include <vector>

// Fixed native/design width -- the whole UI is scaled as one unit by the
// host window (see Main.cpp's ScaleHost), not reflowed, so layout never
// needs to track the window's actual current width/height.
static constexpr int kNativeWidth = 960;
static constexpr int kLeftColumnWidth = 300;
static constexpr int kColumnGap = 24;

// Header: icon + big title + small tracked caption, all vertically centred
// in one row (see resized()), with a subtle divider under the whole thing.
static constexpr int kHeaderHeight = 80;

// Height of the left column's own content (load row through status
// label), used both to lay it out and, via jmax with the right column,
// to size the shared row both columns sit in. The three control groups
// (file picker, Target, Quality, Process/Save) are spaced progressively
// more generously top-to-bottom -- less gap right under the file picker,
// more breathing room between the groups themselves -- per request.
static constexpr int kLeftColumnHeight =
      40 + 6 + 18 + 10        // load button + file label + gap
    + 15 + 6 + 52 + 22        // target section
    + 15 + 6 + 52 + 24        // quality section
    + 44 + 10 + 16;           // process/save row + gap + status label

static constexpr int kRightColumnHeight =
      20 + 6 + 150             // waveform header (+ "More details" link) + waveform
    + 16 + 15 + 6 + 64;        // A/B listen

// The left column now lives inside its own card (see leftCardBounds in
// paint()), padded in from the card's edges rather than touching them.
// Only the top is padded on purpose -- the bottom is left to whatever
// slack is left over when the card has to grow to match the right
// column's height, so that slack reads as intentional card padding
// instead of orphaned background (see the comment in resized()).
static constexpr int kLeftCardHorizPad = 16;
static constexpr int kLeftCardTopPad = 16;

MainComponent::MainComponent()
    : targetControl ({ { "Loud", "-11 LUFS" }, { "Normal", "-14 LUFS" }, { "Quiet", "-19 LUFS" } }),
      qualityControl ({ { "Low", "24 kbps" }, { "Normal", "96 kbps" }, { "High", "160 kbps" }, { "V.High", "320 kbps" } })
{
    setLookAndFeel (&lookAndFeel);
    setSize (960, 600); // final size recomputed by updateHeight() below

    // Header: icon mark on the left, "Predict Spotify" as the big title next
    // to it, and a small tracked caption underneath -- per the design spec
    // (icon + title were previously stacked vertically; the spec puts the
    // title beside the icon instead, with the caption taking the old
    // "Predict Spotify" subtitle's place below). The icon itself is drawn
    // directly in paint() -- see headerIconBounds in MainComponent.h.
    titleLabel.setText ("Predict Spotify", juce::dontSendNotification);
    titleLabel.setFont (PSFonts::ui (23.0f, false));
    titleLabel.setColour (juce::Label::textColourId, PSColours::text);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // See waveformSectionLabel's comment in MainComponent.h: plain
    // juce::String(const char*) does not assume UTF-8, so the middle dot
    // needs the explicit CharPointer_UTF8 wrapper or it mojibakes to "Â·".
    subtitleLabel.setText (juce::String (juce::CharPointer_UTF8 ("MASTER  \xc2\xb7  ANALYZE  \xc2\xb7  STREAM")),
                            juce::dontSendNotification);
    subtitleLabel.setFont (PSFonts::ui (10.5f, false).withExtraKerningFactor (0.12f));
    subtitleLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    loadButton.onClick = [this] { chooseInputFile(); };
    addAndMakeVisible (loadButton);

    inputFileLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    inputFileLabel.setJustificationType (juce::Justification::centredLeft);
    inputFileLabel.setFont (PSFonts::ui (14.0f, false));
    addAndMakeVisible (inputFileLabel);

    auto setupSectionLabel = [] (juce::Label& l)
    {
        l.setFont (PSFonts::ui (12.5f, true).withExtraKerningFactor (0.1f));
        l.setColour (juce::Label::textColourId, PSColours::gold);
    };
    setupSectionLabel (targetSectionLabel);
    setupSectionLabel (qualitySectionLabel);
    setupSectionLabel (abSectionLabel);
    setupSectionLabel (waveformSectionLabel);
    // Target/Quality read as centred headings flanked by divider lines (see
    // drawSectionDividers() in paint()); the two right-column headers stay
    // left-aligned with a single trailing line instead, matching the spec.
    targetSectionLabel.setJustificationType (juce::Justification::centred);
    qualitySectionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (targetSectionLabel);
    addAndMakeVisible (qualitySectionLabel);
    addAndMakeVisible (abSectionLabel);
    addAndMakeVisible (waveformSectionLabel);

    targetControl.setSelectedIndex (1, juce::dontSendNotification);  // Normal
    qualityControl.setSelectedIndex (3, juce::dontSendNotification); // Very High
    addAndMakeVisible (targetControl);
    addAndMakeVisible (qualityControl);

    processButton.onClick = [this] { runProcessing(); };
    processButton.setColour (juce::TextButton::buttonColourId, PSColours::accent);
    processButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible (processButton);

    saveAsButton.onClick = [this] { chooseSaveLocation(); };
    saveAsButton.setEnabled (false); // enabled once a run succeeds
    addAndMakeVisible (saveAsButton);

    addAndMakeVisible (resultsPanel);
    waveformDisplay.onSeek = [this] (float normalizedX) { audioPlayer.seekToNormalizedPosition (normalizedX); };
    addAndMakeVisible (waveformDisplay);
    addAndMakeVisible (audioPlayer);

    detailsToggleButton.onClick = [this] { toggleDetails(); };
    detailsToggleButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    detailsToggleButton.setColour (juce::TextButton::textColourOffId, PSColours::accentHi);
    addAndMakeVisible (detailsToggleButton);
    addChildComponent (detailsPanel); // added but not visible until expanded

    setWantsKeyboardFocus (false);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setFont (PSFonts::ui (13.0f, false));
    statusLabel.setColour (juce::Label::textColourId, PSColours::accentHi);
    addAndMakeVisible (statusLabel);

    creditLabel.setFont (PSFonts::ui (11.0f, false));
    creditLabel.setColour (juce::Label::textColourId, PSColours::textDim);
    creditLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (creditLabel);

    updateHeight();

    startTimerHz (30); // drives the waveform playhead; see timerCallback()
}

void MainComponent::timerCallback()
{
    waveformDisplay.setPlayheadPosition (audioPlayer.getNormalizedPosition());
}

// A brushed-metal texture, drawn once into a cached Image rather than
// re-rolled every repaint. Kept procedural rather than a baked texture
// asset -- same reasoning as every other visual in this app (PSSkin,
// PSLookAndFeel): it has to stay pixel-crisp under the window's own
// continuous AffineTransform scaling, which a stretched bitmap would not.
//
// Real brushed metal is mostly-horizontal fine scratches (long streaks of
// slightly different brightness, smoothed so they blend into their
// neighbours) plus a very slow diagonal sheen, like a reflection sweeping
// across the surface, plus a little per-pixel sparkle on top. Composited
// as light-only (near-black background + a lighter grey overlay at low,
// varying alpha) since that's what reads as "a dark brushed panel", not
// flat isotropic noise.
void MainComponent::generateNoiseImage()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
        return;
    if (noiseImage.isValid() && noiseImage.getWidth() == w && noiseImage.getHeight() == h)
        return;

    noiseImage = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Image::BitmapData bitmap (noiseImage, juce::Image::BitmapData::writeOnly);
    juce::Random rng (0x50530002);

    // One brightness value per row, smoothed across neighbouring rows so
    // it reads as long horizontal scratches rather than per-pixel static.
    std::vector<float> rowBrush ((size_t) h);
    float prevRow = 0.0f;
    for (int y = 0; y < h; ++y)
    {
        const float raw = rng.nextFloat() * 2.0f - 1.0f; // -1..1
        prevRow = prevRow * 0.88f + raw * 0.12f;          // long streaks, not flicker
        rowBrush[(size_t) y] = prevRow;
    }

    const float sheenPeriod = (float) juce::jmax (200, w + h) * 0.9f;

    for (int y = 0; y < h; ++y)
    {
        const float brush = rowBrush[(size_t) y];
        for (int x = 0; x < w; ++x)
        {
            const float sparkle = rng.nextFloat() * 2.0f - 1.0f; // per-pixel fine grain
            const float sheen = std::sin ((float) (x + y) / sheenPeriod * juce::MathConstants<float>::twoPi);

            // Only the brighter side of each term contributes -- brushed
            // metal in a dark panel reads as faint light scratches on a
            // dark base, not dark-and-light bands either side of grey.
            const float highlight = juce::jmax (0.0f, brush) * 0.6f
                                   + juce::jmax (0.0f, sheen) * 0.5f
                                   + std::abs (sparkle) * 0.25f;

            const int alpha = juce::jlimit (0, 46, 6 + (int) (highlight * 46.0f));
            const int level = juce::jlimit (150, 255, 190 + (int) (sparkle * 30.0f));

            bitmap.setPixelColour (x, y, juce::Colour ((juce::uint8) level, (juce::uint8) level,
                                                          (juce::uint8) level, (juce::uint8) alpha));
        }
    }
}

void MainComponent::toggleDetails()
{
    detailsExpanded = ! detailsExpanded;
    detailsPanel.setVisible (detailsExpanded);
    if (detailsExpanded)
    {
        detailsPanel.toFront (false); // float above everything else, not just whatever was added after it
        detailsToggleButton.toFront (false); // ...but the close control stays clickable above it
    }
    detailsToggleButton.setButtonText (detailsExpanded ? "Hide details  v" : "More details  >");
    resized();
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& path : files)
    {
        juce::File f (path);
        auto ext = f.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac")
            return true;
    }
    return false;
}

void MainComponent::fileDragEnter (const juce::StringArray&, int, int)
{
    isDragHover = true;
    repaint();
}

void MainComponent::fileDragExit (const juce::StringArray&)
{
    isDragHover = false;
    repaint();
}

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    isDragHover = false;

    for (auto& path : files)
    {
        juce::File f (path);
        auto ext = f.getFileExtension().toLowerCase();
        if (f.existsAsFile() && (ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac"))
        {
            loadInputFile (f);
            break;
        }
    }
    repaint();
}

MainComponent::~MainComponent()
{
    setLookAndFeel (nullptr);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (PSColours::bg.brighter (0.02f), 0.0f, 0.0f,
                                              PSColours::bg.darker (0.2f), 0.0f, (float) getHeight(), false));
    g.fillRect (getLocalBounds());

    // Subtle radial vignette centred near the top -- a soft glow that
    // falls off toward the edges, instead of a completely flat fill, for
    // more depth (matches the reference mockup's background treatment).
    juce::ColourGradient vignette (PSColours::accent.withAlpha (0.05f), (float) getWidth() * 0.5f, 0.0f,
                                    PSColours::bg.withAlpha (0.0f), (float) getWidth() * 0.5f, (float) getHeight() * 0.7f, true);
    g.setGradientFill (vignette);
    g.fillRect (getLocalBounds());

    // Subtle grain over the whole background -- see generateNoiseImage().
    // Drawn before the main panel, so the panel's own opaque fill still
    // sits cleanly on top of it; the grain mainly shows in the margins and
    // the vignette area around the panel, where it reads as texture
    // instead of a perfectly flat gradient.
    if (noiseImage.isValid())
        g.drawImageAt (noiseImage, 0, 0);

    // Left controls get their OWN card (not one giant panel wrapping both
    // columns, which is what the old layout did) -- matching the
    // reference: a self-contained left card, and separate individual boxes
    // on the right (waveform panel, verdict panel; A/B stays un-boxed).
    // This is also what actually fixes the left/right height balance: the
    // verdict box on the right already visibly stretches to fill any
    // leftover height (it's a real drawn box), but the left side's old
    // "stretch" was just an invisible label -- so when the right column
    // was naturally taller, the left looked like it dead-ended in empty
    // background. Now that leftover space is inside this card too.
    auto leftCard = leftCardBounds.toFloat();
    PSSkin::drawGlowRoundedRect (g, leftCard, 20.0f, PSColours::panel.brighter (0.03f),
                                  PSColours::panel.darker (0.1f), PSColours::panel, 0.0f);
    g.setColour (PSColours::border);
    g.drawRoundedRectangle (leftCard, 20.0f, 1.0f);

    if (isDragHover)
    {
        auto dropBounds = getLocalBounds().reduced (18).withTrimmedTop (18 + kHeaderHeight).toFloat();
        g.setColour (PSColours::accentHi.withAlpha (0.9f));
        g.drawRoundedRectangle (dropBounds.reduced (2.0f), 20.0f, 2.5f);
        g.setFont (PSFonts::ui (18.0f, true));
        g.setColour (PSColours::accentHi);
        g.drawText ("Drop audio file to load", dropBounds, juce::Justification::centred);
    }

    // Header icon mark: a small glowing accent tile with a tiny waveform
    // glyph inside -- code-drawn (not the old baked PNG) so it stays
    // crisp at any scale, matching every other icon in this app.
    {
        auto iconBounds = headerIconBounds.toFloat();
        PSSkin::drawGlowRoundedRect (g, iconBounds, 10.0f, PSColours::accentHi, PSColours::accent, PSColours::accent, 0.35f);
        PSSkin::drawIcon (g, iconBounds.reduced (iconBounds.getWidth() * 0.24f), PSSkin::Icon::waveform, juce::Colours::white);
    }

    // Header: subtle divider under the icon+title row, and a small
    // low-contrast waveform glyph top-right -- both per the design spec.
    g.setColour (PSColours::border.withAlpha (0.6f));
    g.drawHorizontalLine (headerDividerBounds.getY(), (float) headerDividerBounds.getX(),
                           (float) headerDividerBounds.getRight());
    PSSkin::drawIcon (g, headerRightIconBounds.toFloat(), PSSkin::Icon::waveform, PSColours::textDim.withAlpha (0.7f));

    // Target/Quality section headings read as centred text flanked by
    // short divider lines (per spec); the right column's two headings stay
    // left-aligned with one trailing line instead -- see each label's own
    // setJustificationType() in the constructor for which is which.
    auto drawFlankingDividers = [&] (const juce::Label& label)
    {
        auto bounds = label.getBounds().toFloat();
        const float textW = juce::GlyphArrangement::getStringWidth (label.getFont(), label.getText());
        const float pad = 14.0f;
        const float cx = bounds.getCentreX();
        const float y = bounds.getCentreY();
        const float leftEnd = cx - textW * 0.5f - pad;
        const float rightStart = cx + textW * 0.5f + pad;

        g.setColour (PSColours::border);
        if (leftEnd > bounds.getX())
            g.drawHorizontalLine ((int) y, bounds.getX(), leftEnd);
        if (rightStart < bounds.getRight())
            g.drawHorizontalLine ((int) y, rightStart, bounds.getRight());
    };
    drawFlankingDividers (targetSectionLabel);
    drawFlankingDividers (qualitySectionLabel);

    auto drawTrailingDivider = [&] (const juce::Label& label, int rightEdge)
    {
        auto bounds = label.getBounds().toFloat();
        const float textW = juce::GlyphArrangement::getStringWidth (label.getFont(), label.getText());
        const float startX = bounds.getX() + textW + 14.0f;
        const float y = bounds.getCentreY();
        if (startX < (float) rightEdge)
        {
            g.setColour (PSColours::border);
            g.drawHorizontalLine ((int) y, startX, (float) rightEdge);
        }
    };
    drawTrailingDivider (waveformSectionLabel, detailsToggleButton.getBounds().getX() - 10);
    drawTrailingDivider (abSectionLabel, waveformDisplay.getBounds().getRight());

    // Thin glowing dividers flanking the credit line, fading out toward
    // the edges -- a small finishing touch from the reference mockup.
    {
        auto creditArea = getLocalBounds().toFloat().removeFromBottom (24.0f);
        const auto creditFont = PSFonts::ui (11.0f, false);
        const float textHalfWidth = juce::GlyphArrangement::getStringWidth (creditFont, creditLabel.getText()) * 0.5f + 16.0f;
        const float y = creditArea.getCentreY();
        const float lineInset = 60.0f;

        juce::ColourGradient leftGrad (PSColours::border.withAlpha (0.0f), lineInset, y,
                                        PSColours::accent.withAlpha (0.5f), creditArea.getCentreX() - textHalfWidth, y, false);
        g.setGradientFill (leftGrad);
        g.drawHorizontalLine ((int) y, lineInset, creditArea.getCentreX() - textHalfWidth);

        juce::ColourGradient rightGrad (PSColours::accent.withAlpha (0.5f), creditArea.getCentreX() + textHalfWidth, y,
                                         PSColours::border.withAlpha (0.0f), creditArea.getWidth() - lineInset, y, false);
        g.setGradientFill (rightGrad);
        g.drawHorizontalLine ((int) y, creditArea.getCentreX() + textHalfWidth, creditArea.getWidth() - lineInset);
    }
}

void MainComponent::resized()
{
    generateNoiseImage(); // no-op if the size hasn't actually changed

    auto area = getLocalBounds().reduced (18);

    auto header = area.removeFromTop (kHeaderHeight);
    // Same 22px inset the content panel below uses (see "inner" below) --
    // was 20px, a stray 2px mismatch against the panel's left edge that
    // broke the left-margin symmetry the spec calls for.
    header.removeFromLeft (22);
    auto headerRight = header.removeFromRight (34);
    headerRightIconBounds = headerRight.removeFromRight (20).withSizeKeepingCentre (20, 20);

    headerIconBounds = header.removeFromLeft (42).withSizeKeepingCentre (42, 42);
    header.removeFromLeft (14);

    // Title + caption as one 42px-tall block, centred in the full header
    // height so it lines up with the icon regardless of the header's own
    // (slightly generous) height.
    auto textBlock = header.withSizeKeepingCentre (header.getWidth(), 42);
    titleLabel.setBounds (textBlock.removeFromTop (26));
    textBlock.removeFromTop (2);
    subtitleLabel.setBounds (textBlock);

    headerDividerBounds = { 18, 18 + kHeaderHeight, getWidth() - 36, 1 };

    area.removeFromTop (18);
    auto inner = area.reduced (22, 18);

    // The details overlay floats over this whole area (see below) instead
    // of pushing the layout down, so it's captured before anything below
    // consumes "inner" -- otherwise its bounds would shrink to whatever
    // happened to be left over. Trimmed down from the very top so it never
    // covers the "More details" toggle itself (which sits right there,
    // and needs to stay clickable so you can close the overlay again).
    auto overlayBounds = inner.withTrimmedTop (40);

    // --- Two columns side by side: controls on the left, the visual
    // (waveform + A/B) on the right -- a wide, plugin-like layout instead
    // of one long vertical stack. Both columns share one row whose height
    // is whichever column is taller, so nothing gets clipped.
    // The right column's own pixel width (horizontal only, so it's valid
    // before the row's height below is decided) -- the verdict now lives
    // at the bottom of this column, so its height depends on this width.
    const int rightColumnWidth = inner.getWidth() - kLeftColumnWidth - kColumnGap;
    const int resultsPanelH = resultsPanel.getPreferredHeight (rightColumnWidth);
    const int rightColumnTotalHeight = kRightColumnHeight + 18 + resultsPanelH;

    auto columnsArea = inner.removeFromTop (juce::jmax (kLeftColumnHeight + kLeftCardTopPad, rightColumnTotalHeight));
    leftCardBounds = columnsArea.removeFromLeft (kLeftColumnWidth); // the card's own bounds, captured before insetting for content
    columnsArea.removeFromLeft (kColumnGap);
    auto right = columnsArea;

    // Left column -------------------------------------------------------
    // Inset from the card's edges (see leftCardBounds/kLeftCard* above) --
    // only the top is padded; the bottom is left to statusLabel's natural
    // stretch, which now reads as card padding instead of orphaned space.
    auto left = leftCardBounds.reduced (kLeftCardHorizPad, 0).withTrimmedTop (kLeftCardTopPad);

    loadButton.setBounds (left.removeFromTop (40));
    left.removeFromTop (6);
    inputFileLabel.setBounds (left.removeFromTop (18));
    left.removeFromTop (10);

    targetSectionLabel.setBounds (left.removeFromTop (15));
    left.removeFromTop (6);
    targetControl.setBounds (left.removeFromTop (52));
    left.removeFromTop (22);

    qualitySectionLabel.setBounds (left.removeFromTop (15));
    left.removeFromTop (6);
    qualityControl.setBounds (left.removeFromTop (52));
    left.removeFromTop (24);

    auto actionRow = left.removeFromTop (44);
    processButton.setBounds (actionRow.removeFromLeft (juce::roundToInt ((float) actionRow.getWidth() * 0.6f)));
    actionRow.removeFromLeft (10);
    saveAsButton.setBounds (actionRow);
    left.removeFromTop (10);
    // Fills whatever height is actually left in the column (centred text,
    // so it reads fine whether that's 16px or more) instead of a fixed
    // 16px -- when the right column ends up taller (its verdict box is
    // dynamic), this keeps both columns' bottom edges lined up instead of
    // leaving a dead gap under the left column's buttons.
    statusLabel.setBounds (left);

    // Right column --------------------------------------------------------
    auto waveHeader = right.removeFromTop (20);
    // The toggle's font scales with its own bounds height (see
    // PSLookAndFeel::getTextButtonFont), so it's given a taller box than
    // the 20px label row to read clearly -- centred on that row rather
    // than confined to it.
    auto toggleArea = waveHeader.removeFromRight (170).withSizeKeepingCentre (170, 40);
    detailsToggleButton.setBounds (toggleArea);
    waveformSectionLabel.setBounds (waveHeader);
    right.removeFromTop (6);
    waveformDisplay.setBounds (right.removeFromTop (150));

    right.removeFromTop (16);
    abSectionLabel.setBounds (right.removeFromTop (15));
    right.removeFromTop (6);
    audioPlayer.setBounds (right.removeFromTop (64));

    // The verdict now sits directly under the A/B section, in the right
    // column's own (narrower) width, instead of spanning the full width
    // below both columns -- this is what removes the dead space that used
    // to sit below the A/B buttons, and lets the whole window be shorter.
    // It takes ALL of the column's remaining height (not just its own
    // preferred height) -- "right" is already exactly as tall as the
    // shared row (see rightColumnTotalHeight above), so whichever column
    // is naturally shorter has its trailing element (this, or statusLabel
    // on the left) stretch to make up the difference. That's what keeps
    // the two columns' bottom edges lined up no matter which side's
    // content happens to be taller.
    right.removeFromTop (18);
    resultsPanel.setBounds (right);

    // Everything numeric lives behind the "More details" link above,
    // closed by default. When open it floats on top of everything else in
    // this panel (see toggleDetails(), which brings it to front) rather
    // than pushing the window taller -- a card dropped over the content,
    // not a reflow.
    if (detailsExpanded)
        detailsPanel.setBounds (overlayBounds.removeFromTop (
            juce::jmin (DetailsPanel::kPreferredHeight, overlayBounds.getHeight())));

    creditLabel.setBounds (getLocalBounds().removeFromBottom (24));
}

void MainComponent::updateHeight()
{
    // Mirrors the block order in resized(): header, the two-column row
    // (sized to whichever column is taller -- the right column now
    // includes the verdict at its bottom, see resized()), and the credit
    // line. The details panel is a floating overlay (see resized()) and
    // deliberately does NOT affect this -- toggling it never resizes the
    // window.
    const int rightColumnWidth = kNativeWidth - 2 * 18 - 2 * 22 - kLeftColumnWidth - kColumnGap;
    const int resultsPanelH = resultsPanel.getPreferredHeight (rightColumnWidth);
    const int rightColumnTotalHeight = kRightColumnHeight + 18 + resultsPanelH;

    int h = 18                              // top margin
          + kHeaderHeight + 18              // header + gap
          + 18                              // inner reduced top
          + juce::jmax (kLeftColumnHeight + kLeftCardTopPad, rightColumnTotalHeight)
          + 18                              // inner reduced bottom margin
          + 24;                             // credit label

    setSize (kNativeWidth, h);

    if (onNativeSizeChanged)
        onNativeSizeChanged();
}

void MainComponent::chooseInputFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select a finished WAV to simulate...",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
            loadInputFile (file);
    });
}

void MainComponent::loadInputFile (const juce::File& file)
{
    inputFile = file;
    lastOutputFile = juce::File {};
    saveAsButton.setEnabled (false);
    inputFileLabel.setText (file.getFileName(), juce::dontSendNotification);
    resultsPanel.setPlaceholder ("Ready. Press \"Process\" to continue.");
    statusLabel.setText ({}, juce::dontSendNotification); // clear any stale "Done"/"Error" from a previous file
    detailsPanel.clear();
    waveformDisplay.clear();
    audioPlayer.reset();
    updateHeight();
}

void MainComponent::runProcessing()
{
    if (! inputFile.existsAsFile())
    {
        statusLabel.setText ("Choose an input WAV first.", juce::dontSendNotification);
        return;
    }
    if (isProcessing.load())
        return;

    // Re-processing the same input overwrites the same temp output path
    // every time -- release any file handle the player still holds on it
    // from a previous A/B listen first, or the overwrite can fail on
    // Windows (can't write to a file that's still open for playback).
    audioPlayer.stop();

    // Process immediately -- no save dialog up front. The result is
    // written to a temp file (always writable, no permissions surprises);
    // "Save As..." below copies it wherever you actually want it once you
    // like what you hear, instead of forcing you to pick a destination
    // before you even know if the run will be worth keeping.
    auto outFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("PS_" + inputFile.getFileNameWithoutExtension() + "_spotify_sim.wav");

    auto target  = (SpotifyProcessor::Target)  targetControl.getSelectedIndex();
    auto quality = (SpotifyProcessor::Quality) qualityControl.getSelectedIndex();

    isProcessing = true;
    processButton.setEnabled (false);
    saveAsButton.setEnabled (false);
    statusLabel.setText ("Processing...", juce::dontSendNotification);

    auto inFile = inputFile;
    std::thread worker ([this, inFile, outFile, target, quality]
    {
        SpotifyProcessor processor;
        auto report = processor.process (inFile, outFile, target, quality);

        juce::MessageManager::callAsync ([this, report, outFile]
        {
            isProcessing = false;
            processButton.setEnabled (true);
            showReport (report, outFile);
        });
    });
    worker.detach();
}

void MainComponent::chooseSaveLocation()
{
    if (! lastOutputFile.existsAsFile())
        return;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save the Spotify-simulated WAV as...",
        inputFile.getParentDirectory().getChildFile (inputFile.getFileNameWithoutExtension() + "_spotify_sim.wav"),
        "*.wav");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
    {
        auto dest = fc.getResult();
        if (dest == juce::File {})
            return;

        if (lastOutputFile.copyFileTo (dest))
            statusLabel.setText ("Saved to \"" + dest.getFileName() + "\".", juce::dontSendNotification);
        else
            statusLabel.setText ("Could not save to that location.", juce::dontSendNotification);
    });
}

void MainComponent::showReport (const SpotifyProcessor::Report& report, const juce::File& outFile)
{
    resultsPanel.setReport (report);

    if (report.success)
    {
        lastOutputFile = outFile;
        saveAsButton.setEnabled (true);
        statusLabel.setText ("Done - press \"Save As...\" to keep this result.", juce::dontSendNotification);

        // Level-match the original for a fair A/B comparison: play it
        // back with the same gain the processor applied.
        const float levelMatchGain = (float) std::pow (10.0, report.appliedGainDb / 20.0);
        audioPlayer.setSources (inputFile, outFile, levelMatchGain,
                                 report.differenceAvailable ? report.differenceFile : juce::File {},
                                 report.differenceBoostDb);

        detailsPanel.setReport (report);
        waveformDisplay.setData (report.inputWaveformMin, report.inputWaveformMax,
                                   report.outputWaveformMin, report.outputWaveformMax);
    }
    else
    {
        lastOutputFile = juce::File {};
        saveAsButton.setEnabled (false);
        statusLabel.setText ("Error.", juce::dontSendNotification);
        audioPlayer.reset();
        detailsPanel.clear();
        waveformDisplay.clear();
    }

    updateHeight();
}
