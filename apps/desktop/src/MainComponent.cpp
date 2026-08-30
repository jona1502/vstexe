#include "MainComponent.h"
#include <inputrack/FeatureAccess.h>
#include <inputrack/IsolatedPluginScanner.h>

#include <cmath>

#if JUCE_WINDOWS
#include <windows.h>
#endif

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
constexpr juce::uint32 warning = 0xffffb84d;
constexpr juce::uint32 danger = 0xffff5d68;

// Accent surfaces carry dark text, exactly as the buttons do on the site.
constexpr juce::uint32 onAccent = 0xff08090c;

constexpr float panelRadius = 14.0f;
constexpr float controlRadius = 8.0f;

// The status-strip readout is measured as well as drawn, so both use one font.
constexpr float readoutFontHeight = 10.5f;
constexpr int readoutPadding = 8;

juce::File pluginDataDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("InputRack");
}

juce::File settingsFile()
{
    return pluginDataDirectory().getChildFile("settings.json");
}

juce::File pluginCacheFile()
{
    return pluginDataDirectory().getChildFile("known-plugins.xml");
}

juce::File recoveryStateFile()
{
    return pluginDataDirectory().getChildFile("recovery.inputrack.json");
}

juce::File profileLibraryFile()
{
    return pluginDataDirectory().getChildFile("profiles.json");
}

juce::String foregroundExecutable()
{
#if JUCE_WINDOWS
    DWORD processId{};
    GetWindowThreadProcessId(GetForegroundWindow(), &processId);
    if (processId == 0) return {};
    auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) return {};
    wchar_t path[32768]{};
    DWORD length = static_cast<DWORD>(std::size(path));
    const auto succeeded = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
    CloseHandle(process);
    if (!succeeded) return {};
    return juce::File(juce::String(path, static_cast<int>(length))).getFileName();
#else
    return {};
#endif
}

/*
 * Created on every launch and deleted only on a clean shutdown. Finding it
 * already there at startup means the previous process never got that far, so
 * the last state written to recoveryStateFile() is offered back automatically.
 */
juce::File crashMarkerFile()
{
    return pluginDataDirectory().getChildFile("session.lock");
}

/*
 * PluginDirectoryScanner blacklists everything this file still names when it is
 * constructed, on the assumption that a leftover entry marks a module that
 * crashed the host. Clearing the blacklist therefore only sticks if the pedal
 * is cleared with it.
 */
juce::File deadMansPedalFile()
{
    return pluginDataDirectory().getChildFile("inputrack-scan-dead-mans-pedal.txt");
}

/** A module identifier is a full path; the file name is what the user knows. */
juce::String moduleDisplayName(const juce::String& fileOrIdentifier)
{
    const auto name =
        juce::File::createFileWithoutCheckingPath(fileOrIdentifier).getFileName();
    return name.isNotEmpty() ? name : fileOrIdentifier;
}

juce::File pluginScannerExecutable()
{
    auto name = juce::String("InputRackPluginScanner");
#if JUCE_WINDOWS
    name += ".exe";
#endif
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getSiblingFile(name);
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

/** Thin fill bar used by the input/output level meters. */
void drawLevelBar(juce::Graphics& g, juce::Rectangle<float> bounds, float level, bool clipped)
{
    g.setColour(juce::Colour(surfaceRaised));
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
    const auto fraction = juce::jlimit(0.0f, 1.0f, level);
    if (fraction > 0.0f) {
        auto fill = bounds.withWidth(bounds.getWidth() * fraction);
        g.setColour(juce::Colour(clipped ? 0xffe0505a : accent));
        g.fillRoundedRectangle(fill, bounds.getHeight() * 0.5f);
    }
    g.setColour(juce::Colour(border));
    g.drawRoundedRectangle(bounds.reduced(0.5f), bounds.getHeight() * 0.5f, 1.0f);
}

constexpr float meterMinimumDb = -60.0f;
constexpr float meterMaximumDb = 3.0f;

float meterYForDb(juce::Rectangle<float> bounds, float db)
{
    const auto normalised = juce::jmap(juce::jlimit(meterMinimumDb, meterMaximumDb, db),
                                      meterMinimumDb, meterMaximumDb, 0.0f, 1.0f);
    return bounds.getBottom() - normalised * bounds.getHeight();
}

juce::String formatMeterDb(float gain)
{
    const auto db = juce::Decibels::gainToDecibels(gain, meterMinimumDb);
    if (db <= meterMinimumDb + 0.01f) return "-inf";
    return juce::String(db, 1) + " dB";
}

void drawVerticalMeter(juce::Graphics& g, juce::Rectangle<int> area,
                       const float levels[2], bool clipped, bool stereo)
{
    static constexpr float ticks[] = {3.0f, 0.0f, -6.0f, -12.0f, -18.0f,
                                      -24.0f, -30.0f, -42.0f, -54.0f, -60.0f};
    auto content = area.reduced(10, 4);
    auto readout = content.removeFromBottom(48);
    auto labels = content.removeFromLeft(42);
    content.removeFromLeft(8);
    auto meterBounds = content.reduced(16, 0).toFloat();

    g.setFont(juce::FontOptions(10.0f));
    for (const auto tick : ticks) {
        const auto y = meterYForDb(meterBounds, tick);
        g.setColour(juce::Colour(tick == 0.0f ? textMuted : textFaint));
        g.drawText(tick > 0.0f ? "+" + juce::String(juce::roundToInt(tick))
                              : juce::String(juce::roundToInt(tick)),
                   labels.getX(), juce::roundToInt(y) - 7, labels.getWidth() - 7, 14,
                   juce::Justification::centredRight);
        g.setColour(juce::Colour(border).withAlpha(tick == 0.0f ? 0.95f : 0.55f));
        g.drawHorizontalLine(juce::roundToInt(y), meterBounds.getX() - 6.0f,
                             meterBounds.getRight() + 6.0f);
    }

    const auto channelCount = stereo ? 2 : 1;
    const auto gap = 10.0f;
    const auto barWidth = juce::jmin(30.0f,
        (meterBounds.getWidth() - gap * static_cast<float>(channelCount - 1))
            / static_cast<float>(channelCount));
    const auto totalWidth = barWidth * channelCount + gap * (channelCount - 1);
    auto x = meterBounds.getCentreX() - totalWidth * 0.5f;

    for (int channel = 0; channel < channelCount; ++channel) {
        auto bar = juce::Rectangle<float>(x, meterBounds.getY(), barWidth, meterBounds.getHeight());
        g.setColour(juce::Colour(0xff0b0e13));
        g.fillRoundedRectangle(bar, 3.0f);
        g.setColour(juce::Colour(border));
        g.drawRoundedRectangle(bar.reduced(0.5f), 3.0f, 1.0f);

        const auto db = juce::jlimit(meterMinimumDb, meterMaximumDb,
            juce::Decibels::gainToDecibels(levels[channel], meterMinimumDb));
        const auto top = meterYForDb(bar, db);
        auto fill = bar.withTop(top);
        juce::ColourGradient gradient(juce::Colour(clipped ? danger : accent),
                                      fill.getCentreX(), fill.getY(),
                                      juce::Colour(0xff25989a), fill.getCentreX(),
                                      fill.getBottom(), false);
        if (!clipped && db > 0.0f)
            gradient = juce::ColourGradient(juce::Colour(warning), fill.getCentreX(), fill.getY(),
                                            juce::Colour(accent), fill.getCentreX(),
                                            fill.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(fill.reduced(2.0f, 1.5f), 2.0f);

        const auto zeroY = meterYForDb(bar, 0.0f);
        g.setColour(juce::Colour(textMuted));
        g.fillRect(bar.getX(), zeroY - 1.0f, bar.getWidth(), 2.0f);

        g.setColour(juce::Colour(textMuted));
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(stereo ? (channel == 0 ? "L" : "R") : "MONO",
                   juce::roundToInt(x - 4.0f), readout.getY(), juce::roundToInt(barWidth + 8.0f), 16,
                   juce::Justification::centred);
        x += barWidth + gap;
    }

    g.setColour(juce::Colour(text));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const auto peak = stereo ? juce::jmax(levels[0], levels[1]) : levels[0];
    g.drawText(formatMeterDb(peak), readout.removeFromBottom(28), juce::Justification::centred);
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

class MainComponent::GlobalHotkeys final {
public:
    GlobalHotkeys(std::function<void()> bypassCallback,
                  std::function<void(int)> profileCallback)
        : bypass(std::move(bypassCallback)), selectProfile(std::move(profileCallback))
    {
#if JUCE_WINDOWS
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = windowProcedure;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"InputRackGlobalHotkeys";
        if (RegisterClassW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return;
        window = CreateWindowExW(0, windowClass.lpszClassName, L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, windowClass.hInstance, this);
        if (window == nullptr) return;
        registerHotkey(1, 'B', "Ctrl+Alt+B");
        for (int i = 0; i < 9; ++i)
            registerHotkey(100 + i, '1' + i, "Ctrl+Alt+" + juce::String(i + 1));
#endif
    }

    ~GlobalHotkeys()
    {
#if JUCE_WINDOWS
        if (window == nullptr) return;
        for (const auto id : registeredIds) UnregisterHotKey(window, id);
        DestroyWindow(window);
#endif
    }

    juce::String failureMessage() const
    {
#if JUCE_WINDOWS
        if (window == nullptr)
            return "Global hotkeys are unavailable because their Windows message window could not be created.";
        if (!failedCombinations.isEmpty())
            return "These global hotkeys are already in use: "
                + failedCombinations.joinIntoString(", ") + ".";
#endif
        return {};
    }

private:
#if JUCE_WINDOWS
    static LRESULT CALLBACK windowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* self = reinterpret_cast<GlobalHotkeys*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<GlobalHotkeys*>(create->lpCreateParams);
            SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (message == WM_HOTKEY && self != nullptr) {
            if (wParam == 1) self->bypass();
            else if (wParam >= 100 && wParam < 109)
                self->selectProfile(static_cast<int>(wParam - 100));
            return 0;
        }
        return DefWindowProcW(handle, message, wParam, lParam);
    }

    void registerHotkey(int id, unsigned int key, const juce::String& label)
    {
        if (RegisterHotKey(window, id, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, key) != FALSE)
            registeredIds.add(id);
        else
            failedCombinations.add(label);
    }

    HWND window{};
    juce::Array<int> registeredIds;
    juce::StringArray failedCombinations;
#endif
    std::function<void()> bypass;
    std::function<void(int)> selectProfile;
};

InputRackLookAndFeel::InputRackLookAndFeel()
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

void InputRackLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
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

void InputRackLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                            bool, bool)
{
    const auto isPrimary = button.getComponentID() == "primary";
    const auto colour = isPrimary ? juce::Colour(onAccent) : juce::Colour(text);
    g.setColour(colour.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    g.setFont(juce::FontOptions(13.0f, isPrimary ? juce::Font::bold : juce::Font::plain));
    // Ten points of breathing room suits a full-width button and swallows a
    // narrow one whole, so the padding follows the width it is taken out of.
    const auto padding = juce::jmin(10, button.getWidth() / 6);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(padding, 0),
                     juce::Justification::centred, 1);
}

void InputRackLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
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

void InputRackLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(12, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont(juce::FontOptions(13.0f));
}

class MainComponent::PluginBrowserComponent final : public juce::Component,
                                                    private juce::ListBoxModel {
public:
    explicit PluginBrowserComponent(MainComponent& ownerIn)
        : owner(ownerIn), results("Available effects", this), add("Add effect")
    {
        for (auto* component : std::initializer_list<juce::Component*>{
                 &owner.pluginSearch, &owner.pluginSort, &owner.scan, &results, &add})
            addAndMakeVisible(component);
        owner.activePluginBrowser = this;
        results.setRowHeight(30);
        results.setOutlineThickness(1);
        results.setColour(juce::ListBox::outlineColourId, juce::Colour(border));
        results.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0b0e13));
        add.setComponentID("primary");
        add.onClick = [this] { addRow(results.getSelectedRow()); };
        jumpToInitial();
        setSize(560, 430);
    }

    ~PluginBrowserComponent() override
    {
        if (owner.activePluginBrowser == this) owner.activePluginBrowser = nullptr;
    }

    void refreshResults()
    {
        results.updateContent();
        jumpToInitial();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(surface));
        drawCaption(g, "ADD VST3 EFFECT", {18, 12, getWidth() - 36, 18}, juce::Colour(accent));
        drawCaption(g, "SORT BY", {18, 82, 78, 18}, juce::Colour(textFaint), 9.0f);
        g.setColour(juce::Colour(textMuted));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(owner.visiblePlugins.size()) + " effects available - type one initial to jump",
                   18, getHeight() - 29, getWidth() - 36, 18,
                   juce::Justification::centredLeft);
    }

    void resized() override
    {
        owner.pluginSearch.setBounds(18, 38, getWidth() - 36, 36);
        owner.pluginSort.setBounds(96, 80, 174, 34);
        owner.scan.setBounds(getWidth() - 148, 80, 130, 34);
        results.setBounds(18, 126, getWidth() - 36, getHeight() - 174);
        add.setBounds(getWidth() - 130, getHeight() - 39, 112, 32);
    }

