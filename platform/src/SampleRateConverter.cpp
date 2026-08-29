#include <inputrack/SampleRateConverter.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace inputrack {
void SampleRateConverter::prepare(double sourceRate, double targetRate, int maximumInputFrames)
{
    maximumInput = juce::jmax(1, maximumInputFrames);
    ratio = sourceRate > 0.0 && targetRate > 0.0 ? sourceRate / targetRate : 1.0;
    /*
     * A call leaves at most carryMargin plus one ratio's worth of frames behind,
     * so the staging buffer holds a full block on top of that carry and convert()
     * never has to drop input it was handed.
     */
    const auto carry = static_cast<int>(std::ceil(ratio)) + carryMargin + 2;
    staging.assign(static_cast<std::size_t>(maximumInput + carry), 0.0f);
    maximumOutput = isPassThrough()
        ? maximumInput
        : static_cast<int>(std::floor(static_cast<double>(staging.size()) / ratio)) + 1;
    staged = 0;
    reset();
}

void SampleRateConverter::reset() noexcept
{
    staged = 0;
    interpolator.reset();
}

bool SampleRateConverter::isPassThrough() const noexcept
{
    return std::abs(ratio - 1.0) < 1.0e-9;
}

int SampleRateConverter::maximumOutputFrames() const noexcept { return maximumOutput; }

int SampleRateConverter::convert(const float* input, int inputFrames,
                                 float* output, int outputCapacity) noexcept
{
    if (input == nullptr || output == nullptr || inputFrames <= 0 || outputCapacity <= 0)
        return 0;

    if (isPassThrough()) {
        const auto frames = juce::jmin(inputFrames, outputCapacity);
        std::copy(input, input + frames, output);
        return frames;
    }

    const auto taken = juce::jmin(inputFrames, static_cast<int>(staging.size()) - staged);
    std::copy(input, input + taken, staging.data() + static_cast<std::size_t>(staged));
    staged += taken;

    // Holding carryMargin frames back keeps the interpolator inside the staged
    // range whatever its internal sub-sample position happens to be.
    auto produce = static_cast<int>(std::floor((staged - carryMargin) / ratio));
    produce = juce::jlimit(0, outputCapacity, produce);
    if (produce <= 0) return 0;

    const auto used = juce::jmin(staged, interpolator.process(ratio, staging.data(),
                                                              output, produce));
    staged -= used;
    if (staged > 0)
        std::memmove(staging.data(), staging.data() + static_cast<std::size_t>(used),
                     static_cast<std::size_t>(staged) * sizeof(float));
    return produce;
}
}
