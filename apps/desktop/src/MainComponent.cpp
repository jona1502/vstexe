#include "MainComponent.h"

namespace {
// Mirrors the design tokens in apps/web/src/styles/global.css so the app and
// the product site read as one product.
constexpr juce::uint32 background = 0xff08090c;   // ink-950
constexpr juce::uint32 surface = 0xff101319;      // ink-850
constexpr juce::uint32 surfaceRaised = 0xff141821; // ink-800
constexpr juce::uint32 border = 0xff1c212c;       // ink-700
constexpr juce::uint32 text = 0xfff2f5f8;         // mist-100
constexpr juce::uint32 textMuted = 0xff8b96a6;    // mist-500
constexpr juce::uint32 textFaint = 0xff6b7686;    // mist-600
constexpr juce::uint32 accent = 0xff38dfe0;       // signal-400
constexpr juce::uint32 accentBright = 0xff7df2ee; // signal-300
constexpr juce::uint32 accentDeep = 0xff17c3c9;   // signal-500

// Accent surfaces carry dark text, exactly as the buttons do on the site.
constexpr juce::uint32 onAccent = 0xff08090c;

constexpr float panelRadius = 14.0f;
constexpr float controlRadius = 8.0f;

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
    g.setColour(juce::Colour(surface));
    g.fillRoundedRectangle(bounds, panelRadius);
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(bounds.reduced(0.5f), panelRadius, 1.0f);
}

/** Small uppercase caption with the wide tracking used for section labels. */
void drawCaption(juce::Graphics& g, const juce::String& caption, juce::Rectangle<int> area,
                 juce::Colour colour, float size = 10.5f)
{
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(size, juce::Font::bold)).withExtraKerningFactor(0.22f));
    g.drawText(caption, area, juce::Justification::centredLeft);
}

/** Pill used for the ACTIVE and BYPASS row states. */
void drawStatePill(juce::Graphics& g, const juce::String& label, juce::Rectangle<float> bounds,
                   juce::Colour colour, float backgroundAlpha)
{
    g.setColour(colour.withAlpha(backgroundAlpha));
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)).withExtraKerningFactor(0.16f));
    g.drawText(label, bounds.toNearestInt(), juce::Justification::centred);
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
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(onAccent));
    setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff2a313f));
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
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

    auto colour = isPrimary ? juce::Colour(accent) : juce::Colour(surfaceRaised);
    if (isPrimary) {
        if (down) colour = juce::Colour(accentDeep);
        else if (highlighted) colour = juce::Colour(accentBright);
    }
    else if (isDanger && highlighted) colour = juce::Colour(0xff3a2029);
    else if (down) colour = colour.brighter(0.14f);
    else if (highlighted) colour = colour.brighter(0.07f);

    if (isPrimary && highlighted) {
        g.setColour(juce::Colour(accent).withAlpha(0.20f));
        g.fillRoundedRectangle(bounds.expanded(2.5f), controlRadius + 2.0f);
    }

    g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));
    g.fillRoundedRectangle(bounds, controlRadius);

    if (!isPrimary) {
        auto outline = juce::Colour(border);
        if (isDanger && highlighted) outline = juce::Colour(0xff7d3a4a);
        else if (highlighted) outline = outline.brighter(0.22f);
        g.setColour(outline.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
        g.drawRoundedRectangle(bounds, controlRadius, 1.0f);
    }
}

void VocalChainLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    const auto isPrimary = button.getComponentID() == "primary";
    const auto colour = isPrimary ? juce::Colour(onAccent) : juce::Colour(text);
    g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.setFont(juce::FontOptions(13.0f, isPrimary ? juce::Font::bold : juce::Font::plain));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(10, 0),
                     juce::Justification::centred, 1);
}

void VocalChainLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                                          int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width - 1),
                                                static_cast<float>(height - 1));
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, controlRadius);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId)
                   .brighter(box.isMouseOverOrDragging() ? 0.2f : 0.0f));
    g.drawRoundedRectangle(bounds, controlRadius, 1.0f);

    juce::Path chevron;
    const auto cx = static_cast<float>(width - 18);
    const auto cy = static_cast<float>(height) * 0.5f + (isDown ? 1.0f : 0.0f);
    chevron.startNewSubPath(cx - 4.0f, cy - 2.0f);
    chevron.lineTo(cx, cy + 2.0f);
    chevron.lineTo(cx + 4.0f, cy - 2.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void VocalChainLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(12, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont(juce::FontOptions(13.0f));
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
    chainList.setRowHeight(56);
    chainList.setOutlineThickness(0);
    chainList.getViewport()->setScrollBarsShown(true, false);
    availablePlugins.setTextWhenNothingSelected("Choose a VST3 effect");

    status.setText("Select a microphone, scan VST3 plug-ins, then build your Vocal Chain.",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colour(textMuted));
    status.setFont(juce::FontOptions(12.5f));
    addAndMakeVisible(status);
    loadPluginCache();
    refreshAvailablePlugins();
    if (engine.knownPlugins().getNumTypes() > 0)
        status.setText(juce::String(engine.knownPlugins().getNumTypes())
                           + " saved VST3 plug-ins loaded.",
                       juce::dontSendNotification);
    const auto error = engine.initialiseAudio();
    if (error.isNotEmpty()) status.setText("Audio: " + error, juce::dontSendNotification);
    else if (!engine.isVirtualMicrophoneRunning())
        status.setText("Virtual microphone: " + engine.virtualMicrophoneStatus(),
                       juce::dontSendNotification);
    else
        status.setText("VocalChain Virtual Mic is ready for Discord.",
                       juce::dontSendNotification);
    startTimerHz(10);
    setSize(1180, 780);
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

    // Single accent bloom behind the header, matching the site hero.
    juce::ColourGradient glow(juce::Colour(accent).withAlpha(0.10f),
                               static_cast<float>(getWidth()) * 0.5f, -120.0f,
                               juce::Colours::transparentBlack,
                               static_cast<float>(getWidth()) * 0.5f, 380.0f, true);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());

    drawCard(g, inputPanel.toFloat());
    drawCard(g, chainPanel.toFloat());

    drawCaption(g, "AUDIO INPUT", inputPanel.reduced(16, 0).withY(inputPanel.getY() + 15).withHeight(18),
                juce::Colour(textFaint));
    drawCaption(g, "EFFECT CHAIN", chainPanel.reduced(16, 0).withY(chainPanel.getY() + 15).withHeight(18),
                juce::Colour(textFaint));

    // Status strip
    g.setColour(juce::Colour(border));
    g.fillRect(statusStrip.withHeight(1));
    const auto running = engine.isVirtualMicrophoneRunning();
    g.setColour(running ? juce::Colour(accent) : juce::Colour(textFaint));
    g.fillEllipse(static_cast<float>(statusStrip.getX() + 20),
                  static_cast<float>(statusStrip.getCentreY()) - 3.0f, 6.0f, 6.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    statusStrip = area.removeFromBottom(38);
    status.setBounds(statusStrip.withTrimmedLeft(36).withTrimmedRight(20));

    auto body = area.reduced(18);
    inputPanel = body.removeFromLeft(374);
    body.removeFromLeft(16);
    chainPanel = body;

    // Left panel: the device selector keeps its natural height so the controls
    // stay grouped with it instead of drifting to the bottom.
    auto inputInner = inputPanel.reduced(16, 15);
    inputInner.removeFromTop(26);
    devices.setBounds(inputInner.removeFromTop(juce::jmin(272, inputInner.getHeight())));

    inputInner.removeFromTop(4);
    auto controlRow = inputInner.removeFromTop(38);
    scan.setBounds(controlRow.removeFromLeft(112));
    controlRow.removeFromLeft(8);
    monitor.setBounds(controlRow.removeFromLeft(120));

    // Right panel: plug-in picker on the caption row, actions along the bottom.
    auto chainInner = chainPanel.reduced(16, 15);
    auto headerRow = chainInner.removeFromTop(34);
    add.setBounds(headerRow.removeFromRight(66).reduced(0, 2));
    headerRow.removeFromRight(8);
    availablePlugins.setBounds(headerRow.removeFromRight(juce::jmin(240, headerRow.getWidth() - 120))
                                   .reduced(0, 2));
    chainInner.removeFromTop(12);

    auto actionRow = chainInner.removeFromBottom(38);
    load.setBounds(actionRow.removeFromRight(112));
    actionRow.removeFromRight(8);
    save.setBounds(actionRow.removeFromRight(112));
    up.setBounds(actionRow.removeFromLeft(62));
    actionRow.removeFromLeft(8);
    down.setBounds(actionRow.removeFromLeft(70));
    actionRow.removeFromLeft(8);
    bypass.setBounds(actionRow.removeFromLeft(84));
    actionRow.removeFromLeft(8);
    remove.setBounds(actionRow.removeFromLeft(88));
    actionRow.removeFromLeft(8);
    open.setBounds(actionRow.removeFromLeft(116));

    chainInner.removeFromBottom(14);
    chainList.setBounds(chainInner);
}

int MainComponent::getNumRows() { return engine.pluginCount(); }

void MainComponent::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    auto card = juce::Rectangle<float>(5.0f, 3.0f, static_cast<float>(width - 10),
                                       static_cast<float>(height - 6));
    g.setColour(selected ? juce::Colour(accent).withAlpha(0.07f) : juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(card, 10.0f);
    g.setColour(selected ? juce::Colour(accent).withAlpha(0.38f) : juce::Colour(border));
    g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);

    auto* plugin = engine.pluginAt(row);
    if (plugin == nullptr) return;

    const auto bypassed = engine.isBypassed(row);

    g.setColour(juce::Colour(textFaint));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(juce::String(row + 1), 18, 0, 22, height, juce::Justification::centredLeft);

    const auto textLeft = 44;
    const auto textWidth = width - textLeft - 96;
    g.setColour(juce::Colour(bypassed ? textMuted : text));
    g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    g.drawText(plugin->getName(), textLeft, 12, textWidth, 18, juce::Justification::centredLeft);
    drawCaption(g, "VST3 EFFECT", {textLeft, 29, textWidth, 14}, juce::Colour(textFaint), 9.0f);

    const auto pill = juce::Rectangle<float>(static_cast<float>(width - 86),
                                             static_cast<float>(height) * 0.5f - 10.0f, 62.0f, 20.0f);
    if (bypassed) drawStatePill(g, "BYPASS", pill, juce::Colour(textMuted), 0.12f);
    else drawStatePill(g, "ACTIVE", pill, juce::Colour(accent), 0.14f);
}

void MainComponent::timerCallback()
{
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
