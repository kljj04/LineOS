// memorylib.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <memorylib.h>

VOID *CopyMem(VOID *destination, CONST VOID *source, UINTN length)
{
    UINT8 *DestinationBuffer;
    CONST UINT8 *SourceBuffer;

    DestinationBuffer = (UINT8 *) destination;
    SourceBuffer = (CONST UINT8 *) source;

    for (UINTN i = 0; i < length; i++)
    {
        DestinationBuffer[i] = SourceBuffer[i];
    }

    return destination;
}


VOID *SetMem(VOID *buffer, UINTN length, UINT8 value)
{
    UINT8 *ptr;

    ptr = (UINT8 *) buffer;

    for (UINTN i = 0; i < length; i++)
    {
        ptr[i] = value;
    }

    return buffer;
}
