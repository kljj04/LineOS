// bootinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

#define LINEOS_BOOT_MAGIC 0x4C494E454F530001ULL

typedef struct
{
    UINT64 FrameBufferBase;
    UINT64 FrameBufferSize;
    UINT32 ScreenWidth;
    UINT32 ScreenHeight;
    UINT32 PixelsPerScanLine;
    EFI_GRAPHICS_PIXEL_FORMAT PixelFormat;
} LINEOS_GOP;

typedef struct
{
    EFI_MEMORY_DESCRIPTOR *MemoryMap;
    UINTN MemoryMapSize;
    UINTN MemoryMapDescriptorSize;
    UINT32 MemoryMapDescriptorVersion;
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
    UINT64 Magic;
    UINT32 Version;
    UINT32 Size;

    LINEOS_GOP *GOP;
    LINEOS_MEMORY_MAP *MemoryMap;
    LINEOS_ACPI_RSDP *RSDP;
    LINEOS_KERNEL_IMAGE Kernel;
} LINEOS_BOOT_INFO;
