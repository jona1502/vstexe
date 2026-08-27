#include <cmath>
#include <inputrack/MeteringProcessor.h>
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
    using Meter = inputrack::MeteringProcessor;
    juce::MidiBuffer midi;

    {
        Meter meter(false);
        juce::AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add(juce::AudioChannelSet::stereo());
        stereo.outputBuses.add(juce::AudioChannelSet::stereo());
        expect(meter.isBusesLayoutSupported(stereo), "a stereo bus layout is accepted");

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add(juce::AudioChannelSet::mono());
        mono.outputBuses.add(juce::AudioChannelSet::mono());
        expect(!meter.isBusesLayoutSupported(mono), "a mono bus layout is rejected");
    }

    {
        // Without clip protection, over-scale samples pass through untouched
        // and never latch the clip flag: this is the input meter's job, and
        // clamping the microphone signal before the chain would be wrong.
        Meter meter(false);
        juce::AudioBuffer<float> buffer(2, 4);
        buffer.clear();
        buffer.setSample(0, 0, 0.5f);
        buffer.setSample(0, 1, -1.5f);
        buffer.setSample(1, 0, 0.25f);
        buffer.setSample(1, 1, -0.25f);
        meter.processBlock(buffer, midi);
        expect(closeTo(buffer.getSample(0, 1), -1.5f), "an over-scale sample is left untouched");
        expect(!meter.readAndResetClipped(), "the clip flag never latches without clip protection");
        expect(closeTo(meter.readAndResetPeak(0), 1.5f), "channel 0 reports its true peak magnitude");
        expect(closeTo(meter.readAndResetPeak(1), 0.25f), "channel 1 is tracked independently");
        expect(closeTo(meter.readAndResetPeak(0), 0.0f), "reading a peak resets it for the next block");
    }

    {
        // With clip protection, an over-scale sample is hard-clamped in place
        // and the clip flag latches until it is explicitly consumed.
        Meter meter(true);
        juce::AudioBuffer<float> buffer(2, 2);
        buffer.setSample(0, 0, 1.2f);
        buffer.setSample(0, 1, -0.4f);
        buffer.setSample(1, 0, 0.1f);
        buffer.setSample(1, 1, -1.1f);
        meter.processBlock(buffer, midi);
        expect(closeTo(buffer.getSample(0, 0), 1.0f), "a positive over is clamped to full scale");
        expect(closeTo(buffer.getSample(1, 1), -1.0f), "a negative over is clamped to full scale");
        expect(meter.readAndResetClipped(), "clamping an over-scale sample latches the clip flag");
        expect(!meter.readAndResetClipped(), "the clip flag clears once consumed");

        juce::AudioBuffer<float> quiet(2, 1);
        quiet.setSample(0, 0, 0.1f);
        quiet.setSample(1, 0, 0.1f);
        meter.processBlock(quiet, midi);
        expect(!meter.readAndResetClipped(), "a block with no overs does not latch the clip flag");
    }

    if (failures == 0) std::cout << "MeteringProcessorTests passed\n";
    return failures == 0 ? 0 : 1;
}
