#include <inputrack/ChainState.h>
#include <iostream>
#include <stdexcept>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void expectInvalid(const juce::String& json, const char* message)
{
    try {
        inputrack::ChainState::fromJson(json);
        expect(false, message);
    } catch (const std::invalid_argument&) {
    }
}
}

int main()
{
    inputrack::ChainState state;
    state.setName("Streaming Voice");
    juce::PluginDescription plugin;
    plugin.name = "Test Compressor";
    plugin.manufacturerName = "InputRack Tests";
    plugin.fileOrIdentifier = "test.plugin";
    plugin.uniqueId = 42;
    plugin.pluginFormatName = "VST3";
    const char bytes[] = {1, 2, 3, 4};
    juce::MemoryBlock pluginState(bytes, sizeof(bytes));
    state.addPlugin(plugin, pluginState);
    state.setBypassed(0, true);

    const auto restored = inputrack::ChainState::fromJson(state.toJson());
    expect(restored.name() == "Streaming Voice", "preset name round-trips");
    expect(restored.size() == 1, "plug-in count round-trips");
    expect(restored.pluginAt(0).getProperty("name") == "Test Compressor", "plug-in identity round-trips");
    expect(restored.isBypassed(0), "bypass state round-trips");
    expect(restored.pluginState(0) == pluginState, "binary plug-in state round-trips");

    expectInvalid(R"({"_type":"inputRack","properties":{},"children":[]})",
                  "a missing schema version is rejected");
    expectInvalid(
        R"({"_type":"inputRack","properties":{"schemaVersion":1},"children":[{"_type":"plugin","properties":{"name":"Broken","format":"VST3","fileOrIdentifier":"broken","state":"%%%"},"children":[]}]})",
        "invalid base64 plug-in state is rejected");
    expectInvalid(
        R"({"_type":"inputRack","properties":{"schemaVersion":1},"children":[{"_type":"plugin","properties":{"name":"Broken","format":"VST3","state":""},"children":[]}]})",
        "an incomplete plug-in identity is rejected");
    return failures == 0 ? 0 : 1;
}
