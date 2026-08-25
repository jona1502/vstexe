#include "MainComponent.h"
#include <cmath>

namespace {
constexpr juce::uint32 background = 0xff090b10;
constexpr juce::uint32 surface = 0xff11151e;
constexpr juce::uint32 surfaceRaised = 0xff181d29;
constexpr juce::uint32 border = 0xff272d3b;
constexpr juce::uint32 text = 0xfff5f7fb;
constexpr juce::uint32 textMuted = 0xff8e98aa;
constexpr juce::uint32 accent = 0xff7c5cff;
constexpr juce::uint32 accentBright = 0xff9b87ff;
constexpr juce::uint32 success = 0xff3ddc97;

juce::File pluginDataDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("VocalChain");
}

juce::File pluginCacheFile()
{
    return pluginDataDirectory().getChildFile("known-plugins.xml");
}

void drawCard(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(0x24000000));
    g.fillRoundedRectangle(bounds.translated(0.0f, 4.0f), 16.0f);
    g.setColour(juce::Colour(surface));
    g.fillRoundedRectangle(bounds, 16.0f);
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(bounds, 16.0f, 1.0f);
}

class PluginEditorWindow final : public juce::DocumentWindow {
public:
    explicit PluginEditorWindow(const juce::String& name)
        : DocumentWindow(name, juce::Colours::black, closeButton, true)
    {
        setResizable(true, false);
    }
    void closeButtonPressed() override { setVisible(false); }
};
}

VocalChainLookAndFeel::VocalChainLookAndFeel()
{
    setColour(juce::Label::textColourId, juce::Colour(text));
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, juce::Colour(text));
    setColour(juce::TextButton::textColourOnId, juce::Colour(text));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(surfaceRaised));
    setColour(juce::ComboBox::textColourId, juce::Colour(text));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(border));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(textMuted));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(surfaceRaised));
    setColour(juce::PopupMenu::textColourId, juce::Colour(text));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(accent));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff424a5c));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(surfaceRaised));
    setColour(juce::TextEditor::textColourId, juce::Colour(text));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(border));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(accent));
}

void VocalChainLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                  const juce::Colour&, bool highlighted, bool down)
{
    const auto isPrimary = button.getComponentID() == "primary";
    const auto isDanger = button.getComponentID() == "danger";
    auto colour = isPrimary ? juce::Colour(accent) : juce::Colour(surfaceRaised);
    if (isDanger && highlighted) colour = juce::Colour(0xffa63e55);
    else if (down) colour = colour.brighter(0.16f);
    else if (highlighted) colour = colour.brighter(0.08f);

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    if (highlighted && isPrimary) {
        g.setColour(juce::Colour(accent).withAlpha(0.22f));
        g.fillRoundedRectangle(bounds.expanded(2.0f), 10.0f);
    }
    g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));
    g.fillRoundedRectangle(bounds, 8.0f);
    if (!isPrimary) {
        g.setColour(juce::Colour(border).brighter(highlighted ? 0.18f : 0.0f));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    }
}

void VocalChainLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    g.setColour(button.findColour(juce::TextButton::textColourOffId)
                       .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(10, 0),
                     juce::Justification::centred, 1);
}

void VocalChainLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                                          int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width - 1),
                                                static_cast<float>(height - 1));
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 9.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId)
                   .brighter(box.isMouseOverOrDragging() ? 0.2f : 0.0f));
    g.drawRoundedRectangle(bounds, 9.0f, 1.0f);

    juce::Path chevron;
    const auto cx = static_cast<float>(width - 18);
    const auto cy = static_cast<float>(height) * 0.5f + (isDown ? 1.0f : 0.0f);
    chevron.startNewSubPath(cx - 4.0f, cy - 2.0f);
    chevron.lineTo(cx, cy + 2.0f);
    chevron.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void VocalChainLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(13, 1, box.getWidth() - 42, box.getHeight() - 2);
    label.setFont(juce::FontOptions(14.0f));
}

