#pragma once

#include <portcls.h>
#include "../../../platform/include/vocalchain/VirtualMicProtocol.h"

DEFINE_GUIDSTRUCT("6CB95973-265F-4D3B-94D1-7121E579442D", KSPROPSETID_VocalChainVirtualMic);
#define KSPROPSETID_VocalChainVirtualMic DEFINE_GUIDNAMED(KSPROPSETID_VocalChainVirtualMic)

NTSTATUS VocalChainVirtualMicInitialize();
void VocalChainVirtualMicShutdown();
void VocalChainVirtualMicRead(_Out_writes_(frameCount) SHORT* output, _In_ ULONG frameCount);
NTSTATUS PropertyHandler_VocalChainVirtualMic(_In_ PPCPROPERTY_REQUEST propertyRequest);
