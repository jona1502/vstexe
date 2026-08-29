#include <inputrack/SampleRateConverter.h>

#include <cmath>
#include <iostream>
#include <vector>

namespace {
int failures = 0;
void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

constexpr int blockSize = 480;
constexpr int blockCount = 100;

/** Runs a sine through the converter and returns the frames it produced. */
int runFrames(double sourceRate, double targetRate)
{
    inputrack::SampleRateConverter converter;
    converter.prepare(sourceRate, targetRate, blockSize);
    std::vector<float> input(blockSize);
    std::vector<float> output(static_cast<std::size_t>(converter.maximumOutputFrames()));

    int produced = 0;
    for (int block = 0; block < blockCount; ++block) {
        for (int i = 0; i < blockSize; ++i) {
            const auto frame = block * blockSize + i;
            input[static_cast<std::size_t>(i)] = static_cast<float>(
                std::sin(2.0 * 3.14159265358979 * 440.0 * frame / sourceRate));
        }
        produced += converter.convert(input.data(), blockSize, output.data(),
                                      static_cast<int>(output.size()));
    }
    return produced;
}
}

int main()
{
    // The driver only ever accepts its own rate, so a chain running at anything
    // else has to come out the far side at that rate; a frame count that drifts
    // means the virtual microphone runs fast or slow against real time.
    const auto totalInput = blockSize * blockCount;
    for (const auto sourceRate : {44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) {
        const auto expected = static_cast<double>(totalInput) * 48000.0 / sourceRate;
        const auto produced = runFrames(sourceRate, 48000.0);
        // Frames that do not line up with an output frame are carried over, so
        // the tolerance only has to cover one block's worth of carry.
        expect(std::abs(produced - expected) < 4.0,
               "converted frame count tracks the rate ratio");
    }

    // A matching rate must not resample at all: the driver feed is the hot path
    // and 48 kHz is what the great majority of chains run at.
    inputrack::SampleRateConverter identity;
    identity.prepare(48000.0, 48000.0, blockSize);
    expect(identity.isPassThrough(), "a matching rate converts by copying");
    const std::vector<float> ramp{0.25f, -0.5f, 0.75f, -1.0f};
    std::vector<float> copied(ramp.size());
    const auto copiedFrames = identity.convert(ramp.data(), static_cast<int>(ramp.size()),
                                               copied.data(), static_cast<int>(copied.size()));
    expect(copiedFrames == static_cast<int>(ramp.size()), "pass-through keeps every frame");
    expect(copied == ramp, "pass-through leaves the samples untouched");

    inputrack::SampleRateConverter resampling;
    resampling.prepare(44100.0, 48000.0, blockSize);
    expect(!resampling.isPassThrough(), "a differing rate is resampled");
    expect(resampling.maximumOutputFrames() > blockSize,
           "upsampling reports room for more frames than it is given");

    // convert() runs on the audio thread and writes straight into the caller's
    // buffer, so it must respect a short one instead of running past its end.
    std::vector<float> input(blockSize, 0.5f);
    std::vector<float> tiny(8, 0.0f);
    const auto clamped = resampling.convert(input.data(), blockSize, tiny.data(),
                                            static_cast<int>(tiny.size()));
    expect(clamped <= static_cast<int>(tiny.size()),
           "a short output buffer caps the frames written");

    // Nothing may be written for input the converter was never handed.
    expect(resampling.convert(nullptr, blockSize, tiny.data(), 8) == 0,
           "a null input produces no frames");
    expect(resampling.convert(input.data(), 0, tiny.data(), 8) == 0,
           "an empty block produces no frames");

    if (failures == 0) std::cout << "SampleRateConverterTests passed\n";
    return failures == 0 ? 0 : 1;
}
