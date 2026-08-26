#include "MainComponent.h"

#if JUCE_WINDOWS
#include <dwmapi.h>
#endif

namespace {
#if JUCE_WINDOWS
/*
 * The installer names this mutex as its AppMutex, which is how a silent update
 * knows InputRack is running and can close it through the restart manager
 * before replacing the executable. The handle stays open for the process
 * lifetime and Windows releases it on exit.
 */
void claimRunningInstanceMutex()
{
    static HANDLE handle = CreateMutexW(nullptr, FALSE, L"InputRackRunningInstance");
    juce::ignoreUnused(handle);
}
#endif

/** Paints the native title bar dark so the frame matches the window content. */
void applyDarkTitleBar([[maybe_unused]] juce::Component& window)
{
#if JUCE_WINDOWS
    if (auto* peer = window.getPeer()) {
        const BOOL dark = TRUE;
        // 20 is DWMWA_USE_IMMERSIVE_DARK_MODE on Windows 10 2004 and newer,
        // 19 on the earlier builds that support it at all. Both are no-ops when
        // unsupported, so trying either way costs nothing.
        auto* handle = static_cast<HWND>(peer->getNativeHandle());
        if (FAILED(DwmSetWindowAttribute(handle, 20, &dark, sizeof(dark))))
            DwmSetWindowAttribute(handle, 19, &dark, sizeof(dark));
    }
#endif
}
}

class InputRackApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "InputRack"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    void initialise(const juce::String&) override
    {
#if JUCE_WINDOWS
        claimRunningInstanceMutex();
#endif
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
            centreWithSize(1180, 780);
            setVisible(true);
            applyDarkTitleBar(*this);
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(InputRackApplication)
