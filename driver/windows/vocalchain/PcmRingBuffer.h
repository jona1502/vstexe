#pragma once

#include <cstddef>
#include <cstdint>

namespace vocalchain::driver {

// The kernel driver protects each operation with a spin lock. Keeping the
// storage external lets the driver allocate it once from nonpaged pool.
class PcmRingBuffer final {
public:
    PcmRingBuffer(std::int16_t* storage, std::size_t capacityFrames) noexcept
        : samples(storage), capacity(capacityFrames)
    {
    }

    std::size_t availableFrames() const noexcept { return size; }
    std::uint64_t droppedFrameCount() const noexcept { return droppedFrames; }
    std::uint64_t silentFrameCount() const noexcept { return silentFrames; }

    void clear() noexcept
    {
        readIndex = 0;
        writeIndex = 0;
        size = 0;
    }

    void write(const std::int16_t* input, std::size_t frameCount) noexcept
    {
        if (samples == nullptr || input == nullptr || capacity == 0 || frameCount == 0)
            return;

        if (frameCount >= capacity) {
            droppedFrames += size + (frameCount - capacity);
            input += frameCount - capacity;
            frameCount = capacity;
            readIndex = 0;
            writeIndex = 0;
            size = 0;
        } else if (frameCount > capacity - size) {
            const auto discard = frameCount - (capacity - size);
            readIndex = (readIndex + discard) % capacity;
            size -= discard;
            droppedFrames += discard;
        }

        for (std::size_t i = 0; i < frameCount; ++i) {
            samples[writeIndex] = input[i];
            writeIndex = (writeIndex + 1) % capacity;
        }
        size += frameCount;
    }

    void read(std::int16_t* output, std::size_t frameCount) noexcept
    {
        if (output == nullptr || frameCount == 0)
            return;

        const auto readable = frameCount < size ? frameCount : size;
        for (std::size_t i = 0; i < readable; ++i) {
            output[i] = samples[readIndex];
            readIndex = (readIndex + 1) % capacity;
        }
        size -= readable;

        for (std::size_t i = readable; i < frameCount; ++i)
            output[i] = 0;
        silentFrames += frameCount - readable;
    }

private:
    std::int16_t* samples{};
    std::size_t capacity{};
    std::size_t readIndex{};
    std::size_t writeIndex{};
    std::size_t size{};
    std::uint64_t droppedFrames{};
    std::uint64_t silentFrames{};
};

} // namespace vocalchain::driver