MainComponent::MainComponent()
    : juce::Thread("VST3 plug-in scanner"),
      devices(engine.deviceManager(), 1, 1, 2, 2, false, false, true, false)
{
    setLookAndFeel(&lookAndFeel);
    for (auto* component : std::initializer_list<juce::Component*>{
             &devices, &availablePlugins, &chainList, &scan, &add, &remove,
             &up, &down, &bypass, &open, &monitor, &save, &load})
        addAndMakeVisible(component);

    for (auto* button : std::initializer_list<juce::Button*>{
             &scan, &add, &remove, &up, &down, &bypass, &open, &monitor, &save, &load})
        button->addListener(this);

    scan.setComponentID("secondary");
    add.setComponentID("primary");
    remove.setComponentID("danger");
    open.setComponentID("primary");
    monitor.setClickingTogglesState(true);
    monitor.setToggleState(engine.isMonitoringEnabled(), juce::dontSendNotification);
    chainList.setRowHeight(68);
    chainList.setOutlineThickness(0);
    chainList.getViewport()->setScrollBarsShown(true, false);
    availablePlugins.setTextWhenNothingSelected("Choose a VST3 effect");

    status.setText("Select a microphone, scan VST3 plug-ins, then build your Vocal Chain.",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colour(textMuted));
    status.setFont(juce::FontOptions(13.0f));
    loadPluginCache();
    refreshAvailablePlugins();
    if (engine.knownPlugins().getNumTypes() > 0)
        status.setText(juce::String(engine.knownPlugins().getNumTypes())
                           + " saved VST3 plug-ins loaded.",
                       juce::dontSendNotification);
    const auto error = engine.initialiseAudio();
    if (error.isNotEmpty()) status.setText("Audio: " + error, juce::dontSendNotification);
    startTimerHz(60);
    setSize(1120, 760);
}

MainComponent::~MainComponent()
{
    signalThreadShouldExit();
    stopThread(10000);
    stopTimer();
    setLookAndFeel(nullptr);
    editorWindow.reset();
    engine.shutdownAudio();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(background));

    juce::ColourGradient glow(juce::Colour(accent).withAlpha(0.16f),
                               static_cast<float>(getWidth()) * 0.72f, -40.0f,
                               juce::Colours::transparentBlack,
                               static_cast<float>(getWidth()) * 0.45f, 300.0f, true);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());

    auto body = getLocalBounds().reduced(24);
    auto left = body.removeFromLeft(330).toFloat();
    body.removeFromLeft(16);
    drawCard(g, left);
    drawCard(g, body.toFloat());

    g.setColour(juce::Colour(textMuted));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("AUDIO INPUT", static_cast<int>(left.getX() + 18), static_cast<int>(left.getY() + 14),
               180, 22, juce::Justification::centredLeft);
    g.drawText("EFFECT CHAIN", body.getX() + 18, body.getY() + 14, 180, 22,
               juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(24);
    status.setBounds({});

    auto deviceCard = area.removeFromLeft(330);
    area.removeFromLeft(16);
    deviceCard.reduce(18, 14);
    deviceCard.removeFromTop(30);
    devices.setBounds(deviceCard);

    auto chainCard = area.reduced(18, 14);
    chainCard.removeFromTop(30);
    auto presetRow = chainCard.removeFromBottom(44);
    save.setBounds(presetRow.removeFromLeft(128).reduced(0, 3));
    presetRow.removeFromLeft(8);
    load.setBounds(presetRow.removeFromLeft(128).reduced(0, 3));

    chainCard.removeFromBottom(10);
    auto actionRow = chainCard.removeFromBottom(44);
    remove.setBounds(actionRow.removeFromLeft(92).reduced(0, 3));
    actionRow.removeFromLeft(7);
    up.setBounds(actionRow.removeFromLeft(66).reduced(0, 3));
    actionRow.removeFromLeft(7);
    down.setBounds(actionRow.removeFromLeft(72).reduced(0, 3));
    actionRow.removeFromLeft(7);
    bypass.setBounds(actionRow.removeFromLeft(90).reduced(0, 3));
    actionRow.removeFromLeft(7);
    open.setBounds(actionRow.removeFromLeft(126).reduced(0, 3));
    actionRow.removeFromLeft(7);
    monitor.setBounds(actionRow.removeFromLeft(118).reduced(0, 3));

    chainCard.removeFromBottom(10);
    auto pluginRow = chainCard.removeFromTop(44);
    scan.setBounds(pluginRow.removeFromLeft(118).reduced(0, 3));
    pluginRow.removeFromLeft(8);
    add.setBounds(pluginRow.removeFromRight(92).reduced(0, 3));
    pluginRow.removeFromRight(8);
    availablePlugins.setBounds(pluginRow.reduced(0, 3));
    chainCard.removeFromTop(10);
    chainList.setBounds(chainCard);
}

