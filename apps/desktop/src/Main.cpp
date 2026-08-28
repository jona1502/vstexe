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

juce::Image createTrayIcon()
{
    juce::Image icon(juce::Image::ARGB, 64, 64, true);
    juce::Graphics graphics(icon);
    graphics.setColour(juce::Colour(0xff38dfe0));
    graphics.fillEllipse(2.0f, 2.0f, 60.0f, 60.0f);
    graphics.setColour(juce::Colour(0xff08090c));
    graphics.fillRoundedRectangle(26.0f, 15.0f, 12.0f, 26.0f, 6.0f);
    graphics.drawRoundedRectangle(19.0f, 13.0f, 26.0f, 36.0f, 13.0f, 4.0f);
    graphics.fillRect(29.0f, 46.0f, 6.0f, 9.0f);
    graphics.fillRoundedRectangle(21.0f, 53.0f, 22.0f, 5.0f, 2.5f);
    return icon;
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
    class TrayIcon final : public juce::SystemTrayIconComponent {
    public:
        explicit TrayIcon(std::function<void()> showWindowIn,
                          std::function<void()> quitIn)
            : showWindow(std::move(showWindowIn)), quit(std::move(quitIn)) {}

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (!event.mods.isPopupMenu()) {
                showWindow();
                return;
            }
            juce::PopupMenu menu;
            menu.addItem(1, "Open InputRack");
            menu.addSeparator();
            menu.addItem(2, "Quit");
            menu.showMenuAsync({}, [show = showWindow, exit = quit](int result) {
                if (result == 1) show();
                else if (result == 2) exit();
            });
        }

    private:
        std::function<void()> showWindow, quit;
    };

    class MainWindow final : public juce::DocumentWindow {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colours::black,
                             DocumentWindow::allButtons, true),
              tray([this] { showFromTray(); }, [] {
                  juce::JUCEApplication::getInstance()->systemRequestedQuit();
              })
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, false);
            setResizeLimits(980, 640, 1600, 1050);
            centreWithSize(1180, 720);
            setVisible(true);
            applyDarkTitleBar(*this);
            tray.setIconImage(createTrayIcon(), createTrayIcon());
            tray.setIconTooltip("InputRack");
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
        void minimiseButtonPressed() override { setVisible(false); }

    private:
        void showFromTray()
        {
            setVisible(true);
            setMinimised(false);
            toFront(true);
        }

        TrayIcon tray;
    };
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(InputRackApplication)
