// UserPrograms/Terminal/Terminal/Source/TerminalAllocator.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <stddef.h>

STATIC UINT8 AllocatorBuffer[TERMINAL_ALLOCATOR_CAPACITY];
UINTN AllocatorUsed;

VOID *TerminalAllocate(UINTN Size, VOID *Context)
{
    UINTN Aligned;
    VOID *Result;

    (VOID) Context;

    if (Size > TERMINAL_ALLOCATOR_CAPACITY - 15U)
    {
        return NULL;
    }

    Aligned = (Size + 15U) & ~(UINTN) 15U;

    if (AllocatorUsed > TERMINAL_ALLOCATOR_CAPACITY - Aligned)
    {
        return NULL;
    }

    Result = &AllocatorBuffer[AllocatorUsed];
    AllocatorUsed += Aligned;

    return Result;
}

VOID TerminalFree(VOID *Pointer, VOID *Context)
{
    (VOID) Pointer;
    (VOID) Context;
}

