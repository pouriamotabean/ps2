#pragma once
#include <JuceHeader.h>

// A premium pill-style exclusive-choice selector: N segments inside one
// rounded capsule, each with a bold label and an optional smaller
// sub-label underneath (e.g. "Loud" over "-11 LUFS").
class SegmentedControl : public juce::Component
{
public:
    struct Segment { juce::String label, subLabel; };

    explicit SegmentedControl (std::vector<Segment> segmentsIn);

    void setSelectedIndex (int index, juce::NotificationType notify = juce::sendNotification);
    int getSelectedIndex() const noexcept { return selected; }

    std::function<void (int)> onChange;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    std::vector<Segment> segments;
    int selected = 0;
    int hovered = -1;

    juce::Rectangle<float> segmentBounds (int index) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};
