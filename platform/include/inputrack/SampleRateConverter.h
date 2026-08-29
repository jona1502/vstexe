#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace inputrack {
/*
 * The virtual-microphone driver publishes one fixed rate, so a chain running at
 * anything else has to be converted on the way out. Everything is sized in
 * prepare() because convert() runs on the audio thread and must not allocate.
 */
class SampleRateConverter final {
public:
    /** Sizes the buffers. maximumInputFrames caps one convert() call. */
    void prepare(double sourceRate, double targetRate, int maximumInputFrames);
    /** Drops the interpolator history after a break in the input stream. */
    void reset() noexcept;
    /** True when the rates match, so convert() is a plain copy. */
    bool isPassThrough() const noexcept;
    /** Frames one convert() call can produce, i.e. the output size to provide. */
    int maximumOutputFrames() const noexcept;
    /**
        Converts one block, carrying the frames that did not line up with an
        output frame over to the next call.

        @returns the number of frames written to output.
    */
    int convert(const float* input, int inputFrames, float* output, int outputCapacity) noexcept;

private:
    /*
     * The interpolator can consume one frame more than the ratio suggests, so
     * the tail of the staged input is held back rather than read past.
     */
    static constexpr int carryMargin = 2;

    double ratio{1.0};
    int maximumInput{};
    int maximumOutput{};
    int staged{};
    std::vector<float> staging;
    juce::LagrangeInterpolator interpolator;
};
}
