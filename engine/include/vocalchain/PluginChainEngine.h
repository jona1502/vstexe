#pragma once
#include <vocalchain/ChainState.h>
#include <vocalchain/VirtualMicrophone.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace vocalchain {
class PluginChainEngine final {
public:
    static constexpr double sampleRate = 48000.0;
    static constexpr int inputChannelCount = 1;
    static constexpr int outputChannelCount = 2;
    static constexpr int pluginChannelCount = 1;
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
    void setMonitoringEnabled(bool);
    bool isMonitoringEnabled() const noexcept;
    bool isVirtualMicrophoneRunning() const noexcept;
    juce::String virtualMicrophoneStatus() const;
    /** Name of the device the processed chain is written to, empty when closed. */
    juce::String outputDeviceName() const;
    /** Capture endpoint a listening app should select, empty when unknown. */
    static juce::String pairedCaptureName(const juce::String& outputDeviceName);
    /** True when the name matches a known virtual audio cable. */
    static bool looksLikeVirtualCable(const juce::String& deviceName);
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
    std::unique_ptr<VirtualMicrophone> virtualMicrophone;
    juce::String virtualMicStatus;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorGraph::Node::Ptr inputNode, outputNode, virtualMicNode;
    juce::Array<HostedPlugin> chain;
    bool monitoringEnabled{};
};
}
