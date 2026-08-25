#pragma once
#include <vocalchain/PluginChainEngine.h>
#include <juce_audio_utils/juce_audio_utils.h>

class MainComponent final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::Button::Listener {
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

    vocalchain::PluginChainEngine engine;
    juce::AudioDeviceSelectorComponent devices;
    juce::ComboBox availablePlugins;
    juce::ListBox chainList{"Vocal Chain", this};
    juce::TextButton scan{"Scan VST3"}, add{"Add"}, remove{"Remove"};
    juce::TextButton up{"Up"}, down{"Down"}, bypass{"Bypass"}, open{"Open editor"};
    juce::TextButton save{"Save preset"}, load{"Load preset"};
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::DocumentWindow> editorWindow;
};