private:
    int getNumRows() override { return owner.visiblePlugins.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (!juce::isPositiveAndBelow(row, owner.visiblePlugins.size())) return;
        if (selected) g.fillAll(juce::Colour(accent).withAlpha(0.12f));
        const auto& plugin = owner.visiblePlugins.getReference(row);
        g.setColour(juce::Colour(text));
        g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
        g.drawFittedText(plugin.name, 12, 2, width - 190, height - 4,
                         juce::Justification::centredLeft, 1);
        g.setColour(juce::Colour(textMuted));
        g.setFont(juce::FontOptions(10.5f));
        const auto details = plugin.manufacturerName + "  -  "
            + (plugin.category.isNotEmpty() ? plugin.category : "VST3");
        g.drawFittedText(details, width - 178, 2, 164, height - 4,
                         juce::Justification::centredRight, 1);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { addRow(row); }

    void jumpToInitial()
    {
        const auto query = owner.pluginSearch.getText().trim();
        if (query.length() != 1 || !juce::CharacterFunctions::isLetter(query[0])) return;
        const auto row = inputrack::findPluginByInitial(owner.visiblePlugins, query[0]);
        if (row < 0) return;
        results.selectRow(row);
        results.scrollToEnsureRowIsOnscreen(row);
    }

    void addRow(int row)
    {
        if (!juce::isPositiveAndBelow(row, owner.visiblePlugins.size())) return;
        owner.addPluginAtVisibleIndex(row);
        if (auto* callout = findParentComponentOfClass<juce::CallOutBox>()) callout->dismiss();
    }

    MainComponent& owner;
    juce::ListBox results;
    juce::TextButton add;
};

class MainComponent::PluginRowComponent final : public juce::Component,
                                                 private juce::Timer {
public:
    PluginRowComponent(MainComponent& ownerIn, int rowIn)
        : owner(ownerIn), row(rowIn), power("ON"), menu("...")
    {
        addAndMakeVisible(power);
        addAndMakeVisible(menu);
        power.setComponentID("secondary");
        menu.setComponentID("secondary");
        power.onClick = [this] { owner.togglePluginBypass(row); };
        menu.onClick = [this] { showMenu(); };
        startTimerHz(12);
    }

    /*
     * Repainting unconditionally, even when the index has not moved: the row a
     * recycled component lands on is rarely the plug-in it last drew.
     */
    void setRow(int newRow)
    {
        row = newRow;
        repaint();
    }

    void setSelected(bool shouldBeSelected)
    {
        if (selected == shouldBeSelected) return;
        selected = shouldBeSelected;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto card = getLocalBounds().toFloat().reduced(4.0f, 3.0f);
        g.setColour(selected ? juce::Colour(accent).withAlpha(0.075f)
                             : juce::Colour(surfaceRaised));
        g.fillRoundedRectangle(card, 9.0f);
        g.setColour(selected ? juce::Colour(accent).withAlpha(0.42f) : juce::Colour(border));
        g.drawRoundedRectangle(card.reduced(0.5f), 9.0f, 1.0f);

        auto* plugin = owner.engine.pluginAt(row);
        if (plugin == nullptr) return;
        const auto missing = owner.engine.isPluginMissing(row);
        g.setColour(juce::Colour(textFaint));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(row + 1), 13, 0, 22, getHeight(), juce::Justification::centred);

        const auto nameWidth = juce::jmax(120, getWidth() - 88 - 60);
        g.setColour(juce::Colour(text));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawFittedText(plugin->getName(), 88, 12, nameWidth, 18,
                         juce::Justification::centredLeft, 1);
        const auto details = missing
            ? juce::String("MISSING PLUG-IN - AUDIO PASSES THROUGH")
            : "VST3  " + juce::String(owner.engine.pluginInputChannelCountAt(row))
                + " -> " + juce::String(owner.engine.pluginOutputChannelCountAt(row));
        drawCaption(g, details, {88, 30, nameWidth, 14},
                    missing ? juce::Colour(0xffffa45b) : juce::Colour(textFaint), 8.5f);

        // Drag handle.
        g.setColour(juce::Colour(textFaint));
        for (int y = 18; y <= 36; y += 6)
            g.fillEllipse(42.0f, static_cast<float>(y), 2.0f, 2.0f);
    }

    void resized() override
    {
        const auto buttonHeight = 32;
        const auto y = (getHeight() - buttonHeight) / 2;
        power.setBounds(53, y, 29, buttonHeight);
        menu.setBounds(getWidth() - 42, y, 30, buttonHeight);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        // The reorder from the previous drag has long since been applied.
        reorderPending = false;
        owner.chainList.selectRow(row);
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        owner.selectAndOpenPlugin(row);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (reorderPending || event.getDistanceFromDragStart() < 18) return;
        const auto relative = event.getEventRelativeTo(&owner.chainList);
        const auto target = owner.chainList.getRowContainingPosition(relative.x, relative.y);
        if (target < 0 || target == row) return;
        reorderPending = true;
        const juce::Component::SafePointer<MainComponent> safeOwner(&owner);
        const auto source = row;
        juce::MessageManager::callAsync([safeOwner, source, target] {
            if (safeOwner != nullptr) safeOwner->movePluginRow(source, target);
        });
    }

