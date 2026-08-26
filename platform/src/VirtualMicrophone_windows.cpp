#include <inputrack/VirtualMicrophone.h>
#include <inputrack/VirtualMicProtocol.h>

#define NOMINMAX
#include <Windows.h>
#include <SetupAPI.h>
#include <ks.h>
#include <ksmedia.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace inputrack {
namespace protocol = virtualmic;

namespace {
class DeviceInfoSet final {
public:
    explicit DeviceInfoSet(HDEVINFO value) : handle(value) {}
    ~DeviceInfoSet()
    {
        if (handle != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList(handle);
    }
    operator HDEVINFO() const noexcept { return handle; }

private:
    HDEVINFO handle{INVALID_HANDLE_VALUE};
};

class WindowsVirtualMicrophone final : public VirtualMicrophone, private juce::Thread {
public:
    WindowsVirtualMicrophone()
        : Thread("InputRack virtual microphone writer"),
          fifo(static_cast<int>(protocol::ringCapacityFrames)),
          floatBuffer(protocol::ringCapacityFrames),
          packet(protocol::maximumPacketBytes)
    {
    }

    ~WindowsVirtualMicrophone() override { stop(); }

    juce::String start(double sourceSampleRate, int channels, int) override
    {
        stop();
        if (std::abs(sourceSampleRate - static_cast<double>(protocol::sampleRate)) > 0.5
            || channels != protocol::channelCount)
            return "Virtual microphone requires mono audio at 48 kHz";

        device = findCompatibleFilter();
        if (device == INVALID_HANDLE_VALUE)
            return "InputRack Virtual Mic driver was not found";

        fifo.reset();
        sequence = 0;
        running.store(true, std::memory_order_release);
        startThread(juce::Thread::Priority::high);
        return {};
    }

    void stop() override
    {
        running.store(false, std::memory_order_release);
        signal.signal();
        stopThread(2000);
        if (device != INVALID_HANDLE_VALUE) {
            CloseHandle(device);
            device = INVALID_HANDLE_VALUE;
        }
        fifo.reset();
    }

    void push(const float* monoSamples, int sampleCount) noexcept override
    {
        if (!running.load(std::memory_order_acquire) || monoSamples == nullptr || sampleCount <= 0)
            return;

        const auto toWrite = std::min(sampleCount, fifo.getFreeSpace());
        const auto scope = fifo.write(toWrite);
        for (int i = 0; i < scope.blockSize1; ++i)
            floatBuffer[static_cast<std::size_t>(scope.startIndex1 + i)] = monoSamples[i];
        for (int i = 0; i < scope.blockSize2; ++i)
            floatBuffer[static_cast<std::size_t>(scope.startIndex2 + i)] =
                monoSamples[scope.blockSize1 + i];
        if (toWrite > 0)
            signal.signal();
    }

    bool isRunning() const noexcept override
    {
        return running.load(std::memory_order_acquire);
    }

private:
    static GUID propertySetGuid() noexcept
    {
        return {protocol::propertySetGuidData1, protocol::propertySetGuidData2,
                protocol::propertySetGuidData3,
                {protocol::propertySetGuidData4[0], protocol::propertySetGuidData4[1],
                 protocol::propertySetGuidData4[2], protocol::propertySetGuidData4[3],
                 protocol::propertySetGuidData4[4], protocol::propertySetGuidData4[5],
                 protocol::propertySetGuidData4[6], protocol::propertySetGuidData4[7]}};
    }

    static HANDLE findCompatibleFilter()
    {
        DeviceInfoSet devices(SetupDiGetClassDevsW(
            &KSCATEGORY_AUDIO, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
        if (static_cast<HDEVINFO>(devices) == INVALID_HANDLE_VALUE)
            return INVALID_HANDLE_VALUE;

        for (DWORD index = 0;; ++index) {
            SP_DEVICE_INTERFACE_DATA interfaceData{};
            interfaceData.cbSize = sizeof(interfaceData);
            if (!SetupDiEnumDeviceInterfaces(devices, nullptr, &KSCATEGORY_AUDIO,
                                             index, &interfaceData))
                break;

            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, nullptr, 0,
                                             &requiredSize, nullptr);
            if (requiredSize < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
                continue;

            std::vector<unsigned char> detailStorage(requiredSize);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
                detailStorage.data());
            detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, detail,
                                                  requiredSize, nullptr, nullptr))
                continue;

            auto candidate = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (candidate == INVALID_HANDLE_VALUE)
                continue;

            KSPROPERTY property{propertySetGuid(),
                static_cast<ULONG>(protocol::PropertyId::protocolInfo), KSPROPERTY_TYPE_GET};
            protocol::ProtocolInfo info;
            DWORD returned = 0;
            if (DeviceIoControl(candidate, IOCTL_KS_PROPERTY, &property, sizeof(property),
                                &info, sizeof(info), &returned, nullptr)
                && returned == sizeof(info) && protocol::isCompatible(info))
                return candidate;
            CloseHandle(candidate);
        }
        return INVALID_HANDLE_VALUE;
    }

    bool send(const short* samples, unsigned int frameCount)
    {
        auto* header = reinterpret_cast<protocol::AudioPacketHeader*>(packet.data());
        *header = {};
        header->sequence = sequence++;
        header->frameCount = frameCount;
        header->payloadBytes = frameCount * static_cast<protocol::u32>(protocol::bytesPerFrame);
        std::memcpy(packet.data() + sizeof(*header), samples, header->payloadBytes);

        KSPROPERTY property{propertySetGuid(),
            static_cast<ULONG>(protocol::PropertyId::audioPacket), KSPROPERTY_TYPE_SET};
        DWORD returned = 0;
        return DeviceIoControl(device, IOCTL_KS_PROPERTY, &property, sizeof(property),
                               packet.data(), sizeof(*header) + header->payloadBytes,
                               &returned, nullptr) != FALSE;
    }

    void run() override
    {
        short pcm[protocol::maximumFramesPerPacket]{};
        while (!threadShouldExit()) {
            signal.wait(20);
            while (!threadShouldExit()) {
                const auto count = std::min(
                    static_cast<int>(protocol::maximumFramesPerPacket), fifo.getNumReady());
                if (count == 0)
                    break;

                const auto scope = fifo.read(count);
                int destination = 0;
                const auto convert = [&](int sourceIndex) {
                    const auto sample = std::clamp(floatBuffer[static_cast<std::size_t>(sourceIndex)],
                                                   -1.0f, 1.0f);
                    pcm[destination++] = static_cast<short>(
                        std::lrint(sample * (sample < 0.0f ? 32768.0f : 32767.0f)));
                };
                for (int i = 0; i < scope.blockSize1; ++i)
                    convert(scope.startIndex1 + i);
                for (int i = 0; i < scope.blockSize2; ++i)
                    convert(scope.startIndex2 + i);

                if (!send(pcm, static_cast<unsigned int>(count))) {
                    running.store(false, std::memory_order_release);
                    return;
                }
            }
        }
    }

    HANDLE device{INVALID_HANDLE_VALUE};
    juce::AbstractFifo fifo;
    std::vector<float> floatBuffer;
    std::vector<unsigned char> packet;
    juce::WaitableEvent signal;
    std::atomic<bool> running{};
    protocol::u64 sequence{};
};
} // namespace

std::unique_ptr<VirtualMicrophone> createWindowsVirtualMicrophone()
{
    return std::make_unique<WindowsVirtualMicrophone>();
}
} // namespace inputrack
