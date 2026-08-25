#include <vocalchain/VirtualMicrophone.h>

namespace vocalchain {
namespace {
// The interface is wired before the privileged platform implementations so the
// audio engine never needs to depend on PipeWire or WDK headers. This backend
// deliberately reports its state instead of pretending to publish a device.
class UnavailableVirtualMicrophone final : public VirtualMicrophone {
public:
    juce::String start(double, int, int) override
    {
#if JUCE_LINUX
        return "PipeWire virtual-source backend is not built yet";
#elif JUCE_WINDOWS
        return "WaveRT virtual-microphone driver is not installed yet";
#else
        return "This platform is not supported";
#endif
    }
    void stop() override {}
    void push(const float*, int) noexcept override {}
    bool isRunning() const noexcept override { return false; }
};
}

std::unique_ptr<VirtualMicrophone> VirtualMicrophone::createPlatformBackend()
{
    return std::make_unique<UnavailableVirtualMicrophone>();
}
}