private:
    void timerCallback() override
    {
        const auto missing = owner.engine.isPluginMissing(row);
        const auto bypassed = owner.engine.isBypassed(row);
        power.setButtonText(missing ? "!" : bypassed ? "OFF" : "ON");
        power.setEnabled(!missing);
        power.setComponentID(missing || bypassed ? "secondary" : "primary");
        power.repaint();
    }

    void showMenu()
    {
        juce::PopupMenu popup;
        const auto missing = owner.engine.isPluginMissing(row);
        popup.addItem(1, "Open editor", !missing);
        popup.addItem(2, owner.engine.isBypassed(row) ? "Enable effect" : "Bypass effect",
                      !missing);
        popup.addSeparator();
        popup.addItem(3, "Move up", row > 0);
        popup.addItem(4, "Move down", row + 1 < owner.engine.pluginCount());
        popup.addSeparator();
        popup.addItem(5, "Remove effect");
        const juce::Component::SafePointer<MainComponent> safeOwner(&owner);
        const auto selectedRow = row;
        popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menu),
            [safeOwner, selectedRow](int result) {
                if (safeOwner == nullptr) return;
                if (result == 1) safeOwner->selectAndOpenPlugin(selectedRow);
                else if (result == 2) safeOwner->togglePluginBypass(selectedRow);
                else if (result == 3) safeOwner->movePluginRow(selectedRow, selectedRow - 1);
                else if (result == 4) safeOwner->movePluginRow(selectedRow, selectedRow + 1);
                else if (result == 5) safeOwner->removePluginRow(selectedRow);
            });
    }

    MainComponent& owner;
    int row{};
    bool selected{};
    bool reorderPending{};
    juce::TextButton power, menu;
};

MainComponent::MainComponent()
    : juce::Thread("VST3 plug-in scanner"),
      profiles(profileLibraryFile()),
      entitlement(inputrack::EntitlementService::create())
#if !INPUTRACK_STORE_BUILD
    , updates(juce::JUCEApplication::getInstance()->getApplicationVersion(), pluginDataDirectory())
#endif
{
    setLookAndFeel(&lookAndFeel);
    for (auto* component : std::initializer_list<juce::Component*>{
             &inputDevice, &outputDevice, &chainList, &addEffect, &presets, &appMenu,
             &proButton, &monitor})
        addAndMakeVisible(component);

    for (auto* button : std::initializer_list<juce::Button*>{
             &scan, &monitor, &addEffect, &presets, &appMenu, &proButton})
        button->addListener(this);

    scan.setComponentID("secondary");
#if !INPUTRACK_STORE_BUILD
    addAndMakeVisible(checkUpdates);
    addAndMakeVisible(installUpdate);
    checkUpdates.addListener(this);
    installUpdate.addListener(this);
    checkUpdates.setComponentID("secondary");
    installUpdate.setComponentID("primary");
    // Only shown once a newer release is actually available.
    installUpdate.setVisible(false);
#endif
    addEffect.setComponentID("secondary");
    presets.setComponentID("secondary");
    presets.setButtonText("Profiles");
    proButton.setComponentID("primary");
    appMenu.setComponentID("secondary");
    monitor.setClickingTogglesState(true);
    monitor.setToggleState(engine.isMonitoringEnabled(), juce::dontSendNotification);
    monitor.setButtonText(engine.isMonitoringEnabled() ? "Monitor on" : "Monitor off");
    monitor.setComponentID(engine.isMonitoringEnabled() ? "primary" : "secondary");
    engine.deviceManager().addChangeListener(this);
    // A row carries its name, its channel count and two buttons; nothing in it
    // needs the height the parameter knobs used to ask for.
    chainList.setRowHeight(56);
    chainList.setOutlineThickness(0);
    chainList.getViewport()->setScrollBarsShown(true, false);
    pluginSort.addItem("Name", 1);
    pluginSort.addItem("Manufacturer", 2);
    pluginSort.addItem("Category", 3);
    pluginSort.onChange = [this] { saveSettings(); refreshAvailablePlugins(); };

    pluginSearch.setTextToShowWhenEmpty("Type one initial to jump, e.g. C", juce::Colour(textFaint));
    pluginSearch.setInputRestrictions(1, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    pluginSearch.setColour(juce::TextEditor::backgroundColourId, juce::Colour(surfaceRaised));
    pluginSearch.setColour(juce::TextEditor::outlineColourId, juce::Colour(border));
    pluginSearch.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(accentDeep));
    pluginSearch.setColour(juce::TextEditor::textColourId, juce::Colour(text));
    pluginSearch.setColour(juce::TextEditor::highlightColourId, juce::Colour(accentDeep));
    pluginSearch.onTextChange = [this] {
        if (activePluginBrowser != nullptr) activePluginBrowser->refreshResults();
    };

    inputDevice.setTextWhenNothingSelected("Select microphone");
    outputDevice.setTextWhenNothingSelected("Select output");
    inputDevice.onChange = [this] { selectInputDevice(); };
    outputDevice.onChange = [this] { selectOutputDevice(); };

    status.setText("Select a microphone, scan VST3 plug-ins, then build your rack.",
                   juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colour(textMuted));
    status.setFont(juce::FontOptions(12.5f));
    addAndMakeVisible(status);
    loadSettings();
    juce::String profileError;
    if (!profiles.load(profileError)) status.setText(profileError, juce::dontSendNotification);
    loadPluginCache();
    refreshAvailablePlugins();
    if (engine.knownPlugins().getNumTypes() > 0)
        status.setText(juce::String(engine.knownPlugins().getNumTypes())
                           + " saved VST3 plug-ins loaded.",
                       juce::dontSendNotification);
    const auto error = engine.initialiseAudio({}, storedSampleRate);
    if (error.isNotEmpty()) status.setText("Audio: " + error, juce::dontSendNotification);
    else status.setText(routingStatus(), juce::dontSendNotification);
    storedSampleRate = engine.sampleRate();
    refreshDeviceSelectors();
    const auto recovering = crashMarkerFile().existsAsFile();
    restoreActiveProfile = !recovering;
    recoverFromUncleanShutdown();
    pluginDataDirectory().createDirectory();
    crashMarkerFile().create();
    updateEntitlementUi(entitlement->state());
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
        safeThis->refreshEntitlement(false);
        if (!safeThis->setupAssistantSeen) safeThis->showSetupAssistant();
    });
    startTimerHz(20);
#if !INPUTRACK_STORE_BUILD
    if (updates.isDueForAutomaticCheck()) startUpdateCheck(false);
#endif
    setSize(1180, 720);
}

