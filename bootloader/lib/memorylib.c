// memorylib.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include "memorylib.h"

VOID *CopyMem(VOID *Destination, CONST VOID *Source, UINTN Length)
{
    UINT8 *Dst;
    CONST UINT8 *Src;

    Dst = (UINT8*)Destination;
    Src = (CONST UINT8*)Source;

    for(UINTN i = 0; i < Length; i++)
    {
        Dst[i] = Src[i];
    }

    return Destination;
}


VOID *SetMem(VOID *Buffer, UINTN Length, UINT8 Value)
{
    UINT8 *Ptr;

    Ptr = (UINT8*)Buffer;

    for(UINTN i = 0; i < Length; i++)
    {
        Ptr[i] = Value;
    }

    return Buffer;
}