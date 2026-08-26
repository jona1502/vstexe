#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

namespace inputrack {
class VirtualMicrophone {
public:
    virtual ~VirtualMicrophone() = default;
    virtual juce::String start(double sampleRate, int channels, int maximumBlockSize) = 0;
    virtual void stop() = 0;
    virtual void push(const float* monoSamples, int sampleCount) noexcept = 0;
    virtual bool isRunning() const noexcept = 0;
    static std::unique_ptr<VirtualMicrophone> createPlatformBackend();
};
}