MainComponent::~MainComponent()
{
    globalHotkeys.reset();
    engine.deviceManager().removeChangeListener(this);
    signalThreadShouldExit();
    stopThread(10000);
    stopTimer();
    setLookAndFeel(nullptr);
    editorWindow.reset();
    engine.shutdownAudio();
    // A clean shutdown means nothing needs recovering on the next launch.
    crashMarkerFile().deleteFile();
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
    drawCard(g, outputPanel.toFloat());

    drawCaption(g, "INPUT", inputPanel.reduced(14, 0).withY(inputPanel.getY() + 15).withHeight(18),
                juce::Colour(accent));
    drawCaption(g, "EFFECT CHAIN", chainPanel.reduced(16, 0).withY(chainPanel.getY() + 15).withHeight(18),
                juce::Colour(accent));
    drawCaption(g, "OUTPUT", outputPanel.reduced(14, 0).withY(outputPanel.getY() + 15).withHeight(18),
                juce::Colour(accent));

    drawVerticalMeter(g, inputMeterArea, inputMeterDisplay, false, false);
    const auto clipping = clipIndicatorTicksRemaining > 0;
    drawVerticalMeter(g, outputMeterArea, outputMeterDisplay, clipping, true);
    if (clipping)
        drawStatePill(g, "CLIP", outputMeterArea.withTrimmedLeft(outputMeterArea.getWidth() - 54)
                                      .withHeight(20).toFloat(),
                      juce::Colour(danger), 0.22f);

    if (engine.pluginCount() == 0) {
        g.setColour(juce::Colour(border));
        g.drawRoundedRectangle(chainList.getBounds().toFloat().reduced(5.0f), 9.0f, 1.0f);
        g.setColour(juce::Colour(textMuted));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Drop an effect here or choose Add Effect", chainList.getBounds(),
                   juce::Justification::centred);
    }

    // Status strip
    g.setColour(juce::Colour(border));
    g.fillRect(statusStrip.withHeight(1));
    const auto running = engine.isVirtualMicrophoneRunning();
    g.setColour(running ? juce::Colour(accent) : juce::Colour(textFaint));
    g.fillEllipse(static_cast<float>(statusTextArea.getX() - 16),
                  static_cast<float>(statusStrip.getCentreY()) - 3.0f, 6.0f, 6.0f);

    g.setColour(juce::Colour(running ? accent : textFaint));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText(running ? "RUNNING" : "STOPPED", statusStrip.getX() + 24,
               statusStrip.getY(), 64, statusStrip.getHeight(), juce::Justification::centredLeft);

    // The readout opens the sample-rate menu, so it brightens and underlines
    // under the pointer instead of looking like dead text.
    const auto readoutArea = transportReadoutBounds();
    g.setFont(juce::FontOptions(readoutFontHeight));
    g.setColour(juce::Colour(transportReadoutHot ? accentBright : textMuted));
    g.drawText(transportReadout(), readoutArea, juce::Justification::centredLeft);
    if (transportReadoutHot)
        g.fillRect(readoutArea.getX(), statusStrip.getCentreY() + 9,
                   readoutArea.getWidth() - readoutPadding, 1);
    g.setColour(juce::Colour(textMuted));
    g.drawText("VST3", readoutArea.getRight() + 8, statusStrip.getY(), 60,
               statusStrip.getHeight(), juce::Justification::centredLeft);
}

juce::String MainComponent::transportReadout()
{
    auto* device = engine.deviceManager().getCurrentAudioDevice();
    const auto rate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    const auto bufferSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;
    return juce::String(rate / 1000.0, 1) + " kHz   " + juce::String(bufferSize) + " samples";
}

/*
 * The readout is painted text rather than a control, so its bounds are measured
 * from the string paint() draws. That keeps the click target on the digits the
 * user is actually pointing at, whatever the current rate and buffer size make
 * the text as wide as.
 */
juce::Rectangle<int> MainComponent::transportReadoutBounds()
{
    const juce::Font font{juce::FontOptions(readoutFontHeight)};
    const auto width = juce::GlyphArrangement::getStringWidthInt(font, transportReadout());
    return {statusStrip.getX() + 96, statusStrip.getY(), width + readoutPadding,
            statusStrip.getHeight()};
}

void MainComponent::mouseDown(const juce::MouseEvent& event)
{
    if (transportReadoutBounds().contains(event.getPosition())) showSampleRateMenu();
}

void MainComponent::mouseMove(const juce::MouseEvent& event)
{
    const auto hot = transportReadoutBounds().contains(event.getPosition());
    if (hot == transportReadoutHot) return;
    transportReadoutHot = hot;
    setMouseCursor(hot ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint(statusStrip);
}

void MainComponent::mouseExit(const juce::MouseEvent&)
{
    if (!transportReadoutHot) return;
    transportReadoutHot = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint(statusStrip);
}

void MainComponent::showSampleRateMenu()
{
    const auto rates = engine.availableSampleRates();
    const auto current = engine.sampleRate();
    juce::PopupMenu popup;
    popup.addSectionHeader("Sample rate");
    for (int i = 0; i < rates.size(); ++i)
        popup.addItem(i + 1, juce::String(rates[i] / 1000.0, 1) + " kHz", true,
                      std::abs(rates[i] - current) < 0.5);
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    popup.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(
            localAreaToGlobal(transportReadoutBounds())),
        [safeThis, rates](int result) {
            if (safeThis == nullptr || result < 1 || result > rates.size()) return;
            safeThis->selectSampleRate(rates[result - 1]);
        });
}

/*
 * Reopening the device also restarts the driver feed, so both can fail
 * independently: the device may refuse the rate outright, or accept it and
 * leave the virtual microphone unable to come back. Neither may pass silently.
 */
void MainComponent::selectSampleRate(double rate)
{
    const auto error = engine.setSampleRate(rate);
    if (error.isNotEmpty()) showError("Sample rate unavailable", error);
    storedSampleRate = engine.sampleRate();
    saveSettings();
    refreshDeviceSelectors();
    const auto driver = engine.virtualMicrophoneStatus();
    status.setText(driver.isNotEmpty() ? driver : routingStatus(), juce::dontSendNotification);
    repaint(statusStrip);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();

    statusStrip = area.removeFromBottom(42);
    auto strip = statusStrip.reduced(12, 5);
    strip.removeFromLeft(284);
    monitor.setBounds(strip.removeFromRight(112));
#if !INPUTRACK_STORE_BUILD
    strip.removeFromRight(8);
    checkUpdates.setBounds(strip.removeFromRight(142));
    if (installUpdate.isVisible()) {
        strip.removeFromRight(8);
        installUpdate.setBounds(strip.removeFromRight(158));
    }
#endif
    statusTextArea = strip.withTrimmedLeft(8).withTrimmedRight(8);
    status.setBounds(statusTextArea);

    auto body = area.reduced(14, 12);
    const auto sideWidth = juce::jlimit(208, 248, body.getWidth() / 5);
    inputPanel = body.removeFromLeft(sideWidth);
    body.removeFromLeft(12);
    outputPanel = body.removeFromRight(sideWidth);
    body.removeFromRight(12);
    chainPanel = body;

    auto inputInner = inputPanel.reduced(12, 14);
    inputInner.removeFromTop(28);
    inputDevice.setBounds(inputInner.removeFromTop(38));
    inputInner.removeFromTop(10);
    inputMeterArea = inputInner;

    auto outputInner = outputPanel.reduced(12, 14);
    outputInner.removeFromTop(28);
    outputDevice.setBounds(outputInner.removeFromTop(38));
    outputInner.removeFromTop(10);
    outputMeterArea = outputInner;

    auto chainInner = chainPanel.reduced(10, 14);
    auto headerRow = chainInner.removeFromTop(40);
    appMenu.setBounds(headerRow.removeFromRight(36).reduced(0, 2));
    headerRow.removeFromRight(6);
    proButton.setBounds(headerRow.removeFromRight(74).reduced(0, 2));
    headerRow.removeFromRight(6);
    presets.setBounds(headerRow.removeFromRight(82).reduced(0, 2));
    headerRow.removeFromRight(6);
    addEffect.setBounds(headerRow.removeFromRight(112).reduced(0, 2));
    chainInner.removeFromTop(6);
    chainList.setBounds(chainInner);
}

int MainComponent::getNumRows() { return engine.pluginCount(); }

/*
 * ListBox keeps a few more row components than the rack has rows and offers
 * them here for every one of those positions, so the rows past the end have
 * to be turned down rather than filled with a component that has no plug-in
 * behind it.
 */
juce::Component* MainComponent::refreshComponentForRow(int row, bool selected,
                                                        juce::Component* existing)
{
    std::unique_ptr<juce::Component> recycled(existing);
    if (!juce::isPositiveAndBelow(row, engine.pluginCount())) return nullptr;

    auto* component = dynamic_cast<PluginRowComponent*>(recycled.get());
    if (component == nullptr) component = new PluginRowComponent(*this, row);
    else recycled.release();
    component->setRow(row);
    component->setSelected(selected);
    return component;
}

/*
 * Every row is drawn by the PluginRowComponent that sits on top of this one,
 * and that component's card is translucent while the row is selected, so
 * anything painted here shows through it as a second, offset copy of the row.
 */
void MainComponent::paintListBoxItem(int, juce::Graphics&, int, int, bool) {}

