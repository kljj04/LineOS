// acpi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <memory/memory.h>

#define ACPI_SIGNATURE(a, b, c, d) ((UINT32)(a) | ((UINT32)(b) << 8) | ((UINT32)(c) << 16) | ((UINT32)(d) << 24))
#define ACPI_SIGNATURE_HPET ACPI_SIGNATURE('H', 'P', 'E', 'T')

typedef struct
{
    UINT32 Signature;
    UINT32 Length;
    UINT8 Revision;
    UINT8 Checksum;
    CHAR8 OEMId[6];
    CHAR8 OEMTableId[8];
    UINT32 OEMRevision;
    UINT32 CreatorId;
    UINT32 CreatorRevision;
} PACKED ACPI_SDT_HEADER;

typedef struct
{
    ACPI_SDT_HEADER Header;
    UINT32 Entry[];
} PACKED ACPI_RSDT;

typedef struct
{
    ACPI_SDT_HEADER Header;
    UINT64 Entry[];
} PACKED ACPI_XSDT;

typedef struct
{
    UINT8 AddressSpaceId;
    UINT8 RegisterBitWidth;
    UINT8 RegisterBitOffset;
    UINT8 AccessSize;
    UINT64 Address;
} PACKED ACPI_GENERIC_ADDRESS;

typedef struct
{
    ACPI_SDT_HEADER Header;
    UINT32 EventTimerBlockId;
    ACPI_GENERIC_ADDRESS BaseAddress;
    UINT8 HPETNumber;
    UINT16 MinimumTick;
    UINT8 PageProtection;
} PACKED ACPI_HPET;

VOID ACPIInit(VOID);
BOOLEAN ACPIChecksum(VOID* table, UINTN length);
ACPI_SDT_HEADER* ACPIFindTable(UINT32 Signature);
ACPI_HPET* HPETGetTable(VOID);
UINT64 HPETGetBaseAddress(VOID);
BOOLEAN HPETIsAvailable(VOID);
