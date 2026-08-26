#include <inputrack/ChannelAdapterProcessor.h>
#include <inputrack/PluginChainEngine.h>

namespace inputrack {
namespace {
struct PluginLayout {
    int inputs{};
    int outputs{};

    explicit operator bool() const noexcept { return inputs > 0 && outputs > 0; }
};

PluginLayout configurePluginLayout(juce::AudioPluginInstance& plugin)
{
    if (plugin.getBusCount(true) == 0 || plugin.getBusCount(false) == 0)
        return {};

    static constexpr PluginLayout candidates[] = {
        {2, 2}, {1, 2}, {1, 1}, {2, 1}
    };
    for (const auto candidate : candidates) {
        auto layout = plugin.getBusesLayout();
        layout.getChannelSet(true, 0) = candidate.inputs == 2
            ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
        layout.getChannelSet(false, 0) = candidate.outputs == 2
            ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::mono();
        if (plugin.setBusesLayout(layout)) {
            plugin.setRateAndBufferSizeDetails(PluginChainEngine::sampleRate, 256);
            return candidate;
        }
    }
    return {};
}

class VirtualMicrophoneSink final : public juce::AudioProcessor {
public:
    explicit VirtualMicrophoneSink(VirtualMicrophone& backend)
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::mono(), true)),
          microphone(backend)
    {
    }

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
            && layouts.getMainOutputChannelSet().isDisabled();
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (buffer.getNumChannels() > 0)
            microphone.push(buffer.getReadPointer(0), buffer.getNumSamples());
    }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Virtual microphone sink"; }
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
    VirtualMicrophone& microphone;
};
} // namespace

PluginChainEngine::PluginChainEngine()
{
    formats.addDefaultFormats();
    virtualMicrophone = VirtualMicrophone::createPlatformBackend();
    graph = std::make_unique<juce::AudioProcessorGraph>();
    inputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    inputUpmixNode = graph->addNode(
        std::make_unique<ChannelAdapterProcessor>(
            ChannelAdapterProcessor::Direction::monoToStereo));
    outputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    virtualMicNode = graph->addNode(std::make_unique<VirtualMicrophoneSink>(*virtualMicrophone));
    rebuildConnections();
}

PluginChainEngine::~PluginChainEngine() { shutdownAudio(); }
juce::AudioDeviceManager& PluginChainEngine::deviceManager() noexcept { return devices; }
juce::AudioPluginFormatManager& PluginChainEngine::formatManager() noexcept { return formats; }
juce::KnownPluginList& PluginChainEngine::knownPlugins() noexcept { return plugins; }

juce::String PluginChainEngine::initialiseAudio(const juce::String& preferredInput)
{
    auto error = devices.initialise(inputChannelCount, outputChannelCount, nullptr, true, preferredInput);
    if (error.isNotEmpty()) return error;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.getAudioDeviceSetup(setup);
    setup.sampleRate = sampleRate;
    setup.inputChannels.clear(); setup.inputChannels.setBit(0);
    setup.outputChannels.clear();
    setup.outputChannels.setRange(0, outputChannelCount, true);
    if (error = devices.setAudioDeviceSetup(setup, true); error.isNotEmpty()) return error;
    auto* audioDevice = devices.getCurrentAudioDevice();
    virtualMicStatus = virtualMicrophone->start(
        sampleRate, inputChannelCount,
        audioDevice != nullptr ? audioDevice->getCurrentBufferSizeSamples() : 256);
    player.setProcessor(graph.get());
    devices.addAudioCallback(&player);
    if (!rebuildConnections()) {
        devices.removeAudioCallback(&player);
        player.setProcessor(nullptr);
        virtualMicrophone->stop();
        devices.closeAudioDevice();
        return "The mono input could not be connected to the stereo processing graph.";
    }
    return {};
}

void PluginChainEngine::shutdownAudio()
{
    devices.removeAudioCallback(&player);
    player.setProcessor(nullptr);
    virtualMicrophone->stop();
    devices.closeAudioDevice();
}