int MainComponent::getNumRows() { return engine.pluginCount(); }

void MainComponent::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto card = juce::Rectangle<float>(7.0f, 5.0f, static_cast<float>(width - 14),
                                       static_cast<float>(height - 10));
    g.setColour(selected ? juce::Colour(accent).withAlpha(0.20f) : juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(card, 11.0f);
    const auto selectedGlow = 0.66f + 0.09f * std::sin(animationPhase * 1.35f);
    g.setColour(selected ? juce::Colour(accentBright).withAlpha(selectedGlow)
                         : juce::Colour(border));
    g.drawRoundedRectangle(card, 11.0f, selected ? 1.4f : 1.0f);
    if (auto* plugin = engine.pluginAt(row)) {
        const auto bypassed = engine.isBypassed(row);
        g.setColour(bypassed ? juce::Colour(textMuted) : juce::Colour(text));
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText(plugin->getName(), 54, 12, width - 150, 23, juce::Justification::centredLeft);
        g.setColour(juce::Colour(textMuted));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("VST3 EFFECT", 54, 34, width - 150, 17, juce::Justification::centredLeft);

        g.setColour(selected ? juce::Colour(accent) : juce::Colour(0xff343b4d));
        g.fillRoundedRectangle(18.0f, 17.0f, 26.0f, 26.0f, 8.0f);
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(juce::String(row + 1), 18, 17, 26, 26, juce::Justification::centred);

        const auto stateColour = bypassed ? juce::Colour(textMuted) : juce::Colour(success);
        g.setColour(stateColour.withAlpha(0.16f));
        g.fillRoundedRectangle(static_cast<float>(width - 91), 20.0f, 66.0f, 22.0f, 11.0f);
        g.setColour(stateColour);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(bypassed ? "BYPASS" : "ACTIVE", width - 91, 20, 66, 22,
                   juce::Justification::centred);
    }
}

void MainComponent::timerCallback()
{
    animationPhase += 0.045f;
    if (animationPhase > juce::MathConstants<float>::twoPi)
        animationPhase -= juce::MathConstants<float>::twoPi;
    if (const auto selectedRow = chainList.getSelectedRow(); selectedRow >= 0)
        chainList.repaintRow(selectedRow);

    if (isThreadRunning()) {
        juce::String current;
        {
            const juce::ScopedLock lock(scanStatusLock);
            current = pluginBeingScanned;
        }
        const auto percent = juce::roundToInt(scanProgress.load() * 100.0f);
        status.setText("Scanning VST3: " + juce::String(percent) + "%  " + current,
                       juce::dontSendNotification);
    }

    if (scanFinished.exchange(false)) {
        refreshAvailablePlugins();
        scan.setEnabled(true);
        add.setEnabled(true);
        status.setText(juce::String(engine.knownPlugins().getNumTypes())
                           + " VST3 plug-ins found and saved.",
                       juce::dontSendNotification);
    }
}

