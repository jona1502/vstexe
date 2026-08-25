#pragma once
#include <vocalchain/ChainState.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace vocalchain {
class PluginChainEngine final {
public:
    static constexpr double sampleRate = 48000.0;
    static constexpr int channelCount = 1;
    PluginChainEngine();
    ~PluginChainEngine();
    juce::AudioDeviceManager& deviceManager() noexcept;
    juce::AudioPluginFormatManager& formatManager() noexcept;
    juce::KnownPluginList& knownPlugins() noexcept;
    juce::String initialiseAudio(const juce::String& preferredInput = {});
    void shutdownAudio();
    bool addPlugin(const juce::PluginDescription&, juce::String& error);
    void removePlugin(int);
    void movePlugin(int from, int to);
    void setBypassed(int, bool);
    bool isBypassed(int) const;
    juce::AudioPluginInstance* pluginAt(int) const;
    int pluginCount() const noexcept;
    ChainState captureState() const;
    bool restoreState(const ChainState&, juce::String& error);

private:
    struct HostedPlugin {
        juce::PluginDescription description;
        juce::AudioProcessorGraph::Node::Ptr node;
        bool bypassed{};
    };
    void rebuildConnections();
    juce::AudioDeviceManager devices;
    juce::AudioProcessorPlayer player;
    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList plugins;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorGraph::Node::Ptr inputNode, outputNode;
    juce::Array<HostedPlugin> chain;
};
}