bool PluginChainEngine::addPlugin(const juce::PluginDescription& description, juce::String& error)
{
    auto instance = formats.createPluginInstance(description, sampleRate, 256, error);
    if (!instance) return false;
    const auto layout = configurePluginLayout(*instance);
    if (!layout) {
        error = "The plug-in supports neither mono nor stereo audio processing.";
        return false;
    }
    if (auto node = graph->addNode(std::move(instance))) {
        juce::AudioProcessorGraph::Node::Ptr inputAdapter;
        juce::AudioProcessorGraph::Node::Ptr outputAdapter;
        if (layout.inputs == 1) {
            inputAdapter = graph->addNode(
                std::make_unique<ChannelAdapterProcessor>(
                    ChannelAdapterProcessor::Direction::stereoToMono));
        }
        if (layout.outputs == 1) {
            outputAdapter = graph->addNode(
                std::make_unique<ChannelAdapterProcessor>(
                    ChannelAdapterProcessor::Direction::monoToStereo));
        }
        chain.add({description, node, inputAdapter, outputAdapter,
                   layout.inputs, layout.outputs, false});
        if (!rebuildConnections()) {
            const auto added = chain.getLast();
            chain.removeLast();
            if (added.inputAdapter != nullptr) graph->removeNode(added.inputAdapter->nodeID);
            if (added.outputAdapter != nullptr) graph->removeNode(added.outputAdapter->nodeID);
            graph->removeNode(added.node->nodeID);
            rebuildConnections();
            error = "The plug-in's audio channels could not be connected.";
            return false;
        }
        return true;
    }
    error = "The plug-in could not be added to the audio graph.";
    return false;
}

void PluginChainEngine::removePlugin(int index)
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return;
    const auto item = chain.getReference(index);
    chain.remove(index);
    if (item.inputAdapter != nullptr) graph->removeNode(item.inputAdapter->nodeID);
    if (item.outputAdapter != nullptr) graph->removeNode(item.outputAdapter->nodeID);
    graph->removeNode(item.node->nodeID);
    rebuildConnections();
}

void PluginChainEngine::movePlugin(int from, int to)
{
    if (!juce::isPositiveAndBelow(from, chain.size()) || !juce::isPositiveAndBelow(to, chain.size())) return;
    chain.move(from, to); rebuildConnections();
}

void PluginChainEngine::setBypassed(int index, bool bypassed)
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return;
    auto& item = chain.getReference(index);
    item.bypassed = bypassed;
    rebuildConnections();
}

bool PluginChainEngine::isBypassed(int index) const
{
    return juce::isPositiveAndBelow(index, chain.size())
        && chain.getReference(index).bypassed;
}

void PluginChainEngine::setMonitoringEnabled(bool enabled)
{
    if (monitoringEnabled == enabled) return;
    monitoringEnabled = enabled;
    rebuildConnections();
}

bool PluginChainEngine::isMonitoringEnabled() const noexcept { return monitoringEnabled; }
bool PluginChainEngine::isVirtualMicrophoneRunning() const noexcept
{
    return virtualMicrophone->isRunning();
}
juce::String PluginChainEngine::virtualMicrophoneStatus() const { return virtualMicStatus; }

juce::String PluginChainEngine::outputDeviceName() const
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.getAudioDeviceSetup(setup);
    return setup.outputDeviceName;
}

/*
 * Virtual cables expose a render and a capture endpoint that belong together.
 * Only the render side is selectable here, so the capture name is derived for
 * the products whose naming is fixed; anything else stays unknown on purpose
 * rather than guessing a device the user cannot find.
 */
juce::String PluginChainEngine::pairedCaptureName(const juce::String& outputDeviceName)
{
    if (outputDeviceName.containsIgnoreCase("CABLE Input"))
        return outputDeviceName.replace("CABLE Input", "CABLE Output", true);
    if (outputDeviceName.containsIgnoreCase("SteelSeries Sonar"))
        return outputDeviceName;
    return {};
}

