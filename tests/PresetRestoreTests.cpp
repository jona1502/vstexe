#include <inputrack/PluginChainEngine.h>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

class TestPlugin final : public juce::AudioPluginInstance {
public:
    explicit TestPlugin(juce::PluginDescription descriptionToUse)
        : AudioPluginInstance(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          description(std::move(descriptionToUse))
    {
    }

    void fillInPluginDescription(juce::PluginDescription& value) const override { value = description; }
    const juce::String getName() const override { return description.name; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& data) override { data = state; }
    void setStateInformation(const void* data, int size) override
    {
        state = juce::MemoryBlock(data, static_cast<size_t>(size));
    }

private:
    juce::PluginDescription description;
    juce::MemoryBlock state;
};

class TestFormat final : public juce::AudioPluginFormat {
public:
    juce::String getName() const override { return "InputRackTest"; }
    void findAllTypesForFile(juce::OwnedArray<juce::PluginDescription>&,
                             const juce::String&) override {}
    bool fileMightContainThisPluginType(const juce::String&) override { return true; }
    juce::String getNameOfPluginFromIdentifier(const juce::String& value) override { return value; }
    bool pluginNeedsRescanning(const juce::PluginDescription&) override { return false; }
    bool doesPluginStillExist(const juce::PluginDescription&) override { return true; }
    bool canScanForPlugins() const override { return true; }
    bool isTrivialToScan() const override { return true; }
    juce::StringArray searchPathsForPlugins(const juce::FileSearchPath&, bool, bool) override { return {}; }
    juce::FileSearchPath getDefaultLocationsToSearch() override { return {}; }
    bool requiresUnblockedMessageThreadDuringCreation(const juce::PluginDescription&) const override
    {
        return false;
    }

private:
    void createPluginInstance(const juce::PluginDescription& description, double, int,
                              PluginCreationCallback callback) override
    {
        if (description.fileOrIdentifier.startsWith("missing")) {
            callback(nullptr, "plug-in is not installed");
            return;
        }
        callback(std::make_unique<TestPlugin>(description), {});
    }
};

juce::PluginDescription description(const juce::String& name, const juce::String& identifier)
{
    juce::PluginDescription result;
    result.name = name;
    result.fileOrIdentifier = identifier;
    result.pluginFormatName = "InputRackTest";
    result.uniqueId = identifier.hashCode();
    return result;
}
}

int main()
{
    inputrack::PluginChainEngine engine;
    engine.formatManager().addFormat(new TestFormat());

    juce::String error;
    expect(engine.addPlugin(description("Existing", "existing"), error),
           "the initial rack can be created");
    auto* existing = engine.pluginAt(0);

    inputrack::ChainState broken;
    broken.addPlugin(description("Replacement", "replacement"), {});
    broken.addPlugin(description("Unavailable A", "missing-a"), {});
    broken.addPlugin(description("Unavailable B", "missing-b"), {});
    expect(engine.restoreState(broken, error),
           "a preset with missing plug-ins loads with placeholders");
    expect(engine.pluginCount() == 3 && engine.pluginAt(0) != existing,
           "the available part of the restored rack replaces the previous rack");
    expect(!engine.isPluginMissing(0) && engine.isPluginMissing(1)
               && engine.isPluginMissing(2),
           "only unavailable entries are marked as missing");
    expect(engine.pluginAt(1)->getName().contains("Unavailable A")
               && engine.pluginAt(1)->getName().containsIgnoreCase("missing"),
           "a missing entry is visible by its original name");
    expect(error.contains("Unavailable A") && error.contains("Unavailable B")
               && error.containsIgnoreCase("pass through"),
           "the restore warning names every missing plug-in and the safe behavior");
    const auto preserved = engine.captureState();
    expect(preserved.size() == 3
               && preserved.pluginAt(1).getProperty("fileOrIdentifier") == "missing-a",
           "saving the rack preserves a missing plug-in for a later machine");

    inputrack::ChainState valid;
    valid.addPlugin(description("Replacement", "replacement"), {});
    error.clear();
    expect(engine.restoreState(valid, error), "a complete preset is restored");
    expect(error.isEmpty(), "a complete preset produces no missing-plug-in warning");
    expect(engine.pluginCount() == 1 && engine.pluginAt(0)->getName() == "Replacement",
           "a successful restore atomically replaces the rack");

    if (failures == 0) std::cout << "PresetRestoreTests passed\n";
    return failures == 0 ? 0 : 1;
}
