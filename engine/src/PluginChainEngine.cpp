#include <inputrack/PluginChainEngine.h>

namespace inputrack {
namespace {
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
        sampleRate, pluginChannelCount,
        audioDevice != nullptr ? audioDevice->getCurrentBufferSizeSamples() : 256);
    player.setProcessor(graph.get());
    devices.addAudioCallback(&player);
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
    instance->setPlayConfigDetails(pluginChannelCount, pluginChannelCount, sampleRate, 256);
    if (auto node = graph->addNode(std::move(instance))) {
        chain.add({description, node, false});
        rebuildConnections();
        return true;
    }
    error = "The plug-in could not be added to the audio graph.";
    return false;
}

void PluginChainEngine::removePlugin(int index)
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return;
    const auto id = chain.getReference(index).node->nodeID;
    chain.remove(index); graph->removeNode(id); rebuildConnections();
}

void PluginChainEngine::movePlugin(int from, int to)
{
    if (!juce::isPositiveAndBelow(from, chain.size()) || !juce::isPositiveAndBelow(to, chain.size())) return;
    chain.move(from, to); rebuildConnections();
}

void PluginChainEngine::setBypassed(int index, bool bypassed)
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return;
    auto& item = chain.getReference(index); item.bypassed = bypassed; item.node->setBypassed(bypassed);
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

void PluginChainEngine::rebuildConnections()
{
    for (const auto& connection : graph->getConnections())
        graph->removeConnection(connection);
    auto previous = inputNode->nodeID;
    for (auto& item : chain) {
        graph->addConnection({{previous, 0}, {item.node->nodeID, 0}});
        previous = item.node->nodeID;
    }
    if (monitoringEnabled) {
        for (int channel = 0; channel < outputChannelCount; ++channel)
            graph->addConnection({{previous, 0}, {outputNode->nodeID, channel}});
    }
    graph->addConnection({{previous, 0}, {virtualMicNode->nodeID, 0}});
}
}
