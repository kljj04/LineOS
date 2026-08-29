// bootinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

#define LINEOS_BOOT_MAGIC 0x4C494E454F530001ULL

#ifdef LINEOS_KERNEL_BUILD
typedef void        *EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER;
typedef unsigned int EFI_GRAPHICS_PIXEL_FORMAT;

#define EFI_MEMORY_UC            0x0000000000000001ULL
#define EFI_MEMORY_WC            0x0000000000000002ULL
#define EFI_MEMORY_WT            0x0000000000000004ULL
#define EFI_MEMORY_WB            0x0000000000000008ULL
#define EFI_MEMORY_UCE           0x0000000000000010ULL
#define EFI_MEMORY_WP            0x0000000000001000ULL
#define EFI_MEMORY_RP            0x0000000000002000ULL
#define EFI_MEMORY_XP            0x0000000000004000ULL
#define EFI_MEMORY_NV            0x0000000000008000ULL
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000ULL
#define EFI_MEMORY_RO            0x0000000000020000ULL
#define EFI_MEMORY_RUNTIME       0x8000000000000000ULL

typedef enum
{
    EFI_RESERVED_MEMORY_TYPE,
    EFI_LOADER_CODE,
    EFI_LOADER_DATA,
    EFI_BOOT_SERVICES_CODE,
    EFI_BOOT_SERVICES_DATA,
    EFI_RUNTIME_SERVICES_CODE,
    EFI_RUNTIME_SERVICES_DATA,
    EFI_CONVENTIONAL_MEMORY,
    EFI_UNUSABLE_MEMORY,
    EFI_ACPI_RECLAIM_MEMORY,
    EFI_ACPI_MEMORY_NVS,
    EFI_MEMORY_MAPPED_IO,
    EFI_MEMORY_MAPPED_IO_PORT_SPACE,
    EFI_PAL_CODE,
    EFI_PERSISTENT_MEMORY,
    EFI_MAX_MEMORY_TYPE
} EFI_MEMORY_TYPE;

typedef struct
{
    UINT32 Type;
    UINT32 Pad;
    UINT64 PhysicalStart;
    UINT64 VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;
#endif

typedef struct
{
    UINT64                    FrameBufferBase;
    UINT64                    FrameBufferSize;
    UINT32                    ScreenWidth;
    UINT32                    ScreenHeight;
    UINT32                    PixelsPerScanLine;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
} LINEOS_GOP;

typedef struct
{
    EFI_MEMORY_DESCRIPTOR *MemoryMap;
    UINTN                  MemoryMapSize;
    UINTN                  MemoryMapDescriptorSize;
    UINT32                 MemoryMapDescriptorVersion;
} LINEOS_MEMORY_MAP;

typedef EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER LINEOS_ACPI_RSDP;

typedef struct
{
    UINT64 Base;
    UINT64 Size;
    UINT64 Entry;
} LINEOS_KERNEL_IMAGE;

typedef struct
{
    UINT64              Magic;
    UINT32              Version;
    UINT32              Size;
    LINEOS_GOP         *GOP;
    LINEOS_MEMORY_MAP  *MemoryMap;
    LINEOS_ACPI_RSDP   *RSDP;
    LINEOS_KERNEL_IMAGE Kernel;
} LINEOS_BOOT_INFO;
