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
typedef void               *EFI_MEMORY_DESCRIPTOR;
typedef void               *EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER;
typedef UINT32              EFI_GRAPHICS_PIXEL_FORMAT;
typedef signed short        INT16;
typedef signed int          INT32;
typedef signed long long    INT64;
typedef UINT8               CHAR8;
typedef UINT16              CHAR16;

#define TRUE  ((BOOLEAN)1)
#define FALSE ((BOOLEAN)0)
#define STATIC static
#else
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#endif