void MainComponent::timerCallback()
{
    if (++automaticProfilePollTicks >= 20) {
        automaticProfilePollTicks = 0;
        pollAutomaticProfile();
    }
    if (entitlement->state().trial) {
        if (++trialRefreshTicks >= 20 * 60 * 60) {
            trialRefreshTicks = 0;
            refreshEntitlement(false);
        }
    } else {
        trialRefreshTicks = 0;
    }
    // Peak-hold with decay: a block's peak snaps the bar up instantly and it
    // falls back gradually, which reads far more usefully at 10 Hz than a
    // meter that jumps straight back to zero between polls.
    for (int channel = 0; channel < 2; ++channel) {
        const auto outputPeak = engine.consumeOutputPeak(channel);
        inputMeterDisplay[channel] = juce::jmax(engine.consumeInputPeak(channel),
                                                inputMeterDisplay[channel] * 0.84f);
        outputMeterDisplay[channel] = juce::jmax(outputPeak,
                                                 outputMeterDisplay[channel] * 0.84f);
        if (setupDialog != nullptr && outputPeak > 0.001f) routingSignalSeen = true;
    }
    if (engine.consumeOutputClipped()) clipIndicatorTicksRemaining = 10;
    else if (clipIndicatorTicksRemaining > 0) --clipIndicatorTicksRemaining;
    repaint(inputMeterArea);
    repaint(outputMeterArea);
    repaint(statusStrip);

    if (isThreadRunning()) {
        juce::String current;
        {
            const juce::ScopedLock lock(scanStatusLock);
            current = pluginBeingScanned;
        }
        const auto percent = juce::roundToInt(scanProgress.load() * 100.0f);
        auto message = "Scanning VST3: " + juce::String(percent) + "%  " + current;
        /*
         * A shell module houses a whole plug-in pack and has to be described
         * one housed effect at a time, so the strip can sit on one name for
         * minutes. Nothing distinguishes a shell up front; dwelling on it is
         * the only signal, and saying so beats looking frozen.
         */
        const auto dwellMs = juce::Time::getMillisecondCounterHiRes()
            - moduleScanStartedAt.load();
        if (dwellMs > 15000.0)
            message += "   (plug-in packs take several minutes)";
        status.setText(message, juce::dontSendNotification);
    }

    if (scanFinished.exchange(false)) {
        refreshAvailablePlugins();
        scan.setEnabled(true);
        auto message = juce::String(engine.knownPlugins().getNumTypes())
            + " VST3 plug-ins found and saved.";
        const auto blocked = blockedModules();
        if (!blocked.isEmpty()) {
            juce::StringArray names;
            for (const auto& identifier : blocked)
                names.add(moduleDisplayName(identifier));
            message += " Skipped: " + names.joinIntoString(", ")
                + ". Retry them from the ... menu.";
        }
        status.setText(message, juce::dontSendNotification);
    }
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &addEffect) showPluginBrowser();
    else if (button == &presets) showPresetMenu();
    else if (button == &proButton) showProMenu();
    else if (button == &appMenu) showApplicationMenu();
    else if (button == &scan) scanPlugins();
    else if (button == &monitor) {
        const auto enabled = monitor.getToggleState();
        engine.setMonitoringEnabled(enabled);
        monitor.setButtonText(enabled ? "Monitor on" : "Monitor off");
        monitor.setComponentID(enabled ? "primary" : "secondary");
        monitor.repaint();
        status.setText(routingStatus(), juce::dontSendNotification);
    }
#if !INPUTRACK_STORE_BUILD
    else if (button == &checkUpdates) startUpdateCheck(true);
    else if (button == &installUpdate) startUpdateDownload();
#endif
}

void MainComponent::refreshDeviceSelectors()
{
    refreshingDeviceSelectors = true;
    inputDevice.clear(juce::dontSendNotification);
    outputDevice.clear(juce::dontSendNotification);

    auto& manager = engine.deviceManager();
    if (auto* type = manager.getCurrentDeviceTypeObject()) {
        type->scanForDevices();
        const auto inputs = type->getDeviceNames(true);
        const auto outputs = type->getDeviceNames(false);
        for (int i = 0; i < inputs.size(); ++i) inputDevice.addItem(inputs[i], i + 1);
        for (int i = 0; i < outputs.size(); ++i) outputDevice.addItem(outputs[i], i + 1);

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        manager.getAudioDeviceSetup(setup);
        inputDevice.setText(setup.inputDeviceName, juce::dontSendNotification);
        outputDevice.setText(setup.outputDeviceName, juce::dontSendNotification);
    }
    refreshingDeviceSelectors = false;
}

void MainComponent::selectInputDevice()
{
    if (refreshingDeviceSelectors || inputDevice.getText().isEmpty()) return;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    engine.deviceManager().getAudioDeviceSetup(setup);
    if (setup.inputDeviceName == inputDevice.getText()) return;
    setup.inputDeviceName = inputDevice.getText();
    setup.inputChannels.clear();
    setup.inputChannels.setBit(0);
    const auto error = engine.deviceManager().setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty()) showError("Input device unavailable", error);
    refreshDeviceSelectors();
    status.setText(routingStatus(), juce::dontSendNotification);
}

void MainComponent::selectOutputDevice()
{
    if (refreshingDeviceSelectors || outputDevice.getText().isEmpty()) return;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    engine.deviceManager().getAudioDeviceSetup(setup);
    if (setup.outputDeviceName == outputDevice.getText()) return;
    setup.outputDeviceName = outputDevice.getText();
    setup.outputChannels.clear();
    setup.outputChannels.setRange(0, inputrack::PluginChainEngine::outputChannelCount, true);
    const auto error = engine.deviceManager().setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty()) showError("Output device unavailable", error);
    refreshDeviceSelectors();
    status.setText(routingStatus(), juce::dontSendNotification);
}

void MainComponent::showPluginBrowser()
{
    refreshAvailablePlugins();
    auto content = std::make_unique<PluginBrowserComponent>(*this);
    auto& callout = juce::CallOutBox::launchAsynchronously(
        std::move(content), addEffect.getBounds(), this);
    callout.setDismissalMouseClicksAreAlwaysConsumed(true);
    pluginSearch.grabKeyboardFocus();
}

void MainComponent::showPresetMenu()
{
    juce::PopupMenu popup;
    const auto hasProAccess = inputrack::hasFeatureAccess(
        inputrack::ProductFeature::workflowProfiles, entitlement->state());
    popup.addItem(10, hasProAccess ? "Save current rack as profile..."
                                  : "Save current rack as profile... (Pro)",
                  hasProAccess);
    const auto& stored = profiles.all();
    if (hasProAccess && !stored.isEmpty()) {
        popup.addSeparator();
        for (int i = 0; i < stored.size(); ++i) {
            const auto suffix = stored.getReference(i).name == activeProfileName ? "  (active)" : "";
            juce::PopupMenu profileMenu;
            profileMenu.addItem(100 + i, "Activate");
            profileMenu.addItem(200 + i, "Edit name and app bindings...");
            profileMenu.addSeparator();
            profileMenu.addItem(300 + i, "Delete profile...");
            popup.addSubMenu(stored.getReference(i).name + suffix, profileMenu);
        }
    }
    popup.addSeparator();
    popup.addItem(1, "Export rack preset...");
    popup.addItem(2, "Import rack preset...");
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presets),
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result == 1) safeThis->savePreset();
            else if (result == 2) safeThis->loadPreset();
            else if (result == 10) safeThis->showSaveProfileDialog();
            else if (result >= 300) safeThis->confirmDeleteProfile(result - 300);
            else if (result >= 200) safeThis->showSaveProfileDialog(result - 200);
            else if (result >= 100) safeThis->activateProfileAtIndex(result - 100);
        });
}

void MainComponent::showSaveProfileDialog(int profileIndex)
{
    profileDialogEditIndex = juce::isPositiveAndBelow(profileIndex, profiles.all().size())
        ? profileIndex : -1;
    const auto editing = profileDialogEditIndex >= 0;
    const auto existing = editing
        ? std::optional<inputrack::WorkflowProfile>{profiles.all().getReference(profileDialogEditIndex)}
        : profiles.find(activeProfileName);
    profileDialog = std::make_unique<juce::AlertWindow>(
        editing ? "Edit profile" : "Save profile",
        editing ? "Update the profile name and automatic app bindings."
                : "Store this rack, its devices and optional app bindings.",
        juce::MessageBoxIconType::NoIcon);
    const auto suggested = existing.has_value()
        ? existing->name : "Profile " + juce::String(profiles.all().size() + 1);
    const auto applicationText = existing.has_value()
        ? existing->applications.joinIntoString(", ") : lastExternalApplication;
    profileDialog->addTextEditor("name", suggested, "Profile name");
    profileDialog->addTextEditor("applications", applicationText,
                                 "Applications (comma-separated .exe names)");
    profileDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    profileDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    profileDialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result == 1) safeThis->saveProfileFromDialog();
            safeThis->profileDialog.reset();
            safeThis->profileDialogEditIndex = -1;
        }), false);
}

