#include <inputrack/ChannelAdapterProcessor.h>

namespace inputrack {
ChannelAdapterProcessor::ChannelAdapterProcessor(Direction directionToUse)
    : AudioProcessor(busProperties(directionToUse)), direction(directionToUse)
{
}

void ChannelAdapterProcessor::prepareToPlay(double, int) {}
void ChannelAdapterProcessor::releaseResources() {}

bool ChannelAdapterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    if (direction == Direction::monoToStereo)
        return input == juce::AudioChannelSet::mono()
            && output == juce::AudioChannelSet::stereo();
    return input == juce::AudioChannelSet::stereo()
        && output == juce::AudioChannelSet::mono();
}

void ChannelAdapterProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    if (buffer.getNumChannels() < 2) return;

    if (direction == Direction::monoToStereo) {
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
        return;
    }

    auto* mono = buffer.getWritePointer(0);
    const auto* right = buffer.getReadPointer(1);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        mono[sample] = 0.5f * (mono[sample] + right[sample]);
}

juce::AudioProcessorEditor* ChannelAdapterProcessor::createEditor() { return nullptr; }
bool ChannelAdapterProcessor::hasEditor() const { return false; }
const juce::String ChannelAdapterProcessor::getName() const
{
    return direction == Direction::monoToStereo ? "Mono to stereo" : "Stereo to mono";
}
bool ChannelAdapterProcessor::acceptsMidi() const { return false; }
bool ChannelAdapterProcessor::producesMidi() const { return false; }
bool ChannelAdapterProcessor::isMidiEffect() const { return false; }
double ChannelAdapterProcessor::getTailLengthSeconds() const { return 0.0; }
int ChannelAdapterProcessor::getNumPrograms() { return 1; }
int ChannelAdapterProcessor::getCurrentProgram() { return 0; }
void ChannelAdapterProcessor::setCurrentProgram(int) {}
const juce::String ChannelAdapterProcessor::getProgramName(int) { return {}; }
void ChannelAdapterProcessor::changeProgramName(int, const juce::String&) {}
void ChannelAdapterProcessor::getStateInformation(juce::MemoryBlock&) {}
void ChannelAdapterProcessor::setStateInformation(const void*, int) {}

juce::AudioProcessor::BusesProperties
ChannelAdapterProcessor::busProperties(Direction direction)
{
    if (direction == Direction::monoToStereo)
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::mono(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    return BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::mono(), true);
}
}
