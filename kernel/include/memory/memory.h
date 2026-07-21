// memory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct
{
    UINT64 UsableBase;
    UINT64 UsableSize;
    UINT64 UsableTotalSize;
    UINT64 ReservedTotalSize;
    UINT64 ACPITotalSize;
    UINT64 MMIOTotalSize;
    UINT64 HighestPhysicalAddress;
    UINTN  DescriptorCount;
} LINEOS_MEMORY_INFO;

typedef struct
{
    UINT64 Base;
    UINT64 PageCount;
    UINT64 FreePageCount;
    UINT64 UsedPageCount;
    UINT64 BitmapSize;
    UINT8 *Bitmap;
} LINEOS_PMM_INFO;

typedef struct
{
    UINT64 Signature;
    UINT8 Checksum;
    CHAR8 OEMId[6];
    UINT8 Revision;
    UINT32 RSDTAddress;
    UINT32 Length;
    UINT64 XSDTAddress;
    UINT8 ExtendedChecksum;
    UINT8 Reserved[3];
} PACKED LINEOS_RSDP;

VOID* MemSet(VOID* destination, UINT8 value, UINTN size);
VOID* MemCopy(VOID* destination, CONST VOID* source, UINTN size);
VOID* MemMove(VOID* destination, CONST VOID* source, UINTN size);
INT32 MemCompare(CONST VOID* first, CONST VOID* second, UINTN size);

VOID MemoryMapInit(LINEOS_BOOT_INFO* BootInfo);
VOID PMMInit(LINEOS_BOOT_INFO* BootInfo);
VOID* PMMAllocPage(VOID);
VOID* PMMAllocPages(UINTN count);
VOID PMMFreePage(VOID* address);
VOID PMMFreePages(VOID* address, UINTN count);
VOID* KMalloc(UINTN size);
VOID* KRealloc(VOID* ptr, UINTN size);
VOID KFree(VOID* ptr);
LINEOS_RSDP* ACPIGetRSDP(VOID);
UINT8 MMIORead8(UINT64 address);
UINT16 MMIORead16(UINT64 address);
UINT32 MMIORead32(UINT64 address);
UINT64 MMIORead64(UINT64 address);
VOID MMIOWrite8(UINT64 address, UINT8 value);
VOID MMIOWrite16(UINT64 address, UINT16 value);
VOID MMIOWrite32(UINT64 address, UINT32 value);
VOID MMIOWrite64(UINT64 address, UINT64 value);
BOOLEAN MemoryTypeIsUsable(UINT32 type);
BOOLEAN MemoryTypeIsMMIO(UINT32 type);
EFI_MEMORY_DESCRIPTOR* MemoryMapGetDescriptor(UINTN index);

extern LINEOS_MEMORY_INFO MemoryInfo;
extern LINEOS_PMM_INFO PMMInfo;