void MainComponent::saveProfileFromDialog()
{
    if (profileDialog == nullptr) return;
    const auto name = profileDialog->getTextEditorContents("name").trim();
    if (name.isEmpty()) {
        showError("Profile name required", "Enter a name before saving the profile.");
        return;
    }
    auto applications = juce::StringArray::fromTokens(
        profileDialog->getTextEditorContents("applications"), ",;", "");
    applications.trim();
    applications.removeEmptyStrings();
    auto originalName = juce::String{};
    if (juce::isPositiveAndBelow(profileDialogEditIndex, profiles.all().size()))
        originalName = profiles.all().getReference(profileDialogEditIndex).name;
    const auto conflicts = profiles.applicationConflicts(
        applications, originalName.isNotEmpty() ? originalName : name);
    if (!conflicts.isEmpty()) {
        showError("Application already assigned",
                  "Each application can activate only one profile. Remove "
                      + conflicts.joinIntoString(", ")
                      + " from its existing profile before assigning it here.");
        return;
    }
    if (juce::isPositiveAndBelow(profileDialogEditIndex, profiles.all().size())) {
        auto edited = profiles.all().getReference(profileDialogEditIndex);
        edited.name = name;
        edited.applications = applications;
        if (!originalName.equalsIgnoreCase(name)) profiles.remove(originalName);
        profiles.upsert(std::move(edited));
    } else {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        engine.deviceManager().getAudioDeviceSetup(setup);
        profiles.upsert({name, setup.inputDeviceName, setup.outputDeviceName,
                         applications, engine.captureState().toJson()});
    }
    juce::String error;
    if (!profiles.save(error)) {
        showError("Could not save profile", error);
        return;
    }
    if (profileDialogEditIndex < 0 || activeProfileName.equalsIgnoreCase(originalName))
        activeProfileName = name;
    saveSettings();
    status.setText("Profile \"" + name + "\" saved. Ctrl+Alt+1..9 switches profiles.",
                   juce::dontSendNotification);
}

void MainComponent::confirmDeleteProfile(int index)
{
    if (!juce::isPositiveAndBelow(index, profiles.all().size())) return;
    const auto profile = profiles.all().getReference(index);
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Delete profile?")
            .withMessage("Delete \"" + profile.name + "\"? Rack presets are not affected.")
            .withButton("Delete")
            .withButton("Cancel")
            .withAssociatedComponent(this),
        [safeThis, profile](int result) {
            if (safeThis == nullptr || result != 1) return;
            if (!safeThis->profiles.remove(profile.name)) return;
            juce::String error;
            if (!safeThis->profiles.save(error)) {
                safeThis->profiles.upsert(profile);
                safeThis->showError("Could not delete profile", error);
                return;
            }
            if (safeThis->activeProfileName.equalsIgnoreCase(profile.name)) {
                safeThis->activeProfileName.clear();
                safeThis->saveSettings();
            }
            safeThis->status.setText("Profile \"" + profile.name + "\" deleted.",
                                     juce::dontSendNotification);
        });
}

void MainComponent::activateProfile(const inputrack::WorkflowProfile& profile, bool automatic)
{
    try {
        editorWindow.reset();
        editorPlugin = nullptr;
        juce::String error;
        if (!engine.restoreState(inputrack::ChainState::fromJson(profile.chainJson), error)) {
            showError("Could not load profile", error);
            return;
        }
        const auto restoreWarning = error;
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        engine.deviceManager().getAudioDeviceSetup(setup);
        if (profile.inputDevice.isNotEmpty()) setup.inputDeviceName = profile.inputDevice;
        if (profile.outputDevice.isNotEmpty()) setup.outputDeviceName = profile.outputDevice;
        const auto deviceError = engine.deviceManager().setAudioDeviceSetup(setup, true);
        if (deviceError.isNotEmpty())
            status.setText("Profile loaded; device unavailable: " + deviceError,
                           juce::dontSendNotification);
        activeProfileName = profile.name;
        saveSettings();
        refreshDeviceSelectors();
        refresh();
        status.setText(restoreWarning.isNotEmpty()
                           ? "Profile loaded with unavailable plug-ins; audio passes through them."
                           : (automatic ? "Automatically selected \"" : "Selected \"")
                               + profile.name + "\".",
                       juce::dontSendNotification);
    } catch (const std::exception& exception) {
        showError("Invalid profile", exception.what());
    }
}

void MainComponent::activateProfileAtIndex(int index)
{
    if (juce::isPositiveAndBelow(index, profiles.all().size()))
        activateProfile(profiles.all().getReference(index));
}

void MainComponent::pollAutomaticProfile()
{
    if (!inputrack::hasFeatureAccess(inputrack::ProductFeature::automaticProfiles,
                                     entitlement->state())) return;
    const auto executable = foregroundExecutable();
    if (executable.isEmpty() || executable.equalsIgnoreCase("InputRack.exe")) return;
    lastExternalApplication = executable;
    const auto match = profiles.matchApplication(executable);
    if (match.has_value() && match->name != activeProfileName)
        activateProfile(*match, true);
}

void MainComponent::toggleGlobalBypass()
{
    engine.setGloballyBypassed(!engine.isGloballyBypassed());
    chainList.repaint();
    status.setText(engine.isGloballyBypassed() ? "All effects bypassed (Ctrl+Alt+B)."
                                               : "All effects enabled (Ctrl+Alt+B).",
                   juce::dontSendNotification);
}

void MainComponent::showProMenu()
{
    juce::PopupMenu popup;
    const auto current = entitlement->state();
    if (current.permanent) popup.addItem(1, "InputRack Pro is active", false, true);
    else {
        if (current.trial)
            popup.addItem(4, "Pro trial: " + juce::String(current.trialDaysRemaining)
                                 + " day(s) remaining", false, true);
        else if (current.trialAvailable)
            popup.addItem(3, "Start 14-day free trial", !entitlement->isBusy());
        popup.addItem(1, "Buy InputRack Pro", !entitlement->isBusy());
    }
    popup.addItem(2, "Restore purchases", !entitlement->isBusy());
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&proButton),
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result == 1 && !safeThis->entitlement->state().permanent) safeThis->purchasePro();
            else if (result == 2) safeThis->refreshEntitlement(true);
            else if (result == 3) safeThis->startProTrial();
        });
}

void MainComponent::refreshEntitlement(bool requestedByUser)
{
    if (entitlement->isBusy()) return;
    proButton.setEnabled(false);
    if (requestedByUser) status.setText("Checking Microsoft Store purchases...",
                                        juce::dontSendNotification);
    void* window = getPeer() != nullptr ? getPeer()->getNativeHandle() : nullptr;
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    entitlement->refresh(window, [safeThis, requestedByUser](inputrack::EntitlementResult result) {
        if (safeThis == nullptr) return;
        safeThis->updateEntitlementUi(result);
        if (requestedByUser && result.message.isEmpty())
            safeThis->status.setText(result.permanent ? "InputRack Pro purchase restored."
                : result.trial ? "InputRack Pro trial restored."
                               : "No InputRack Pro purchase or trial was found.",
                                     juce::dontSendNotification);
    });
}

void MainComponent::purchasePro()
{
    if (entitlement->isBusy()) return;
    proButton.setEnabled(false);
    status.setText("Opening Microsoft Store...", juce::dontSendNotification);
    void* window = getPeer() != nullptr ? getPeer()->getNativeHandle() : nullptr;
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    entitlement->purchase(window, [safeThis](inputrack::EntitlementResult result) {
        if (safeThis == nullptr) return;
        safeThis->updateEntitlementUi(result);
    });
}

void MainComponent::startProTrial()
{
    if (entitlement->isBusy() || entitlement->state().hasProAccess()) return;
    proButton.setEnabled(false);
    status.setText("Starting your 14-day Pro trial...", juce::dontSendNotification);
    void* window = getPeer() != nullptr ? getPeer()->getNativeHandle() : nullptr;
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    entitlement->startTrial(window, [safeThis](inputrack::EntitlementResult result) {
        if (safeThis == nullptr) return;
        safeThis->updateEntitlementUi(result);
    });
}

void MainComponent::updateEntitlementUi(const inputrack::EntitlementResult& result)
{
    juce::String hotkeyWarning;
    proButton.setEnabled(true);
    proButton.setButtonText(result.permanent ? "Pro  ✓"
        : result.trial ? "Trial " + juce::String(result.trialDaysRemaining) + "d"
                       : "Get Pro");
    proButton.setComponentID(result.hasProAccess() ? "secondary" : "primary");
    const auto hasHotkeyAccess = inputrack::hasFeatureAccess(
        inputrack::ProductFeature::globalHotkeys, result);
    if (hasHotkeyAccess && globalHotkeys == nullptr) {
        globalHotkeys = std::make_unique<GlobalHotkeys>(
            [this] { toggleGlobalBypass(); },
            [this](int index) { activateProfileAtIndex(index); });
        hotkeyWarning = globalHotkeys->failureMessage();
        if (restoreActiveProfile && activeProfileName.isNotEmpty()) {
            if (const auto profile = profiles.find(activeProfileName); profile.has_value())
                activateProfile(*profile);
        }
    } else if (!hasHotkeyAccess) {
        globalHotkeys.reset();
    }
    const auto entitlementMessage = result.message.trim();
    const auto combinedMessage = entitlementMessage.isNotEmpty() && hotkeyWarning.isNotEmpty()
        ? entitlementMessage + " " + hotkeyWarning
        : entitlementMessage.isNotEmpty() ? entitlementMessage : hotkeyWarning;
    if (combinedMessage.isNotEmpty())
        status.setText(combinedMessage, juce::dontSendNotification);
    repaint();
}

