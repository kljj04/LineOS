// Kernel/Include/Memory/PMM.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/BootInfo.h>

typedef struct BUDDY_BLOCK
{
    struct BUDDY_BLOCK *Next;
} BUDDY_BLOCK;

typedef struct HEAP_BLOCK
{
    UINTN              Size;
    BOOLEAN            Free;
    struct HEAP_BLOCK *Next;
    struct HEAP_BLOCK *Previous;
} HEAP_BLOCK;

BOOLEAN KMemoryInit(LINEOS_BOOT_INFO *BootInfo);
UINT64  KGetTotalPages(VOID);
VOID   *KMemMove(VOID *Destination, CONST VOID *Source, UINTN Size);
VOID   *KMemCpy(VOID *Destination, CONST VOID *Source, UINTN Size);
VOID   *KMemSet(VOID *Destination, UINT8 Value, UINTN Size);
VOID   *KAllocPages(UINTN PageCount);
VOID   *KAllocPagesBelow(UINTN PageCount, UINT64 Limit);
VOID    KMemFreePages(VOID *Address, UINTN PageCount);
BOOLEAN KHeapInit(VOID);
BOOLEAN KMemoryPanicLocksAvailable(VOID);
VOID   *KAlloc(UINTN Size);
VOID    KFree(VOID *Address);
VOID    KGetManagedMemory(UINT64 *Managed, UINT64 *Free);
