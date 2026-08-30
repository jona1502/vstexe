#include <inputrack/ChannelAdapterProcessor.h>
#include <inputrack/PluginChainEngine.h>

#include <cmath>

namespace inputrack {
namespace {
struct PluginLayout {
    int inputs{};
    int outputs{};

    explicit operator bool() const noexcept { return inputs > 0 && outputs > 0; }
};

PluginLayout configurePluginLayout(juce::AudioPluginInstance& plugin, double sampleRate)
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
            plugin.setRateAndBufferSizeDetails(sampleRate, 256);
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

/** A stereo pass-through that preserves an unavailable plug-in's identity and state. */
class MissingPluginProcessor final : public juce::AudioPluginInstance {
public:
    explicit MissingPluginProcessor(juce::PluginDescription descriptionToKeep)
        : AudioPluginInstance(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          description(std::move(descriptionToKeep))
    {
    }

    void fillInPluginDescription(juce::PluginDescription& value) const override
    {
        value = description;
    }
    const juce::String getName() const override { return description.name + " (missing)"; }
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
    void getStateInformation(juce::MemoryBlock& data) override { data = savedState; }
    void setStateInformation(const void* data, int size) override
    {
        savedState = juce::MemoryBlock(data, static_cast<std::size_t>(size));
    }

private:
    juce::PluginDescription description;
    juce::MemoryBlock savedState;
};
} // namespace

PluginChainEngine::PluginChainEngine()
{
    formats.addDefaultFormats();
    virtualMicrophone = VirtualMicrophone::createPlatformBackend();
    graph = std::make_unique<juce::AudioProcessorGraph>();
    // Give the graph its fixed rack layout before any nodes are connected.
    // AudioProcessorPlayer will apply the real device block size later, while
    // this also keeps graph edits valid before a device has been opened.
    graph->setPlayConfigDetails(inputChannelCount, outputChannelCount, activeSampleRate, 256);
    inputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    inputUpmixNode = graph->addNode(
        std::make_unique<ChannelAdapterProcessor>(
            ChannelAdapterProcessor::Direction::monoToStereo));
    outputNode = graph->addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    virtualMicNode = graph->addNode(std::make_unique<VirtualMicrophoneSink>(*virtualMicrophone));
    auto inputMeterProcessor = std::make_unique<MeteringProcessor>(false);
    inputMeter = inputMeterProcessor.get();
    inputMeterNode = graph->addNode(std::move(inputMeterProcessor));
    auto outputMeterProcessor = std::make_unique<MeteringProcessor>(true);
    outputMeter = outputMeterProcessor.get();
    outputMeterNode = graph->addNode(std::move(outputMeterProcessor));
    rebuildConnections();
}

PluginChainEngine::~PluginChainEngine() { shutdownAudio(); }
juce::AudioDeviceManager& PluginChainEngine::deviceManager() noexcept { return devices; }
juce::AudioPluginFormatManager& PluginChainEngine::formatManager() noexcept { return formats; }
juce::KnownPluginList& PluginChainEngine::knownPlugins() noexcept { return plugins; }

juce::String PluginChainEngine::initialiseAudio(const juce::String& preferredInput,
                                                double preferredSampleRate)
{
    auto error = devices.initialise(inputChannelCount, outputChannelCount, nullptr, true, preferredInput);
    if (error.isNotEmpty()) return error;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.getAudioDeviceSetup(setup);
    const auto requested = isUsableSampleRate(preferredSampleRate) ? preferredSampleRate
                                                                   : defaultSampleRate;
    setup.sampleRate = requested;
    setup.inputChannels.clear(); setup.inputChannels.setBit(0);
    setup.outputChannels.clear();
    setup.outputChannels.setRange(0, outputChannelCount, true);
    error = devices.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty() && requested != defaultSampleRate) {
        // A rate remembered from a device that is no longer plugged in must not
        // leave the app silent, so the default is tried before giving up.
        setup.sampleRate = defaultSampleRate;
        error = devices.setAudioDeviceSetup(setup, true);
    }
    if (error.isNotEmpty()) return error;
    auto* audioDevice = devices.getCurrentAudioDevice();
    // The device has the last word on the rate; everything downstream follows it
    // rather than what was asked for.
    activeSampleRate = audioDevice != nullptr ? audioDevice->getCurrentSampleRate()
                                              : setup.sampleRate;
    virtualMicStatus = virtualMicrophone->start(
        activeSampleRate, inputChannelCount,
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

double PluginChainEngine::sampleRate() const noexcept { return activeSampleRate; }

/*
 * Devices advertise everything their driver can be talked into, down to
 * telephony rates a voice chain has no use for and up to rates that only cost
 * CPU here. The picker offers the range in between so a stray entry cannot make
 * the rack unusable.
 */
bool PluginChainEngine::isUsableSampleRate(double rate) noexcept
{
    return rate >= 44100.0 - 0.5 && rate <= 192000.0 + 0.5;
}

juce::Array<double> PluginChainEngine::availableSampleRates() const
{
    juce::Array<double> rates;
    if (auto* device = devices.getCurrentAudioDevice())
        for (const auto rate : device->getAvailableSampleRates())
            if (isUsableSampleRate(rate)) rates.addIfNotAlreadyThere(rate);
    if (rates.isEmpty()) rates.add(activeSampleRate);
    rates.sort();
    return rates;
}

/*
 * Changing the rate closes and reopens the device, which pulls the buffer the
 * driver feed is written from out from under it. The feed is therefore stopped
 * first and started again on the rate the device actually settled on, and a
 * refusal puts the previous rate back rather than leaving the rack closed.
 */
juce::String PluginChainEngine::setSampleRate(double rate)
{
    if (!isUsableSampleRate(rate))
        return "InputRack runs between 44.1 kHz and 192 kHz.";
    if (devices.getCurrentAudioDevice() == nullptr)
        return "No audio device is open.";
    if (std::abs(activeSampleRate - rate) < 0.5) return {};

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    devices.getAudioDeviceSetup(setup);
    const auto previous = setup.sampleRate;
    setup.sampleRate = rate;

    virtualMicrophone->stop();
    auto error = devices.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty()) {
        setup.sampleRate = previous;
        devices.setAudioDeviceSetup(setup, true);
    }

    auto* audioDevice = devices.getCurrentAudioDevice();
    activeSampleRate = audioDevice != nullptr ? audioDevice->getCurrentSampleRate() : previous;
    virtualMicStatus = virtualMicrophone->start(
        activeSampleRate, inputChannelCount,
        audioDevice != nullptr ? audioDevice->getCurrentBufferSizeSamples() : 256);
    return error;
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
    auto hosted = createHostedPlugin(description, error);
    if (!hosted.has_value()) return false;
    chain.add(std::move(*hosted));
    if (rebuildConnections()) return true;

    const auto added = chain.getLast();
    chain.removeLast();
    removeHostedPlugin(added);
    rebuildConnections();
    error = "The plug-in's audio channels could not be connected.";
    return false;
}

std::optional<PluginChainEngine::HostedPlugin> PluginChainEngine::createHostedPlugin(
    const juce::PluginDescription& description, juce::String& error)
{
    auto instance = formats.createPluginInstance(description, activeSampleRate, 256, error);
    if (!instance) return std::nullopt;
    const auto layout = configurePluginLayout(*instance, activeSampleRate);
    if (!layout) {
        error = "The plug-in supports neither mono nor stereo audio processing.";
        return std::nullopt;
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
        if ((layout.inputs == 1 && inputAdapter == nullptr)
            || (layout.outputs == 1 && outputAdapter == nullptr)) {
            if (inputAdapter != nullptr) graph->removeNode(inputAdapter->nodeID);
            if (outputAdapter != nullptr) graph->removeNode(outputAdapter->nodeID);
            graph->removeNode(node->nodeID);
            error = "The plug-in's channel adapters could not be created.";
            return std::nullopt;
        }
        return HostedPlugin{description, node, inputAdapter, outputAdapter,
                            layout.inputs, layout.outputs, false, false};
    }
    error = "The plug-in could not be added to the audio graph.";
    return std::nullopt;
}

std::optional<PluginChainEngine::HostedPlugin> PluginChainEngine::createMissingPlugin(
    const juce::PluginDescription& description)
{
    auto processor = std::make_unique<MissingPluginProcessor>(description);
    processor->setRateAndBufferSizeDetails(activeSampleRate, 256);
    if (auto node = graph->addNode(std::move(processor)))
        return HostedPlugin{description, node, nullptr, nullptr, 2, 2, true, false};
    return std::nullopt;
}

void PluginChainEngine::removeHostedPlugin(const HostedPlugin& item)
{
    if (item.inputAdapter != nullptr) graph->removeNode(item.inputAdapter->nodeID);
    if (item.outputAdapter != nullptr) graph->removeNode(item.outputAdapter->nodeID);
    if (item.node != nullptr) graph->removeNode(item.node->nodeID);
}

void PluginChainEngine::removePlugin(int index)
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return;
    const auto item = chain.getReference(index);
    chain.remove(index);
    removeHostedPlugin(item);
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

void PluginChainEngine::setGloballyBypassed(bool bypassed)
{
    if (globallyBypassed == bypassed) return;
    globallyBypassed = bypassed;
    rebuildConnections();
}

bool PluginChainEngine::isGloballyBypassed() const noexcept { return globallyBypassed; }

float PluginChainEngine::consumeInputPeak(int channel) noexcept
{
    return inputMeter->readAndResetPeak(channel);
}

float PluginChainEngine::consumeOutputPeak(int channel) noexcept
{
    return outputMeter->readAndResetPeak(channel);
}

bool PluginChainEngine::consumeOutputClipped() noexcept
{
    return outputMeter->readAndResetClipped();
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

RoutingReadiness PluginChainEngine::evaluateRouting(const juce::String& inputDeviceName,
                                                     const juce::String& outputDeviceName,
                                                     bool outputEnabled,
                                                     bool signalSeen)
{
    return {inputDeviceName.isNotEmpty(), looksLikeVirtualCable(outputDeviceName),
            outputEnabled, signalSeen};
}

juce::AudioPluginInstance* PluginChainEngine::pluginAt(int index) const
{
    if (!juce::isPositiveAndBelow(index, chain.size())) return nullptr;
    return dynamic_cast<juce::AudioPluginInstance*>(chain.getReference(index).node->getProcessor());
}

bool PluginChainEngine::isPluginMissing(int index) const noexcept
{
    return juce::isPositiveAndBelow(index, chain.size())
        && chain.getReference(index).missing;
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
    error.clear();
    juce::Array<HostedPlugin> candidate;
    juce::StringArray failures;
    juce::StringArray missing;
    for (int i = 0; i < state.size(); ++i) {
        const auto item = state.pluginAt(i);
        juce::PluginDescription description;
        description.name = item.getProperty("name").toString();
        description.manufacturerName = item.getProperty("manufacturer").toString();
        description.fileOrIdentifier = item.getProperty("fileOrIdentifier").toString();
        description.uniqueId = static_cast<int>(item.getProperty("uniqueId"));
        description.deprecatedUid = static_cast<int>(item.getProperty("deprecatedUid"));
        description.pluginFormatName = item.getProperty("format").toString();
        juce::String pluginError;
        auto hosted = createHostedPlugin(description, pluginError);
        if (!hosted.has_value()) {
            missing.add(description.name);
            hosted = createMissingPlugin(description);
            if (!hosted.has_value()) {
                failures.add(description.name + ": a missing plug-in placeholder could not be created");
                continue;
            }
        }
        const auto data = state.pluginState(i);
        if (!data.isEmpty())
            hosted->node->getProcessor()->setStateInformation(
                data.getData(), static_cast<int>(data.getSize()));
        hosted->bypassed = state.isBypassed(i);
        candidate.add(std::move(*hosted));
    }

    if (!failures.isEmpty()) {
        for (const auto& item : candidate) removeHostedPlugin(item);
        error = "The preset was not loaded. The current rack is unchanged:\n- "
            + failures.joinIntoString("\n- ");
        return false;
    }

    // Keep every old node alive until the complete candidate chain has proved
    // that it can be connected. This makes restoration an atomic user action:
    // either the new rack works, or the old rack is reconnected unchanged.
    auto previous = std::move(chain);
    chain = std::move(candidate);
    if (!rebuildConnections()) {
        for (const auto& item : chain) removeHostedPlugin(item);
        chain = std::move(previous);
        rebuildConnections();
        error = "The preset's audio graph could not be connected. The current rack is unchanged.";
        return false;
    }

    for (const auto& item : previous) removeHostedPlugin(item);
    if (!missing.isEmpty())
        error = "Audio will pass through the unavailable plug-in(s):\n- "
            + missing.joinIntoString("\n- ");
    return true;
}

bool PluginChainEngine::rebuildConnections()
{
    for (const auto& connection : graph->getConnections())
        graph->removeConnection(connection);

    bool connected = graph->addConnection(
        {{inputNode->nodeID, 0}, {inputUpmixNode->nodeID, 0}});
    for (int channel = 0; channel < processingChannelCount; ++channel)
        connected = graph->addConnection(
            {{inputUpmixNode->nodeID, channel}, {inputMeterNode->nodeID, channel}}) && connected;
    auto previous = inputMeterNode->nodeID;
    if (!globallyBypassed) {
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
    }
    for (int channel = 0; channel < processingChannelCount; ++channel)
        connected = graph->addConnection(
            {{previous, channel}, {outputMeterNode->nodeID, channel}}) && connected;
    previous = outputMeterNode->nodeID;
    if (monitoringEnabled) {
        for (int channel = 0; channel < outputChannelCount; ++channel)
            connected = graph->addConnection(
                {{previous, channel}, {outputNode->nodeID, channel}}) && connected;
    }
    connected = graph->addConnection({{previous, 0}, {virtualMicNode->nodeID, 0}}) && connected;
    return connected;
}
}
