// kernel/include/memory/memory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct BUDDY_BLOCK
{
    struct BUDDY_BLOCK *Next;
} BUDDY_BLOCK;

typedef struct HEAP_BLOCK
{
    UINTN             Size;
    BOOLEAN           Free;
    struct HEAP_BLOCK *Next;
    struct HEAP_BLOCK *Previous;
} HEAP_BLOCK;

BOOLEAN KMemoryInit(LINEOS_BOOT_INFO *BootInfo);
UINT64  KGetTotalPages(VOID);
VOID   *KMemMove(VOID *destination, CONST VOID *source, UINTN size);
VOID   *KMemCpy(VOID *destination, CONST VOID *source, UINTN size);
VOID   *KMemSet(VOID *destination, UINT8 value, UINTN size);
VOID   *KAllocPages(UINTN PageCount);
VOID   *KAllocPagesBelow(UINTN PageCount, UINT64 limit);
VOID    KMemFreePages(VOID *address, UINTN PageCount);
BOOLEAN KHeapInit(VOID);
VOID   *KAlloc(UINTN size);
VOID    KFree(VOID *address);