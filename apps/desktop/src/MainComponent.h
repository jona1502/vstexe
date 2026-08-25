#pragma once
#include <vocalchain/PluginBrowsing.h>
#include <vocalchain/PluginChainEngine.h>
#include <vocalchain/UpdateChecker.h>
#include <optional>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

class VocalChainLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    VocalChainLookAndFeel();

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
                            private juce::Thread {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void buttonClicked(juce::Button*) override;
    void scanPlugins();
    void addSelectedPlugin();
    void openSelectedPlugin();
    void savePreset();
    void loadPreset();
    void showError(const juce::String& title, const juce::String& message);
    void refresh();
    juce::String routingStatus() const;
    void startUpdateCheck(bool requestedByUser);
    void updateCheckFinished(std::optional<vocalchain::AvailableUpdate>, const juce::String& error);
    void startUpdateDownload();
    void refreshAvailablePlugins();
    void loadPluginCache();
    void savePluginCache();
    juce::KnownPluginList::SortMethod selectedSort() const;
    void loadSettings();
    void saveSettings() const;
    void timerCallback() override;
    void run() override;

    vocalchain::PluginChainEngine engine;
    VocalChainLookAndFeel lookAndFeel;
    juce::AudioDeviceSelectorComponent devices;
    juce::ComboBox availablePlugins, pluginSort;
    juce::TextEditor pluginSearch;
    /** The picker contents in display order; ids index into this, not the scan. */
    juce::Array<juce::PluginDescription> visiblePlugins;
    juce::ListBox chainList{"Vocal Chain", this};
    juce::TextButton scan{"Scan VST3"}, remove{"Remove"};
    juce::TextButton up{"Up"}, down{"Down"}, bypass{"Bypass"}, open{"Open editor"};
    juce::TextButton monitor{"Monitor off"};
    juce::TextButton save{"Save preset"}, load{"Load preset"};
    juce::TextButton checkUpdates{"Check for updates"};
    juce::TextButton installUpdate{"Install update"};
    juce::Label status;

    vocalchain::UpdateChecker updates;
    std::optional<vocalchain::AvailableUpdate> availableUpdate;

    // Layout rectangles shared between resized() and paint().
    juce::Rectangle<int> inputPanel, chainPanel, statusStrip, statusTextArea;
    std::atomic<float> scanProgress{};
    std::atomic<bool> scanFinished{};
    juce::CriticalSection scanStatusLock;
    juce::String pluginBeingScanned;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> editorWindow;
    juce::AudioPluginInstance* editorPlugin{};
};
