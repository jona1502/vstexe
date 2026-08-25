#include <vocalchain/PluginChainEngine.h>

namespace vocalchain {
PluginChainEngine::PluginChainEngine()
{
    formats.addDefaultFormats();
    graph = std::make_unique<juce::AudioProcessorGraph>();
    inputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    outputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    rebuildConnections();
}

PluginChainEngine::~PluginChainEngine() { shutdownAudio(); }
juce::AudioDeviceManager& PluginChainEngine::deviceManager() noexcept { return devices; }
juce::AudioPluginFormatManager& PluginChainEngine::formatManager() noexcept { return formats; }
juce::KnownPluginList& PluginChainEngine::knownPlugins() noexcept { return plugins; }

juce::String PluginChainEngine::initialiseAudio(const juce::String& preferredInput)
{
    auto error = devices.initialise(channelCount, channelCount, nullptr, true, preferredInput);
    if (error.isNotEmpty()) return error;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.getAudioDeviceSetup(setup);
    setup.sampleRate = sampleRate;
    setup.inputChannels.clear(); setup.inputChannels.setBit(0);
    setup.outputChannels.clear(); setup.outputChannels.setBit(0);
    if (error = devices.setAudioDeviceSetup(setup, true); error.isNotEmpty()) return error;
    player.setProcessor(graph.get());
    devices.addAudioCallback(&player);
    return {};
}

void PluginChainEngine::shutdownAudio()
{
    devices.removeAudioCallback(&player);
    player.setProcessor(nullptr);
    devices.closeAudioDevice();
}

bool PluginChainEngine::addPlugin(const juce::PluginDescription& description, juce::String& error)
{
    auto instance = formats.createPluginInstance(description, sampleRate, 256, error);
    if (!instance) return false;
    instance->setPlayConfigDetails(channelCount, channelCount, sampleRate, 256);
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
    graph->addConnection({{previous, 0}, {outputNode->nodeID, 0}});
}
}
