// kernel/include/acpi/acpi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/acpi.h>
#include <lineos/typeinfo.h>
#include <lineos/bootinfo.h>

typedef struct PACKED
{
    CHAR8  Signature[4];
    UINT32 Length;
    UINT8  Revision;
    UINT8  Checksum;
    CHAR8  OEMID[6];
    CHAR8  OEMTableID[8];
    UINT32 OEMRevision;
    UINT32 CreatorID;
    UINT32 CreatorRevision;
} ACPI_SDT_HEADER;

typedef struct PACKED
{
    UINT8  AddressSpaceID;
    UINT8  RegisterBitWidth;
    UINT8  RegisterBitOffset;
    UINT8  AccessSize;
    UINT64 Address;
} ACPI_GENERIC_ADDRESS_STRUCTURE;

VOID *ACPIFindTable(LINEOS_BOOT_INFO *BootInfo, CONST CHAR8 *Signature);
