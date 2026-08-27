#include <cmath>
#include <inputrack/MeteringProcessor.h>

namespace inputrack {
MeteringProcessor::MeteringProcessor(bool applyClipProtection)
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      clipProtection(applyClipProtection)
{
}

void MeteringProcessor::prepareToPlay(double, int) {}
void MeteringProcessor::releaseResources() {}

bool MeteringProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MeteringProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    for (int channel = 0; channel < buffer.getNumChannels() && channel < meteredChannelCount; ++channel) {
        auto* data = buffer.getWritePointer(channel);
        float blockPeak = 0.0f;
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
            auto value = data[sample];
            if (clipProtection && (value > 1.0f || value < -1.0f)) {
                value = juce::jlimit(-1.0f, 1.0f, value);
                data[sample] = value;
                clipped.store(true, std::memory_order_relaxed);
            }
            blockPeak = juce::jmax(blockPeak, std::abs(value));
        }
        // Lock-free running max: the UI thread drains it with readAndResetPeak,
        // so a stale read at worst under-reports a peak that arrived mid-block.
        auto current = peaks[static_cast<size_t>(channel)].load(std::memory_order_relaxed);
        while (blockPeak > current
               && !peaks[static_cast<size_t>(channel)].compare_exchange_weak(
                   current, blockPeak, std::memory_order_relaxed)) {
        }
    }
}

float MeteringProcessor::readAndResetPeak(int channel) noexcept
{
    if (!juce::isPositiveAndBelow(channel, meteredChannelCount)) return 0.0f;
    return peaks[static_cast<size_t>(channel)].exchange(0.0f, std::memory_order_relaxed);
}

bool MeteringProcessor::readAndResetClipped() noexcept
{
    return clipped.exchange(false, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* MeteringProcessor::createEditor() { return nullptr; }
bool MeteringProcessor::hasEditor() const { return false; }
const juce::String MeteringProcessor::getName() const
{
    return clipProtection ? "Output meter" : "Input meter";
}
bool MeteringProcessor::acceptsMidi() const { return false; }
bool MeteringProcessor::producesMidi() const { return false; }
bool MeteringProcessor::isMidiEffect() const { return false; }
double MeteringProcessor::getTailLengthSeconds() const { return 0.0; }
int MeteringProcessor::getNumPrograms() { return 1; }
int MeteringProcessor::getCurrentProgram() { return 0; }
void MeteringProcessor::setCurrentProgram(int) {}
const juce::String MeteringProcessor::getProgramName(int) { return {}; }
void MeteringProcessor::changeProgramName(int, const juce::String&) {}
void MeteringProcessor::getStateInformation(juce::MemoryBlock&) {}
void MeteringProcessor::setStateInformation(const void*, int) {}
}
