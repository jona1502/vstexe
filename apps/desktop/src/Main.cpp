#include "MainComponent.h"

class VocalChainApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "VocalChain"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    void initialise(const juce::String&) override
    {
        window = std::make_unique<MainWindow>(getApplicationName());
    }
    void shutdown() override { window.reset(); }

private:
    class MainWindow final : public juce::DocumentWindow {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colours::black,
                             DocumentWindow::allButtons, true)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, false);
            setResizeLimits(980, 680, 1600, 1050);
            centreWithSize(1120, 760);
            setVisible(true);
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(VocalChainApplication)
