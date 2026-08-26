#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace inputrack {
class ChannelAdapterProcessor final : public juce::AudioProcessor {
public:
    enum class Direction { monoToStereo, stereoToMono };

    explicit ChannelAdapterProcessor(Direction);

    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

private:
    static BusesProperties busProperties(Direction);
    Direction direction;
};
}
