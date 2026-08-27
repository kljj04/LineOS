// memory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

BOOLEAN KMemoryInit(LINEOS_BOOT_INFO *BootInfo);
VOID   *KGetPageBitmap(VOID);
UINT64  KGetPageBitmapSize(VOID);
UINT64  KGetTotalPages(VOID);
UINT8   KGetPageBitmapByte(UINT64 ByteIndex);
VOID   *KMemMove(VOID *destination, CONST VOID *source, UINTN size);
VOID   *KMemCpy(VOID *destination, CONST VOID *source, UINTN size);
VOID   *KMemSet(VOID *destination, UINT8 value, UINTN size);
VOID   *KAllocPages(UINTN pageCount);
VOID    KMemFreePages(VOID *address, UINTN pageCount);
