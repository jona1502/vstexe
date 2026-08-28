#pragma once
#include <inputrack/IsolatedPluginScanner.h>
#include <inputrack/PluginBrowsing.h>
#include <inputrack/PluginChainEngine.h>
#include <inputrack/UpdateChecker.h>
#include <optional>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class InputRackLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    InputRackLookAndFeel();

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool isHighlighted, bool isDown) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool isHighlighted, bool isDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
};

class MainComponent final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::Button::Listener,
                            private juce::Timer,
                            private juce::Thread,
                            private juce::ChangeListener {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class PluginRowComponent;
    class PluginBrowserComponent;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    juce::Component* refreshComponentForRow(int row, bool selected,
                                            juce::Component* existing) override;
    void buttonClicked(juce::Button*) override;
    void scanPlugins();
    void rescanBlockedPlugins();
    juce::StringArray blockedModules();
    void addPluginAtVisibleIndex(int index);
    void openSelectedPlugin();
    void savePreset();
    void loadPreset();
    void showError(const juce::String& title, const juce::String& message);
    void refresh();
    juce::String routingStatus() const;
    void startUpdateCheck(bool requestedByUser);
    void updateCheckFinished(std::optional<inputrack::AvailableUpdate>, const juce::String& error);
    void startUpdateDownload();
    void refreshAvailablePlugins();
    void loadPluginCache();
    void savePluginCache();
    juce::KnownPluginList::SortMethod selectedSort() const;
    void loadSettings();
    void saveSettings() const;
    void timerCallback() override;
    void run() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void persistRecoveryState() const;
    void recoverFromUncleanShutdown();
    void refreshDeviceSelectors();
    void selectInputDevice();
    void selectOutputDevice();
    void showPluginBrowser();
    void showPresetMenu();
    void showApplicationMenu();
    void selectAndOpenPlugin(int row);
    void togglePluginBypass(int row);
    void movePluginRow(int row, int destination);
    void removePluginRow(int row);

    inputrack::PluginChainEngine engine;
    InputRackLookAndFeel lookAndFeel;
    juce::ComboBox inputDevice, outputDevice;
    juce::ComboBox pluginSort;
    juce::TextEditor pluginSearch;
    /** The picker contents in display order; ids index into this, not the scan. */
    juce::Array<juce::PluginDescription> visiblePlugins;
    juce::ListBox chainList{"Effect Rack", this};
    juce::TextButton addEffect{"+  Add Effect"};
    juce::TextButton presets{"Presets"};
    juce::TextButton appMenu{"..."};
    juce::TextButton scan{"Scan VST3"};
    juce::TextButton monitor{"Monitor off"};
    juce::TextButton checkUpdates{"Check for updates"};
    juce::TextButton installUpdate{"Install update"};
    juce::Label status;

    inputrack::UpdateChecker updates;
    std::optional<inputrack::AvailableUpdate> availableUpdate;

    // Layout rectangles shared between resized() and paint().
    juce::Rectangle<int> inputPanel, chainPanel, outputPanel, statusStrip, statusTextArea;
    juce::Rectangle<int> inputMeterArea, outputMeterArea;
    float inputMeterDisplay[2]{};
    float outputMeterDisplay[2]{};
    int clipIndicatorTicksRemaining{};
    bool refreshingDeviceSelectors{};
    PluginBrowserComponent* activePluginBrowser{};
    std::atomic<float> scanProgress{};
    std::atomic<bool> scanFinished{};
    juce::CriticalSection scanStatusLock;
    juce::String pluginBeingScanned;
    /** When the module named above went in, so a long one can say so. */
    std::atomic<double> moduleScanStartedAt{};
    std::shared_ptr<inputrack::ScanTimeouts> scanTimeouts{
        std::make_shared<inputrack::ScanTimeouts>()};
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    juce::AudioPluginInstance* editorPlugin{};
};
