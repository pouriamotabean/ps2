#include <JuceHeader.h>
#include "MainComponent.h"

// Hosts MainComponent at its own fixed "native" size and uniformly scales
// it (via an AffineTransform, never a relayout) to fit whatever size the
// window actually is. This is why resizing the window scales everything
// -- text, buttons, spacing -- together instead of reflowing or clipping
// content at the edges, and the content's own proportions never change.
// If the window's aspect ratio doesn't exactly match the content's, the
// scaled content is centred with a small letterbox margin rather than
// stretched out of shape.
class ScaleHost : public juce::Component
{
public:
    ScaleHost()
    {
        addAndMakeVisible (main);
        main.onNativeSizeChanged = [this] { handleNativeSizeChanged(); };
    }

    void resized() override
    {
        rescale();
    }

    MainComponent main;

private:
    void handleNativeSizeChanged()
    {
        // The content's own ideal size changed (e.g. "More details" was
        // toggled, or the verdict grew another line). Resize the actual
        // window to roughly match at close to 1:1 scale -- rescale()
        // below always computes the exact current ratio regardless, so
        // this doesn't need to be pixel-perfect.
        if (auto* win = dynamic_cast<juce::ResizableWindow*> (getTopLevelComponent()))
            win->setSize (main.getWidth(), main.getHeight() + 40);

        rescale();
    }

    void rescale()
    {
        const auto nativeW = (float) main.getWidth();
        const auto nativeH = (float) main.getHeight();

        if (nativeW <= 0.0f || nativeH <= 0.0f || getWidth() <= 0 || getHeight() <= 0)
            return;

        const auto scale = juce::jmin ((float) getWidth() / nativeW, (float) getHeight() / nativeH);
        const auto scaledW = nativeW * scale;
        const auto scaledH = nativeH * scale;
        const auto offsetX = ((float) getWidth()  - scaledW) * 0.5f;
        const auto offsetY = ((float) getHeight() - scaledH) * 0.5f;

        main.setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScaleHost)
};

class PSApplication : public juce::JUCEApplication
{
public:
    PSApplication() = default;

    const juce::String getApplicationName() override    { return "PS"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override           { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                               juce::Colour (0xff0d1117),
                               DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            auto* host = new ScaleHost();
            const auto nativeW = host->main.getWidth();
            const auto nativeH = host->main.getHeight();
            setContentOwned (host, false);

            setResizable (true, true);
            setResizeLimits (420, 480, 1500, 1800);
            centreWithSize (nativeW, nativeH);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (PSApplication)