bool PluginChainEngine::looksLikeVirtualCable(const juce::String& deviceName)
{
    static const char* const markers[] = {"CABLE", "VB-Audio", "Voicemeeter", "SteelSeries Sonar",
                                          "Virtual Audio", "Virtual Cable", "Wave Link"};
    for (const auto* marker : markers)
        if (deviceName.containsIgnoreCase(marker)) return true;
    return false;
}

juce::AudioPluginInstance* PluginChainEngine::pluginAt(int index) const
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return nullptr;
    return dynamic_cast<juce::AudioPluginInstance*>(chain.getReference(index).node->getProcessor());
}

int PluginChainEngine::pluginInputChannelCountAt(int index) const
{
    return juce::isPositiveAndBelow(index, chain.size())
        ? chain.getReference(index).inputChannelCount : 0;
}

int PluginChainEngine::pluginOutputChannelCountAt(int index) const
{
    return juce::isPositiveAndBelow(index, chain.size())
        ? chain.getReference(index).outputChannelCount : 0;
}

int PluginChainEngine::pluginCount() const noexcept { return chain.size(); }

ChainState PluginChainEngine::captureState() const
{
    ChainState state;
    for (int i = 0; i < chain.size(); ++i) {
        juce::MemoryBlock data;
        chain.getReference(i).node->getProcessor()->getStateInformation(data);
        state.addPlugin(chain.getReference(i).description, data);
        state.setBypassed(i, chain.getReference(i).bypassed);
    }
    return state;
}

bool PluginChainEngine::restoreState(const ChainState& state, juce::String& error)
{
    while (!chain.isEmpty()) removePlugin(chain.size() - 1);
    for (int i = 0; i < state.size(); ++i) {
        const auto item = state.pluginAt(i);
        juce::PluginDescription description;
        description.name = item.getProperty("name").toString();
        description.manufacturerName = item.getProperty("manufacturer").toString();
        description.fileOrIdentifier = item.getProperty("fileOrIdentifier").toString();
        description.uniqueId = static_cast<int>(item.getProperty("uniqueId"));
        description.deprecatedUid = static_cast<int>(item.getProperty("deprecatedUid"));
        description.pluginFormatName = item.getProperty("format").toString();
        if (!addPlugin(description, error)) return false;
        const auto data = state.pluginState(i);
        pluginAt(i)->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
        setBypassed(i, state.isBypassed(i));
    }
    return true;
}

bool PluginChainEngine::rebuildConnections()
{
    for (const auto& connection : graph->getConnections())
        graph->removeConnection(connection);

    bool connected = graph->addConnection(
        {{inputNode->nodeID, 0}, {inputUpmixNode->nodeID, 0}});
    auto previous = inputUpmixNode->nodeID;
    for (auto& item : chain) {
        if (item.bypassed) continue;

        if (item.inputChannelCount == 2) {
            for (int channel = 0; channel < processingChannelCount; ++channel)
                connected = graph->addConnection(
                    {{previous, channel}, {item.node->nodeID, channel}}) && connected;
        }
        else {
            for (int channel = 0; channel < processingChannelCount; ++channel)
                connected = graph->addConnection(
                    {{previous, channel}, {item.inputAdapter->nodeID, channel}}) && connected;
            connected = graph->addConnection(
                {{item.inputAdapter->nodeID, 0}, {item.node->nodeID, 0}}) && connected;
        }

        if (item.outputChannelCount == 2) {
            previous = item.node->nodeID;
        }
        else {
            connected = graph->addConnection(
                {{item.node->nodeID, 0}, {item.outputAdapter->nodeID, 0}}) && connected;
            previous = item.outputAdapter->nodeID;
        }
    }
    if (monitoringEnabled) {
        for (int channel = 0; channel < outputChannelCount; ++channel)
            connected = graph->addConnection(
                {{previous, channel}, {outputNode->nodeID, channel}}) && connected;
    }
    connected = graph->addConnection({{previous, 0}, {virtualMicNode->nodeID, 0}}) && connected;
    return connected;
}
}
