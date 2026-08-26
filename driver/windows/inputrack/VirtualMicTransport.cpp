#include <sysvad.h>
#include "VirtualMicTransport.h"
#include "PcmRingBuffer.h"

namespace protocol = inputrack::virtualmic;

namespace {
KSPIN_LOCK transportLock;
inputrack::driver::PcmRingBuffer ring;
SHORT ringStorage[protocol::ringCapacityFrames];
ULONGLONG acceptedPackets = 0;
ULONGLONG rejectedPackets = 0;
ULONGLONG lastSequence = 0;
BOOLEAN initialized = FALSE;

NTSTATUS copyResult(PPCPROPERTY_REQUEST request, const void* source, ULONG size)
{
    if (request->ValueSize == 0) {
        request->ValueSize = size;
        return STATUS_BUFFER_OVERFLOW;
    }
    if (request->Value == nullptr || request->ValueSize < size)
        return STATUS_BUFFER_TOO_SMALL;
    RtlCopyMemory(request->Value, source, size);
    request->ValueSize = size;
    return STATUS_SUCCESS;
}

NTSTATUS pushPacket(PPCPROPERTY_REQUEST request)
{
    if (!initialized || request->Value == nullptr
        || request->ValueSize < sizeof(protocol::AudioPacketHeader)) {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&rejectedPackets));
        return STATUS_INVALID_BUFFER_SIZE;
    }
    const auto* header = static_cast<const protocol::AudioPacketHeader*>(request->Value);
    if (!protocol::isValid(*header)
        || request->ValueSize != header->headerSize + header->payloadBytes) {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&rejectedPackets));
        return STATUS_INVALID_PARAMETER;
    }
    const auto* samples = reinterpret_cast<const SHORT*>(
        static_cast<const UCHAR*>(request->Value) + header->headerSize);
    KIRQL previousIrql;
    KeAcquireSpinLock(&transportLock, &previousIrql);
    ring.write(samples, header->frameCount);
    ++acceptedPackets;
    lastSequence = header->sequence;
    KeReleaseSpinLock(&transportLock, previousIrql);
    return STATUS_SUCCESS;
}

NTSTATUS getStatus(PPCPROPERTY_REQUEST request)
{
    protocol::TransportStatus status;
    KIRQL previousIrql;
    KeAcquireSpinLock(&transportLock, &previousIrql);
    status.bufferedFrames = static_cast<ULONG>(ring.availableFrames());
    status.acceptedPackets = acceptedPackets;
    status.rejectedPackets = rejectedPackets;
    status.droppedFrames = ring.droppedFrameCount();
    status.silentFrames = ring.silentFrameCount();
    status.lastSequence = lastSequence;
    KeReleaseSpinLock(&transportLock, previousIrql);
    return copyResult(request, &status, sizeof(status));
}
} // namespace

NTSTATUS InputRackVirtualMicInitialize()
{
    KeInitializeSpinLock(&transportLock);
    ring.initialize(ringStorage, protocol::ringCapacityFrames);
    acceptedPackets = 0;
    rejectedPackets = 0;
    lastSequence = 0;
    initialized = TRUE;
    return STATUS_SUCCESS;
}

void InputRackVirtualMicShutdown() { initialized = FALSE; }

void InputRackVirtualMicRead(SHORT* output, ULONG frameCount)
{
    if (output == nullptr || frameCount == 0)
        return;
    if (!initialized) {
        RtlZeroMemory(output, frameCount * sizeof(SHORT));
        return;
    }
    KIRQL previousIrql;
    KeAcquireSpinLock(&transportLock, &previousIrql);
    ring.read(output, frameCount);
    KeReleaseSpinLock(&transportLock, previousIrql);
}

NTSTATUS PropertyHandler_InputRackVirtualMic(PPCPROPERTY_REQUEST request)
{
    if (request == nullptr || request->PropertyItem == nullptr)
        return STATUS_INVALID_PARAMETER;
    const auto propertyId = static_cast<protocol::PropertyId>(request->PropertyItem->Id);
    ULONG supportedVerb = KSPROPERTY_TYPE_BASICSUPPORT;
    supportedVerb |= propertyId == protocol::PropertyId::audioPacket
        ? KSPROPERTY_TYPE_SET : KSPROPERTY_TYPE_GET;
    if (request->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        return PropertyHandler_BasicSupport(request, supportedVerb, VT_ILLEGAL);
    switch (propertyId) {
        case protocol::PropertyId::protocolInfo: {
            if (!(request->Verb & KSPROPERTY_TYPE_GET))
                return STATUS_INVALID_DEVICE_REQUEST;
            const protocol::ProtocolInfo info;
            return copyResult(request, &info, sizeof(info));
        }
        case protocol::PropertyId::audioPacket:
            return (request->Verb & KSPROPERTY_TYPE_SET)
                ? pushPacket(request) : STATUS_INVALID_DEVICE_REQUEST;
        case protocol::PropertyId::transportStatus:
            return (request->Verb & KSPROPERTY_TYPE_GET)
                ? getStatus(request) : STATUS_INVALID_DEVICE_REQUEST;
        default:
            return STATUS_NOT_SUPPORTED;
    }
}
