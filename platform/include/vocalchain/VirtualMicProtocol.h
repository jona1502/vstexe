#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace vocalchain::virtualmic {

inline constexpr std::uint32_t protocolMagic = 0x56434D49; // "VCMI"
inline constexpr std::uint16_t protocolVersionMajor = 1;
inline constexpr std::uint16_t protocolVersionMinor = 0;
inline constexpr std::uint32_t sampleRate = 48000;
inline constexpr std::uint16_t channelCount = 1;
inline constexpr std::uint16_t bitsPerSample = 16;
inline constexpr std::uint32_t maximumFramesPerPacket = 960;
inline constexpr std::size_t ringCapacityFrames = sampleRate * 2;

enum class SampleFormat : std::uint16_t {
    signedPcm16LittleEndian = 1
};

#pragma pack(push, 1)
struct ProtocolInfo {
    std::uint32_t magic{protocolMagic};
    std::uint16_t versionMajor{protocolVersionMajor};
    std::uint16_t versionMinor{protocolVersionMinor};
    std::uint32_t structureSize{sizeof(ProtocolInfo)};
    std::uint32_t sampleRateHz{sampleRate};
    std::uint16_t channels{channelCount};
    std::uint16_t bitsPerSample{vocalchain::virtualmic::bitsPerSample};
    SampleFormat sampleFormat{SampleFormat::signedPcm16LittleEndian};
    std::uint16_t reserved{};
    std::uint32_t maximumPacketFrames{maximumFramesPerPacket};
};

struct AudioPacketHeader {
    std::uint32_t magic{protocolMagic};
    std::uint16_t versionMajor{protocolVersionMajor};
    std::uint16_t headerSize{sizeof(AudioPacketHeader)};
    std::uint64_t sequence{};
    std::uint32_t frameCount{};
    std::uint32_t payloadBytes{};
};
#pragma pack(pop)

inline constexpr std::size_t bytesPerFrame = channelCount * (bitsPerSample / 8u);
inline constexpr std::size_t maximumPayloadBytes = maximumFramesPerPacket * bytesPerFrame;
inline constexpr std::size_t maximumPacketBytes = sizeof(AudioPacketHeader) + maximumPayloadBytes;

constexpr bool isCompatible(const ProtocolInfo& info) noexcept
{
    return info.magic == protocolMagic
        && info.versionMajor == protocolVersionMajor
        && info.structureSize >= sizeof(ProtocolInfo)
        && info.sampleRateHz == sampleRate
        && info.channels == channelCount
        && info.bitsPerSample == bitsPerSample
        && info.sampleFormat == SampleFormat::signedPcm16LittleEndian;
}

constexpr bool isValid(const AudioPacketHeader& header) noexcept
{
    return header.magic == protocolMagic
        && header.versionMajor == protocolVersionMajor
        && header.headerSize == sizeof(AudioPacketHeader)
        && header.frameCount <= maximumFramesPerPacket
        && header.payloadBytes == header.frameCount * bytesPerFrame;
}

static_assert(std::is_trivially_copyable_v<ProtocolInfo>);
static_assert(std::is_trivially_copyable_v<AudioPacketHeader>);
static_assert(sizeof(ProtocolInfo) == 28);
static_assert(sizeof(AudioPacketHeader) == 24);

} // namespace vocalchain::virtualmic
