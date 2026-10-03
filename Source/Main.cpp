#include <JuceHeader.h>
#include "MainComponent.h"

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

            // The content is a fairly tall, fixed-size panel (it grew once
            // spectrum/waveform/stereo analysis were added), so it's hosted
            // in a Viewport: the window itself stays a sensible, resizable
            // size that fits any screen, and scrolls to the full content.
            auto* viewport = new juce::Viewport();
            viewport->setViewedComponent (new MainComponent(), true);
            viewport->setScrollBarsShown (true, false);
            setContentOwned (viewport, false);

            setResizable (true, true);
            setResizeLimits (560, 480, 900, 2000);
            centreWithSize (700, 860);
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