void MainComponent::buttonClicked(juce::Button* button)
{
    const int row = chainList.getSelectedRow();
    if (button == &scan) scanPlugins();
    else if (button == &add) addSelectedPlugin();
    else if (button == &remove && row >= 0) {
        editorWindow.reset();
        editorPlugin = nullptr;
        engine.removePlugin(row);
        refresh();
    }
    else if (button == &up && row > 0) { engine.movePlugin(row, row - 1); refresh(); chainList.selectRow(row - 1); }
    else if (button == &down && row + 1 < engine.pluginCount()) { engine.movePlugin(row, row + 1); refresh(); chainList.selectRow(row + 1); }
    else if (button == &bypass && row >= 0) {
        engine.setBypassed(row, !engine.isBypassed(row));
        chainList.repaintRow(row);
    }
    else if (button == &open) openSelectedPlugin();
    else if (button == &monitor) {
        const auto enabled = monitor.getToggleState();
        engine.setMonitoringEnabled(enabled);
        monitor.setButtonText(enabled ? "Monitor on" : "Monitor off");
        monitor.setComponentID(enabled ? "primary" : "secondary");
        monitor.repaint();
    }
    else if (button == &save) savePreset();
    else if (button == &load) loadPreset();
}

void MainComponent::scanPlugins()
{
    if (isThreadRunning()) return;
    scan.setEnabled(false);
    add.setEnabled(false);
    scanProgress.store(0.0f);
    scanFinished.store(false);
    status.setText("Preparing VST3 scan...", juce::dontSendNotification);
    startThread();
}

void MainComponent::run()
{
    for (int i = 0; i < engine.formatManager().getNumFormats(); ++i) {
        if (threadShouldExit()) return;
        auto* format = engine.formatManager().getFormat(i);
        if (format->getName() != "VST3") continue;
        juce::PluginDirectoryScanner scanner(engine.knownPlugins(), *format,
            format->getDefaultLocationsToSearch(), true,
            pluginDataDirectory()
                .getChildFile("vocalchain-scan-dead-mans-pedal.txt"));
        juce::String current;
        while (!threadShouldExit()) {
            {
                const juce::ScopedLock lock(scanStatusLock);
                pluginBeingScanned = scanner.getNextPluginFileThatWillBeScanned();
            }
            scanProgress.store(scanner.getProgress());
            if (!scanner.scanNextFile(true, current)) break;
        }
    }
    if (threadShouldExit()) return;
    scanProgress.store(1.0f);
    savePluginCache();
    scanFinished.store(true);
}

void MainComponent::refreshAvailablePlugins()
{
    availablePlugins.clear();
    const auto types = engine.knownPlugins().getTypes();
    for (int i = 0; i < types.size(); ++i) availablePlugins.addItem(types.getReference(i).name, i + 1);
    if (!types.isEmpty()) availablePlugins.setSelectedId(1);
}

void MainComponent::loadPluginCache()
{
    if (auto xml = juce::XmlDocument::parse(pluginCacheFile()))
        engine.knownPlugins().recreateFromXml(*xml);
}

void MainComponent::savePluginCache()
{
    pluginDataDirectory().createDirectory();
    if (auto xml = engine.knownPlugins().createXml())
        xml->writeTo(pluginCacheFile());
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

    if (editorWindow != nullptr && editorPlugin == plugin) {
        editorWindow->setVisible(true);
        editorWindow->toFront(true);
        return;
    }

    // Destroy the previous editor before asking a processor for its active editor.
    // Otherwise reopening the same plug-in can return an editor that is deleted when
    // editorWindow is replaced, leaving a dangling pointer below.
    editorWindow.reset();
    editorPlugin = nullptr;

    auto* editor = plugin->createEditorIfNeeded();
    if (!editor) { showError("No editor", "This plug-in does not provide an editor."); return; }

    editorWindow = std::make_unique<PluginEditorWindow>(plugin->getName());
    editorPlugin = plugin;
    editorWindow->setUsingNativeTitleBar(true);
    editorWindow->setContentOwned(editor, true);
    editorWindow->setResizable(editor->isResizable(), false);
    editorWindow->centreWithSize(editorWindow->getWidth(), editorWindow->getHeight());
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
                editorWindow.reset();
                editorPlugin = nullptr;
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
