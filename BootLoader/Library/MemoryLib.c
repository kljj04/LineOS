// BootLoader/Library/MemoryLib.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <MemoryLib.h>

VOID *CopyMem(VOID *Destination, CONST VOID *Source, UINTN Length)
{
    UINT8       *DestinationBuffer;
    CONST UINT8 *SourceBuffer;

    DestinationBuffer = (UINT8 *) Destination;
    SourceBuffer = (CONST UINT8 *) Source;

    for (UINTN I = 0; I < Length; I++)
    {
        DestinationBuffer[I] = SourceBuffer[I];
    }

    return Destination;
}

VOID *SetMem(VOID *Buffer, UINTN Length, UINT8 Value)
{
    UINT8 *Ptr;

    Ptr = (UINT8 *) Buffer;

    for (UINTN I = 0; I < Length; I++)
    {
        Ptr[I] = Value;
    }

    return Buffer;
}