void MainComponent::showApplicationMenu()
{
    juce::PopupMenu popup;
    popup.addItem(7, "Setup assistant...");
    popup.addSeparator();
    popup.addItem(1, engine.isGloballyBypassed() ? "Enable all effects" : "Bypass all effects");
    popup.addItem(2, "Scan VST3 effects...");
    const auto blocked = blockedModules();
    popup.addItem(4, "Rescan " + juce::String(blocked.size()) + " skipped plug-in"
                      + (blocked.size() == 1 ? "" : "s"),
                  !blocked.isEmpty());
    const auto hasProAccess = inputrack::hasFeatureAccess(
        inputrack::ProductFeature::globalHotkeys, entitlement->state());
    popup.addItem(5, "Windows startup settings...");
    const auto hotkeyFailure = globalHotkeys != nullptr
        ? globalHotkeys->failureMessage() : juce::String{};
    popup.addItem(6, !hasProAccess
                         ? "Hotkeys: Ctrl+Alt+B, Ctrl+Alt+1..9 (Pro)"
                         : hotkeyFailure.isEmpty()
                             ? "Hotkeys active: Ctrl+Alt+B, Ctrl+Alt+1..9"
                             : "Some global hotkeys are unavailable",
                  false);
#if !INPUTRACK_STORE_BUILD
    popup.addSeparator();
    popup.addItem(3, "Check for updates");
#endif
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&appMenu),
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            if (result == 1) {
                const auto bypassed = !safeThis->engine.isGloballyBypassed();
                safeThis->engine.setGloballyBypassed(bypassed);
                safeThis->chainList.repaint();
            }
            else if (result == 2) safeThis->showPluginBrowser();
#if !INPUTRACK_STORE_BUILD
            else if (result == 3) safeThis->startUpdateCheck(true);
#endif
            else if (result == 4) safeThis->rescanBlockedPlugins();
            else if (result == 5) juce::URL("ms-settings:startupapps").launchInDefaultBrowser();
            else if (result == 7) safeThis->showSetupAssistant();
        });
}

void MainComponent::showSetupAssistant()
{
    if (setupDialog != nullptr) return;
    setupAssistantSeen = true;
    routingSignalSeen = false;
    saveSettings();
    setupDialog = std::make_unique<juce::AlertWindow>(
        "Connect InputRack to your apps",
        "1. Select your physical microphone under INPUT.\n"
        "2. Select CABLE Input (VB-Audio Virtual Cable) under OUTPUT.\n"
        "3. Turn Monitor on to send the output into the cable.\n"
        "4. Speak into the microphone, then run the routing test.\n\n"
        "In Discord or OBS, choose CABLE Output as the microphone.",
        juce::MessageBoxIconType::NoIcon);
    setupDialog->addButton("Run routing test", 1,
                           juce::KeyPress(juce::KeyPress::returnKey));
    setupDialog->addButton("Get VB-CABLE", 2);
    setupDialog->addButton("Later", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<MainComponent> safeThis(this);
    setupDialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis](int result) {
            if (safeThis == nullptr) return;
            safeThis->setupDialog.reset();
            if (result == 1) safeThis->runRoutingTest();
            else if (result == 2)
                juce::URL("https://vb-audio.com/Cable/").launchInDefaultBrowser();
        }), false);
}

void MainComponent::runRoutingTest()
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    engine.deviceManager().getAudioDeviceSetup(setup);
    const auto routing = inputrack::PluginChainEngine::evaluateRouting(
        setup.inputDeviceName, setup.outputDeviceName,
        engine.isMonitoringEnabled(), routingSignalSeen);

    juce::StringArray checks;
    checks.add(juce::String(routing.inputSelected ? "OK   " : "FIX  ")
               + "Microphone: "
               + (routing.inputSelected ? setup.inputDeviceName : "none selected"));
    checks.add(juce::String(routing.cableOutputSelected ? "OK   " : "FIX  ")
               + "Cable output: "
               + (setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : "none selected"));
    checks.add(juce::String(routing.outputEnabled ? "OK   " : "FIX  ")
               + "Output to cable is on");
    checks.add(juce::String(routing.signalSeen ? "OK   " : "FIX  ")
               + "Processed signal reached the output");
    auto message = checks.joinIntoString("\n");
    if (routing.ready()) {
        const auto capture = inputrack::PluginChainEngine::pairedCaptureName(
            setup.outputDeviceName);
        message += "\n\nRouting is ready. Select "
            + (capture.isNotEmpty() ? capture : "the cable capture endpoint")
            + " as the microphone in Discord, OBS or another app.";
        status.setText("Routing test passed.", juce::dontSendNotification);
    } else {
        message += "\n\nFix the marked items, speak into the microphone and run the test again.";
        status.setText("Routing test needs attention.", juce::dontSendNotification);
    }
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(routing.ready() ? juce::MessageBoxIconType::InfoIcon
                                          : juce::MessageBoxIconType::WarningIcon)
            .withTitle(routing.ready() ? "Routing test passed" : "Routing test incomplete")
            .withMessage(message)
            .withButton("OK")
            .withAssociatedComponent(this),
        nullptr);
}

void MainComponent::selectAndOpenPlugin(int row)
{
    if (!juce::isPositiveAndBelow(row, engine.pluginCount())) return;
    if (engine.isPluginMissing(row)) {
        showError("Plug-in unavailable",
                  "This rack entry is a pass-through placeholder. Install and scan the plug-in, then reload the preset.");
        return;
    }
    chainList.selectRow(row);
    openSelectedPlugin();
}

void MainComponent::togglePluginBypass(int row)
{
    if (!juce::isPositiveAndBelow(row, engine.pluginCount())) return;
    engine.setBypassed(row, !engine.isBypassed(row));
    chainList.repaintRow(row);
    persistRecoveryState();
}

void MainComponent::movePluginRow(int row, int destination)
{
    if (!juce::isPositiveAndBelow(row, engine.pluginCount())
        || !juce::isPositiveAndBelow(destination, engine.pluginCount()) || row == destination)
        return;
    engine.movePlugin(row, destination);
    refresh();
    chainList.selectRow(destination);
}

void MainComponent::removePluginRow(int row)
{
    if (!juce::isPositiveAndBelow(row, engine.pluginCount())) return;
    if (editorPlugin == engine.pluginAt(row)) {
        editorWindow.reset();
        editorPlugin = nullptr;
    }
    engine.removePlugin(row);
    refresh();
}

void MainComponent::scanPlugins()
{
    if (isThreadRunning()) return;
    const auto helper = pluginScannerExecutable();
    if (!helper.existsAsFile()) {
        showError("Scanner unavailable",
                  "InputRackPluginScanner is missing. Reinstall InputRack and try again.");
        return;
    }
    scanTimeouts->clear();
    moduleScanStartedAt.store(juce::Time::getMillisecondCounterHiRes());
    engine.knownPlugins().setCustomScanner(
        std::make_unique<inputrack::IsolatedPluginScanner>(
            helper, inputrack::IsolatedPluginScanner::defaultTimeoutMilliseconds,
            scanTimeouts));
    scan.setEnabled(false);
    scanProgress.store(0.0f);
    scanFinished.store(false);
    status.setText("Preparing VST3 scan...", juce::dontSendNotification);
    startThread();
}

/** Everything a scan refused to hand over, whether it crashed or ran long. */
juce::StringArray MainComponent::blockedModules()
{
    juce::StringArray blocked = engine.knownPlugins().getBlacklistedFiles();
    blocked.mergeArray(scanTimeouts->files());
    return blocked;
}

/*
 * The blacklist is a permanent verdict written on a single bad scan, so it
 * needs a way back. Forgetting the verdict and rescanning is that way: a module
 * that really is broken lands on the blacklist again within the same run.
 */
void MainComponent::rescanBlockedPlugins()
{
    if (isThreadRunning()) return;
    engine.knownPlugins().clearBlacklistedFiles();
    scanTimeouts->clear();
    deadMansPedalFile().deleteFile();
    savePluginCache();
    scanPlugins();
}

