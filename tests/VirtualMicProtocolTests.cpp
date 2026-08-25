#include <vocalchain/VirtualMicProtocol.h>
#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main()
{
    using namespace vocalchain::virtualmic;

    expect(isCompatible(ProtocolInfo{}), "default protocol information is compatible");

    ProtocolInfo newerMinor;
    newerMinor.versionMinor = protocolVersionMinor + 1;
    expect(isCompatible(newerMinor), "minor protocol versions remain compatible");

    ProtocolInfo incompatible;
    incompatible.versionMajor = protocolVersionMajor + 1;
    expect(!isCompatible(incompatible), "major protocol versions are rejected");

    AudioPacketHeader packet;
    packet.sequence = 42;
    packet.frameCount = 480;
    packet.payloadBytes = packet.frameCount * bytesPerFrame;
    expect(isValid(packet), "10 ms audio packet is valid");

    packet.payloadBytes -= 1;
    expect(!isValid(packet), "truncated audio payload is rejected");

    packet.payloadBytes = (maximumFramesPerPacket + 1) * bytesPerFrame;
    packet.frameCount = maximumFramesPerPacket + 1;
    expect(!isValid(packet), "oversized audio packet is rejected");

    return failures == 0 ? 0 : 1;
}
