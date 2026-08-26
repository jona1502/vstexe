#include <inputrack/VirtualMicProtocol.h>
#include "PcmRingBuffer.h"
#include <array>
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
    std::array<std::int16_t, 5> storage{};
    inputrack::driver::PcmRingBuffer ring;
    ring.initialize(storage.data(), storage.size());

    const std::array<std::int16_t, 3> first{1, 2, 3};
    ring.write(first.data(), first.size());
    expect(ring.availableFrames() == 3, "write makes frames available");

    std::array<std::int16_t, 2> firstRead{};
    ring.read(firstRead.data(), firstRead.size());
    expect(firstRead == std::array<std::int16_t, 2>{1, 2}, "read preserves order");

    const std::array<std::int16_t, 5> wrapped{4, 5, 6, 7, 8};
    ring.write(wrapped.data(), wrapped.size());
    expect(ring.droppedFrameCount() == 1, "overflow drops the oldest frame");

    std::array<std::int16_t, 7> output{};
    ring.read(output.data(), output.size());
    expect(output == std::array<std::int16_t, 7>{4, 5, 6, 7, 8, 0, 0},
           "underrun appends silence");
    expect(ring.silentFrameCount() == 2, "silent underrun frames are counted");

    const std::array<std::int16_t, 7> oversized{10, 11, 12, 13, 14, 15, 16};
    ring.write(oversized.data(), oversized.size());
    std::array<std::int16_t, 5> newest{};
    ring.read(newest.data(), newest.size());
    expect(newest == std::array<std::int16_t, 5>{12, 13, 14, 15, 16},
           "oversized writes retain the newest capacity-sized suffix");

    return failures == 0 ? 0 : 1;
}
