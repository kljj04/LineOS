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

typedef struct PACKED
{
    ACPI_SDT_HEADER Header;
    UINT32          LocalAPICAddress;
    UINT32          Flags;
    UINT8           Entries[];
} ACPI_MADT;

typedef struct PACKED
{
    UINT8  Type;
    UINT8  Length;
    UINT8  ACPIProcessorID;
    UINT8  APICID;
    UINT32 Flags;
} ACPI_MADT_LOCAL_APIC;

typedef struct PACKED ACPI_MADT_ENTRY
{
    UINT8 Type;
    UINT8 Length;
} ACPI_MADT_ENTRY;

typedef struct PACKED ACPI_MADT_LOCAL_X2APIC
{
    UINT8  Type;
    UINT8  Length;
    UINT16 Reserved;
    UINT32 X2APICID;
    UINT32 Flags;
    UINT32 ACPIProcessorUID;
} ACPI_MADT_LOCAL_X2APIC;

VOID *ACPIFindTable(LINEOS_BOOT_INFO *BootInfo, CONST CHAR8 *Signature);
