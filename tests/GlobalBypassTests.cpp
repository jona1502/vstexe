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
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    juce::PluginDescription description;
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

    expect(!engine.isGloballyBypassed(), "the rack starts audible");

    juce::String error;
    expect(engine.addPlugin(description("Compressor", "compressor"), error),
           "a plug-in can be added while the rack is audible");
    expect(!engine.isBypassed(0), "adding a plug-in does not bypass it individually");

    engine.setGloballyBypassed(true);
    expect(engine.isGloballyBypassed(), "global bypass reports as active once set");
    expect(engine.pluginCount() == 1 && !engine.isBypassed(0),
           "global bypass leaves the chain contents and per-plug-in flags untouched");

    engine.setGloballyBypassed(false);
    expect(!engine.isGloballyBypassed(), "global bypass can be released again");
    expect(engine.pluginCount() == 1, "releasing global bypass keeps the rack intact");

    if (failures == 0) std::cout << "GlobalBypassTests passed\n";
    return failures == 0 ? 0 : 1;
}
