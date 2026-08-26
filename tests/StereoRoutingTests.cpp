#include <inputrack/ChannelAdapterProcessor.h>
#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool closeTo(float actual, float expected)
{
    return std::abs(actual - expected) < 0.000001f;
}
}

int main()
{
    using Adapter = inputrack::ChannelAdapterProcessor;
    juce::MidiBuffer midi;

    juce::AudioBuffer<float> upmixBuffer(2, 3);
    upmixBuffer.setSample(0, 0, 0.25f);
    upmixBuffer.setSample(0, 1, -0.5f);
    upmixBuffer.setSample(0, 2, 1.0f);
    upmixBuffer.clear(1, 0, 3);
    Adapter upmix(Adapter::Direction::monoToStereo);
    expect(upmix.getBusesLayout().getMainInputChannelSet() == juce::AudioChannelSet::mono()
               && upmix.getBusesLayout().getMainOutputChannelSet()
                      == juce::AudioChannelSet::stereo(),
           "upmix adapter exposes a mono input and stereo output");
    upmix.processBlock(upmixBuffer, midi);
    for (int sample = 0; sample < upmixBuffer.getNumSamples(); ++sample)
        expect(closeTo(upmixBuffer.getSample(0, sample), upmixBuffer.getSample(1, sample)),
               "mono input is copied identically to left and right");

    juce::AudioBuffer<float> downmixBuffer(2, 3);
    downmixBuffer.setSample(0, 0, 1.0f);
    downmixBuffer.setSample(1, 0, -1.0f);
    downmixBuffer.setSample(0, 1, 0.5f);
    downmixBuffer.setSample(1, 1, 0.5f);
    downmixBuffer.setSample(0, 2, -0.25f);
    downmixBuffer.setSample(1, 2, 0.75f);
    Adapter downmix(Adapter::Direction::stereoToMono);
    expect(downmix.getBusesLayout().getMainInputChannelSet() == juce::AudioChannelSet::stereo()
               && downmix.getBusesLayout().getMainOutputChannelSet()
                      == juce::AudioChannelSet::mono(),
           "downmix adapter exposes a stereo input and mono output");
    downmix.processBlock(downmixBuffer, midi);
    expect(closeTo(downmixBuffer.getSample(0, 0), 0.0f),
           "opposite stereo samples cancel in the mono downmix");
    expect(closeTo(downmixBuffer.getSample(0, 1), 0.5f),
           "equal stereo samples retain their level in the mono downmix");
    expect(closeTo(downmixBuffer.getSample(0, 2), 0.25f),
           "different stereo samples are averaged without a gain increase");

    if (failures == 0) std::cout << "StereoRoutingTests passed\n";
    return failures == 0 ? 0 : 1;
}
