#include "MainComponent.h"

namespace {
class PluginEditorWindow final : public juce::DocumentWindow {
public:
    explicit PluginEditorWindow(const juce::String& name)
        : DocumentWindow(name, juce::Colours::black, closeButton, true) {}
    void closeButtonPressed() override { setVisible(false); }
};
}

MainComponent::MainComponent()
    : devices(engine.deviceManager(), 1, 1, 2, 2, false, false, true, false)
{
    for (auto* component : std::initializer_list<juce::Component*>{
             &devices, &availablePlugins, &chainList, &scan, &add, &remove,
             &up, &down, &bypass, &open, &save, &load, &status})
        addAndMakeVisible(component);

    for (auto* button : std::initializer_list<juce::Button*>{
             &scan, &add, &remove, &up, &down, &bypass, &open, &save, &load})
        button->addListener(this);

    status.setText("Select a microphone, scan VST3 plug-ins, then build your Vocal Chain.",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    const auto error = engine.initialiseAudio();
    if (error.isNotEmpty()) status.setText("Audio: " + error, juce::dontSendNotification);
    setSize(980, 680);
}

MainComponent::~MainComponent()
{
    editorWindow.reset();
    engine.shutdownAudio();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15171a));
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawText("VocalChain", 20, 10, getWidth() - 40, 35, juce::Justification::left);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20); area.removeFromTop(45);
    auto deviceArea = area.removeFromLeft(300);
    devices.setBounds(deviceArea);
    area.removeFromLeft(20);
    auto statusArea = area.removeFromBottom(34);
    status.setBounds(statusArea);
    auto presetRow = area.removeFromBottom(38);
    save.setBounds(presetRow.removeFromLeft(120).reduced(3));
    load.setBounds(presetRow.removeFromLeft(120).reduced(3));
    auto actionRow = area.removeFromBottom(38);
    for (auto* b : {&remove, &up, &down, &bypass, &open})
        b->setBounds(actionRow.removeFromLeft(b == &open ? 120 : 82).reduced(3));
    auto pluginRow = area.removeFromTop(38);
    scan.setBounds(pluginRow.removeFromLeft(105).reduced(3));
    add.setBounds(pluginRow.removeFromRight(70).reduced(3));
    availablePlugins.setBounds(pluginRow.reduced(3));
    chainList.setBounds(area.reduced(3));
}

int MainComponent::getNumRows() { return engine.pluginCount(); }

void MainComponent::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (selected) g.fillAll(juce::Colour(0xff315b88));
    if (auto* plugin = engine.pluginAt(row)) {
        g.setColour(juce::Colours::white);
        const juce::String suffix = engine.isBypassed(row) ? "  [BYPASSED]" : "";
        g.drawText(juce::String(row + 1) + ".  " + plugin->getName() + suffix, 10, 0,
                   width - 20, height, juce::Justification::centredLeft);
    }
}

void MainComponent::buttonClicked(juce::Button* button)
{
    const int row = chainList.getSelectedRow();
    if (button == &scan) scanPlugins();
    else if (button == &add) addSelectedPlugin();
    else if (button == &remove && row >= 0) { editorWindow.reset(); engine.removePlugin(row); refresh(); }
    else if (button == &up && row > 0) { engine.movePlugin(row, row - 1); refresh(); chainList.selectRow(row - 1); }
    else if (button == &down && row + 1 < engine.pluginCount()) { engine.movePlugin(row, row + 1); refresh(); chainList.selectRow(row + 1); }
    else if (button == &bypass && row >= 0) {
        engine.setBypassed(row, !engine.isBypassed(row));
        chainList.repaintRow(row);
    }
    else if (button == &open) openSelectedPlugin();
    else if (button == &save) savePreset();
    else if (button == &load) loadPreset();
}

void MainComponent::scanPlugins()
{
    status.setText("Scanning VST3 plug-ins...", juce::dontSendNotification);
    for (int i = 0; i < engine.formatManager().getNumFormats(); ++i) {
        auto* format = engine.formatManager().getFormat(i);
        if (format->getName() != "VST3") continue;
        juce::PluginDirectoryScanner scanner(engine.knownPlugins(), *format,
            format->getDefaultLocationsToSearch(), true,
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("vocalchain-scan-dead-mans-pedal.txt"));
        juce::String current;
        while (scanner.scanNextFile(true, current)) {}
    }
    availablePlugins.clear();
    const auto types = engine.knownPlugins().getTypes();
    for (int i = 0; i < types.size(); ++i) availablePlugins.addItem(types.getReference(i).name, i + 1);
    if (!types.isEmpty()) availablePlugins.setSelectedId(1);
    status.setText(juce::String(types.size()) + " VST3 plug-ins found.", juce::dontSendNotification);
}

void MainComponent::addSelectedPlugin()
{
    const int index = availablePlugins.getSelectedId() - 1;
    const auto types = engine.knownPlugins().getTypes();
    if (!juce::isPositiveAndBelow(index, types.size())) return;
    juce::String error;
    if (!engine.addPlugin(types.getReference(index), error)) showError("Could not load plug-in", error);
    refresh();
}

void MainComponent::openSelectedPlugin()
{
    auto* plugin = engine.pluginAt(chainList.getSelectedRow());
    if (!plugin) return;
    auto* editor = plugin->createEditorIfNeeded();
    if (!editor) { showError("No editor", "This plug-in does not provide an editor."); return; }
    editorWindow = std::make_unique<PluginEditorWindow>(plugin->getName());
    editorWindow->setUsingNativeTitleBar(true);
    editorWindow->setContentOwned(editor, false);
    editorWindow->centreWithSize(editor->getWidth(), editor->getHeight());
    editorWindow->setVisible(true);
}

void MainComponent::savePreset()
{
    chooser = std::make_unique<juce::FileChooser>("Save Vocal Chain", juce::File{}, "*.vocalchain.json");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File{} && !file.replaceWithText(engine.captureState().toJson()))
                showError("Could not save preset", file.getFullPathName());
        });
}

void MainComponent::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser>("Load Vocal Chain", juce::File{}, "*.vocalchain.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            try {
                juce::String error;
                if (!engine.restoreState(vocalchain::ChainState::fromJson(fc.getResult().loadFileAsString()), error))
                    showError("Could not restore preset", error);
                refresh();
            } catch (const std::exception& e) { showError("Invalid preset", e.what()); }
        });
}

void MainComponent::showError(const juce::String& title, const juce::String& message)
{
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
}

void MainComponent::refresh() { chainList.updateContent(); chainList.repaint(); }