void MainComponent::run()
{
    for (int i = 0; i < engine.formatManager().getNumFormats(); ++i) {
        if (threadShouldExit()) return;
        auto* format = engine.formatManager().getFormat(i);
        if (format->getName() != "VST3") continue;
        juce::PluginDirectoryScanner scanner(engine.knownPlugins(), *format,
            format->getDefaultLocationsToSearch(), true, deadMansPedalFile());
        juce::String current;
        while (!threadShouldExit()) {
            {
                // VST3 identifies a module by its path, which is too long for
                // the status strip and buries the name the user is waiting on.
                const auto next =
                    moduleDisplayName(scanner.getNextPluginFileThatWillBeScanned());
                const juce::ScopedLock lock(scanStatusLock);
                if (next != pluginBeingScanned) {
                    pluginBeingScanned = next;
                    moduleScanStartedAt.store(juce::Time::getMillisecondCounterHiRes());
                }
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

/*
 * The picker order is a preference, not session state: it is stored beside the
 * plug-in cache and read back on the next launch. The search text deliberately
 * is not — rediscovering yesterday's filter on an apparently short list would
 * look like plug-ins had gone missing.
 */
juce::KnownPluginList::SortMethod MainComponent::selectedSort() const
{
    switch (pluginSort.getSelectedId()) {
        case 2: return juce::KnownPluginList::sortByManufacturer;
        case 3: return juce::KnownPluginList::sortByCategory;
        default: return juce::KnownPluginList::sortAlphabetically;
    }
}

void MainComponent::loadSettings()
{
    const auto settings = juce::JSON::parse(settingsFile());
    const auto stored = settings.getProperty("pluginSort", "name").toString();
    activeProfileName = settings.getProperty("activeProfile", {}).toString();
    setupAssistantSeen = static_cast<bool>(settings.getProperty("setupAssistantSeen", false));
    storedSampleRate = static_cast<double>(settings.getProperty("sampleRate", 0.0));
    const auto id = stored == "manufacturer" ? 2 : stored == "category" ? 3 : 1;
    // A missing or damaged file must not keep the picker empty, so anything
    // unrecognised falls back to sorting by name.
    pluginSort.setSelectedId(id, juce::dontSendNotification);
}

void MainComponent::saveSettings() const
{
    pluginDataDirectory().createDirectory();
    const auto name = pluginSort.getSelectedId() == 2   ? "manufacturer"
                      : pluginSort.getSelectedId() == 3 ? "category"
                                                        : "name";
    juce::DynamicObject::Ptr settings = new juce::DynamicObject();
    settings->setProperty("pluginSort", name);
    settings->setProperty("activeProfile", activeProfileName);
    settings->setProperty("setupAssistantSeen", setupAssistantSeen);
    settings->setProperty("sampleRate", storedSampleRate);
    settingsFile().replaceWithText(juce::JSON::toString(juce::var(settings.get())));
}

void MainComponent::refreshAvailablePlugins()
{
    const auto sort = selectedSort();
    visiblePlugins = inputrack::filterAndSortPlugins(engine.knownPlugins().getTypes(),
                                                      {}, sort);
    if (activePluginBrowser != nullptr) activePluginBrowser->refreshResults();
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

void MainComponent::addPluginAtVisibleIndex(int index)
{
    if (!juce::isPositiveAndBelow(index, visiblePlugins.size())) return;
    juce::String error;
    if (!engine.addPlugin(visiblePlugins.getReference(index), error))
        showError("Could not load plug-in", error);
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
    chooser = std::make_unique<juce::FileChooser>("Save Rack", juce::File{}, "*.inputrack.json");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto file = fc.getResult();
            if (file != juce::File{} && !file.replaceWithText(engine.captureState().toJson()))
                showError("Could not save preset", file.getFullPathName());
        });
}

void MainComponent::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser>("Load Rack", juce::File{}, "*.inputrack.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (file == juce::File{}) return;
            try {
                editorWindow.reset();
                editorPlugin = nullptr;
                juce::String error;
                if (!engine.restoreState(inputrack::ChainState::fromJson(file.loadFileAsString()), error))
                    showError("Could not restore preset", error);
                else if (error.isNotEmpty())
                    showError("Preset loaded with missing plug-ins", error);
                refresh();
            } catch (const std::exception& e) { showError("Invalid preset", e.what()); }
        });
}

void MainComponent::showError(const juce::String& title, const juce::String& message)
{
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
}

void MainComponent::refresh()
{
    chainList.updateContent();
    chainList.repaint();
    repaint(chainPanel);
    persistRecoveryState();
}

/*
 * Written after every rack mutation so a crash mid-session loses nothing more
 * than the last few seconds of edits. Plain file I/O on the message thread
 * matches savePreset(), which already does the same synchronously.
 */
void MainComponent::persistRecoveryState() const
{
    pluginDataDirectory().createDirectory();
    recoveryStateFile().replaceWithText(engine.captureState().toJson());
}

/*
 * crashMarkerFile() only survives to the next launch when the previous
 * process never reached the destructor: a crash, a kill, or a power loss.
 * Recovering silently trades a start-up surprise for never losing a rack.
 */
void MainComponent::recoverFromUncleanShutdown()
{
    if (!crashMarkerFile().existsAsFile()) return;
    const auto recovery = recoveryStateFile();
    if (!recovery.existsAsFile()) return;
    try {
        juce::String error;
        if (engine.restoreState(inputrack::ChainState::fromJson(recovery.loadFileAsString()), error))
            status.setText(error.isNotEmpty()
                               ? "Recovered your rack with pass-through placeholders for unavailable plug-ins."
                               : "Recovered your rack after an unexpected shutdown.",
                           juce::dontSendNotification);
    } catch (const std::exception&) {
        // A damaged recovery file is not worth surfacing as an error; the
        // session simply starts with an empty rack instead.
    }
}

/*
 * JUCE's AudioDeviceManager broadcasts this both for setup changes and for a
 * device list change on backends that support hot-plug notifications (WASAPI
 * on Windows). A null current device means the one in use just disappeared.
 */
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (engine.deviceManager().getCurrentAudioDevice() != nullptr) {
        refreshDeviceSelectors();
        status.setText(routingStatus(), juce::dontSendNotification);
        return;
    }
    const auto error = engine.initialiseAudio();
    refreshDeviceSelectors();
    status.setText(error.isNotEmpty() ? "Audio: " + error
                                       : "Input device changed. " + routingStatus(),
                   juce::dontSendNotification);
}

#if !INPUTRACK_STORE_BUILD
/*
 * Update work reports through the same status label as everything else, so a
 * check never steals the routing information for longer than it takes to
 * answer. A failed check is reported but not dwelt on: being offline is not an
 * error worth interrupting anyone over.
 */
void MainComponent::startUpdateCheck(bool requestedByUser)
{
    if (updates.isBusy()) return;
    if (requestedByUser) status.setText("Checking for updates...", juce::dontSendNotification);
    checkUpdates.setEnabled(false);
    updates.check([this, requestedByUser](std::optional<inputrack::AvailableUpdate> found,
                                          juce::String error) {
        checkUpdates.setEnabled(true);
        if (error.isNotEmpty()) {
            if (requestedByUser) status.setText(error, juce::dontSendNotification);
            return;
        }
        updateCheckFinished(found, error);
        if (!found.has_value() && requestedByUser)
            status.setText("InputRack is up to date.", juce::dontSendNotification);
    });
}

void MainComponent::updateCheckFinished(std::optional<inputrack::AvailableUpdate> found,
                                        const juce::String&)
{
    availableUpdate = found;
    if (!found.has_value()) return;

    installUpdate.setButtonText("Install " + found->version);
    installUpdate.setVisible(true);
    status.setText("Version " + found->version + " is available.", juce::dontSendNotification);
    resized();
}

void MainComponent::startUpdateDownload()
{
    if (!availableUpdate.has_value() || updates.isBusy()) return;
    installUpdate.setEnabled(false);
    checkUpdates.setEnabled(false);
    status.setText("Downloading version " + availableUpdate->version + "...",
                   juce::dontSendNotification);

    updates.download(*availableUpdate, [this](juce::File setup, juce::String error) {
        installUpdate.setEnabled(true);
        checkUpdates.setEnabled(true);
        if (error.isNotEmpty()) {
            showError("Update failed", error);
            status.setText(routingStatus(), juce::dontSendNotification);
            return;
        }
        // The installer closes this process through the restart manager, so the
        // engine is shut down first and the app asked to quit straight after.
        if (!inputrack::UpdateChecker::launchInstaller(setup)) {
            showError("Update failed", "The installer could not be started.");
            return;
        }
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    });
}
#endif

/*
 * The processed chain leaves InputRack through the selected output device, so
 * the status line has to name that device and the capture endpoint another app
 * has to pick. Monitoring gates that connection, which is why an inactive
 * monitor is reported as the reason nothing is being published.
 */
juce::String MainComponent::routingStatus() const
{
    const auto output = engine.outputDeviceName();
    if (output.isEmpty()) return "No output device selected.";
    if (!engine.isMonitoringEnabled())
        return "Monitor is off, so nothing reaches " + output + " yet.";

    if (!inputrack::PluginChainEngine::looksLikeVirtualCable(output))
        return "Sending to " + output
             + ". Select a virtual audio cable here to make other apps hear the chain.";

    const auto capture = inputrack::PluginChainEngine::pairedCaptureName(output);
    if (capture.isEmpty())
        return "Sending to " + output + ". Select its capture side in Discord or OBS.";
    return "Sending to " + output + ". Select \"" + capture + "\" in Discord or OBS.";
}
