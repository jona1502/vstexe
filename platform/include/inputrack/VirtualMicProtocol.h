#pragma once

namespace inputrack::virtualmic {

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using size_type = decltype(sizeof(0));

static_assert(sizeof(u8) == 1 && sizeof(u16) == 2 && sizeof(u32) == 4 && sizeof(u64) == 8);

inline constexpr u32 protocolMagic = 0x56434D49; // "VCMI"
inline constexpr u16 protocolVersionMajor = 1;
inline constexpr u16 protocolVersionMinor = 0;
inline constexpr u32 sampleRate = 48000;
inline constexpr u16 channelCount = 1;
inline constexpr u16 bitsPerSample = 16;
inline constexpr u32 maximumFramesPerPacket = 960;
inline constexpr size_type ringCapacityFrames = sampleRate * 2;

// {6CB95973-265F-4D3B-94D1-7121E579442D}
inline constexpr u32 propertySetGuidData1 = 0x6cb95973;
inline constexpr u16 propertySetGuidData2 = 0x265f;
inline constexpr u16 propertySetGuidData3 = 0x4d3b;
inline constexpr u8 propertySetGuidData4[8] = {
    0x94, 0xd1, 0x71, 0x21, 0xe5, 0x79, 0x44, 0x2d
};

enum class PropertyId : u32 {
    protocolInfo = 1,
    audioPacket = 2,
    transportStatus = 3
};

enum class SampleFormat : u16 {
    signedPcm16LittleEndian = 1
};

#pragma pack(push, 1)
struct ProtocolInfo {
    u32 magic{protocolMagic};
    u16 versionMajor{protocolVersionMajor};
    u16 versionMinor{protocolVersionMinor};
    u32 structureSize{sizeof(ProtocolInfo)};
    u32 sampleRateHz{sampleRate};
    u16 channels{channelCount};
    u16 bitsPerSample{inputrack::virtualmic::bitsPerSample};
    SampleFormat sampleFormat{SampleFormat::signedPcm16LittleEndian};
    u16 reserved{};
    u32 maximumPacketFrames{maximumFramesPerPacket};
};

struct AudioPacketHeader {
    u32 magic{protocolMagic};
    u16 versionMajor{protocolVersionMajor};
    u16 headerSize{sizeof(AudioPacketHeader)};
    u64 sequence{};
    u32 frameCount{};
    u32 payloadBytes{};
};

struct TransportStatus {
    u32 structureSize{sizeof(TransportStatus)};
    u32 bufferedFrames{};
    u64 acceptedPackets{};
    u64 rejectedPackets{};
    u64 droppedFrames{};
    u64 silentFrames{};
    u64 lastSequence{};
};
#pragma pack(pop)

inline constexpr size_type bytesPerFrame = channelCount * (bitsPerSample / 8u);
inline constexpr size_type maximumPayloadBytes = maximumFramesPerPacket * bytesPerFrame;
inline constexpr size_type maximumPacketBytes = sizeof(AudioPacketHeader) + maximumPayloadBytes;

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

static_assert(sizeof(ProtocolInfo) == 28);
static_assert(sizeof(AudioPacketHeader) == 24);
static_assert(sizeof(TransportStatus) == 48);

} // namespace inputrack::virtualmic
