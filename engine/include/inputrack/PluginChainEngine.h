#pragma once
#include <inputrack/ChainState.h>
#include <inputrack/MeteringProcessor.h>
#include <inputrack/VirtualMicrophone.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <optional>

namespace inputrack {
struct RoutingReadiness {
    bool inputSelected{};
    bool cableOutputSelected{};
    bool outputEnabled{};
    bool signalSeen{};

    bool ready() const noexcept
    {
        return inputSelected && cableOutputSelected && outputEnabled && signalSeen;
    }
};

class PluginChainEngine final {
public:
    static constexpr double defaultSampleRate = 48000.0;
    static constexpr int inputChannelCount = 1;
    static constexpr int outputChannelCount = 2;
    static constexpr int processingChannelCount = 2;
    PluginChainEngine();
    ~PluginChainEngine();
    juce::AudioDeviceManager& deviceManager() noexcept;
    juce::AudioPluginFormatManager& formatManager() noexcept;
    juce::KnownPluginList& knownPlugins() noexcept;
    juce::String initialiseAudio(const juce::String& preferredInput = {},
                                 double preferredSampleRate = 0.0);
    void shutdownAudio();
    /** The rate the open device is running the rack at. */
    double sampleRate() const noexcept;
    /** Rates the open device offers that are sensible for a voice chain. */
    juce::Array<double> availableSampleRates() const;
    /** Reopens the device at the given rate; returns an error when it refuses. */
    juce::String setSampleRate(double);
    /** True for a rate the picker is willing to offer. */
    static bool isUsableSampleRate(double) noexcept;
    bool addPlugin(const juce::PluginDescription&, juce::String& error);
    void removePlugin(int);
    void movePlugin(int from, int to);
    void setBypassed(int, bool);
    bool isBypassed(int) const;
    /** Bypasses the whole rack at once, leaving the per-plug-in flags untouched. */
    void setGloballyBypassed(bool);
    bool isGloballyBypassed() const noexcept;
    void setMonitoringEnabled(bool);
    bool isMonitoringEnabled() const noexcept;
    /** Peak sample magnitude since the last call, 0 for an unknown channel. */
    float consumeInputPeak(int channel) noexcept;
    float consumeOutputPeak(int channel) noexcept;
    /** True if the output stage clamped an over-scale sample since the last call. */
    bool consumeOutputClipped() noexcept;
    bool isVirtualMicrophoneRunning() const noexcept;
    juce::String virtualMicrophoneStatus() const;
    /** Name of the device the processed chain is written to, empty when closed. */
    juce::String outputDeviceName() const;
    /** Capture endpoint a listening app should select, empty when unknown. */
    static juce::String pairedCaptureName(const juce::String& outputDeviceName);
    /** True when the name matches a known virtual audio cable. */
    static bool looksLikeVirtualCable(const juce::String& deviceName);
    static RoutingReadiness evaluateRouting(const juce::String& inputDeviceName,
                                            const juce::String& outputDeviceName,
                                            bool outputEnabled,
                                            bool signalSeen);
    juce::AudioPluginInstance* pluginAt(int) const;
    bool isPluginMissing(int) const noexcept;
    int pluginInputChannelCountAt(int) const;
    int pluginOutputChannelCountAt(int) const;
    int pluginCount() const noexcept;
    ChainState captureState() const;
    bool restoreState(const ChainState&, juce::String& error);

private:
    struct HostedPlugin {
        juce::PluginDescription description;
        juce::AudioProcessorGraph::Node::Ptr node;
        juce::AudioProcessorGraph::Node::Ptr inputAdapter;
        juce::AudioProcessorGraph::Node::Ptr outputAdapter;
        int inputChannelCount{};
        int outputChannelCount{};
        bool missing{};
        bool bypassed{};
    };
    std::optional<HostedPlugin> createHostedPlugin(const juce::PluginDescription&,
                                                   juce::String& error);
    std::optional<HostedPlugin> createMissingPlugin(const juce::PluginDescription&);
    void removeHostedPlugin(const HostedPlugin&);
    bool rebuildConnections();
    double activeSampleRate{defaultSampleRate};
    juce::AudioDeviceManager devices;
    juce::AudioProcessorPlayer player;
    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList plugins;
    std::unique_ptr<VirtualMicrophone> virtualMicrophone;
    juce::String virtualMicStatus;
    std::unique_ptr<juce::AudioProcessorGraph> graph;
    juce::AudioProcessorGraph::Node::Ptr inputNode, inputUpmixNode, outputNode, virtualMicNode;
    juce::Array<HostedPlugin> chain;
    bool monitoringEnabled{};
    bool globallyBypassed{};
    juce::AudioProcessorGraph::Node::Ptr inputMeterNode, outputMeterNode;
    MeteringProcessor* inputMeter{};
    MeteringProcessor* outputMeter{};
};
}
