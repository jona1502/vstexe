#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace inputrack {
/**
 * Stereo pass-through node that reports a peak-since-last-read level per
 * channel and, when constructed with clip protection enabled, hard-clamps
 * samples beyond full scale before they leave the node.
 */
class MeteringProcessor final : public juce::AudioProcessor {
public:
    static constexpr int meteredChannelCount = 2;

    explicit MeteringProcessor(bool applyClipProtection);

    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    /** Highest absolute sample value seen on this channel since the last call. */
    float readAndResetPeak(int channel) noexcept;
    /** True if a sample beyond full scale was clamped since the last call. */
    bool readAndResetClipped() noexcept;

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
    bool clipProtection;
    std::array<std::atomic<float>, meteredChannelCount> peaks{};
    std::atomic<bool> clipped{false};
};
}
