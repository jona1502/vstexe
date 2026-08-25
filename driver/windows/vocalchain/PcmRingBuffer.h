#pragma once

namespace vocalchain::driver {

using ring_size_type = decltype(sizeof(0));

// The kernel driver protects each operation with a spin lock. Keeping the
// storage external lets the driver allocate it once from nonpaged pool.
class PcmRingBuffer final {
public:
    constexpr PcmRingBuffer() noexcept = default;

    PcmRingBuffer(short* storage, ring_size_type capacityFrames) noexcept
        : samples(storage), capacity(capacityFrames)
    {
    }

    void initialize(short* storage, ring_size_type capacityFrames) noexcept
    {
        samples = storage;
        capacity = capacityFrames;
        readIndex = 0;
        writeIndex = 0;
        size = 0;
        droppedFrames = 0;
        silentFrames = 0;
    }

    ring_size_type availableFrames() const noexcept { return size; }
    unsigned long long droppedFrameCount() const noexcept { return droppedFrames; }
    unsigned long long silentFrameCount() const noexcept { return silentFrames; }

    void clear() noexcept
    {
        readIndex = 0;
        writeIndex = 0;
        size = 0;
    }

    void write(const short* input, ring_size_type frameCount) noexcept
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

        for (ring_size_type i = 0; i < frameCount; ++i) {
            samples[writeIndex] = input[i];
            writeIndex = (writeIndex + 1) % capacity;
        }
        size += frameCount;
    }

    void read(short* output, ring_size_type frameCount) noexcept
    {
        if (output == nullptr || frameCount == 0)
            return;

        const auto readable = frameCount < size ? frameCount : size;
        for (ring_size_type i = 0; i < readable; ++i) {
            output[i] = samples[readIndex];
            readIndex = (readIndex + 1) % capacity;
        }
        size -= readable;

        for (ring_size_type i = readable; i < frameCount; ++i)
            output[i] = 0;
        silentFrames += frameCount - readable;
    }

private:
    short* samples{};
    ring_size_type capacity{};
    ring_size_type readIndex{};
    ring_size_type writeIndex{};
    ring_size_type size{};
    unsigned long long droppedFrames{};
    unsigned long long silentFrames{};
};

} // namespace vocalchain::driver
