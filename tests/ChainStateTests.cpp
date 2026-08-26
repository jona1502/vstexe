#include <inputrack/ChainState.h>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
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
    return failures == 0 ? 0 : 1;
}
