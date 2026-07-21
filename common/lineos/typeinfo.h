// typeinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#ifdef LINEOS_KERNEL_BUILD
typedef unsigned char       UINT8;
typedef unsigned short      UINT16;
typedef unsigned int        UINT32;
typedef unsigned long long  UINT64;
typedef UINT64              UINTN;
typedef void                VOID;
typedef UINT8               BOOLEAN;
typedef void               *EFI_HANDLE;
typedef void               *EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER;
typedef UINT32              EFI_GRAPHICS_PIXEL_FORMAT;
typedef signed short        INT16;
typedef signed int          INT32;
typedef signed long long    INT64;
typedef UINT8               CHAR8;
typedef UINT16              CHAR16;

#define EFI_MEMORY_UC 0x0000000000000001ULL
#define EFI_MEMORY_WC 0x0000000000000002ULL
#define EFI_MEMORY_WT 0x0000000000000004ULL
#define EFI_MEMORY_WB 0x0000000000000008ULL
#define EFI_MEMORY_UCE 0x0000000000000010ULL
#define EFI_MEMORY_WP 0x0000000000001000ULL
#define EFI_MEMORY_RP 0x0000000000002000ULL
#define EFI_MEMORY_XP 0x0000000000004000ULL
#define EFI_MEMORY_NV 0x0000000000008000ULL
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000ULL
#define EFI_MEMORY_RO 0x0000000000020000ULL
#define EFI_MEMORY_RUNTIME 0x8000000000000000ULL

typedef enum
{
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
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

#define CONST const
#define NULL ((VOID*)0)
#define TRUE  ((BOOLEAN)1)
#define FALSE ((BOOLEAN)0)
#define STATIC static
#define PACKED __attribute__((packed))
#else
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#endif
