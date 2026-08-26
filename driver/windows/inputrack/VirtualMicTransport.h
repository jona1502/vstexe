#pragma once

#include <portcls.h>
#include "../../../platform/include/inputrack/VirtualMicProtocol.h"

DEFINE_GUIDSTRUCT("6CB95973-265F-4D3B-94D1-7121E579442D", KSPROPSETID_InputRackVirtualMic);
#define KSPROPSETID_InputRackVirtualMic DEFINE_GUIDNAMED(KSPROPSETID_InputRackVirtualMic)

NTSTATUS InputRackVirtualMicInitialize();
void InputRackVirtualMicShutdown();
void InputRackVirtualMicRead(_Out_writes_(frameCount) SHORT* output, _In_ ULONG frameCount);
NTSTATUS PropertyHandler_InputRackVirtualMic(_In_ PPCPROPERTY_REQUEST propertyRequest);
