// common/lineos/acpi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

typedef struct PACKED ACPI_RSDP
{
    CHAR8  Signature[8];
    UINT8  Checksum;
    CHAR8  OEMID[6];
    UINT8  Revision;
    UINT32 RSDTAddress;
    UINT32 Length;
    UINT64 XSDTAddress;
    UINT8  ExtendedChecksum;
    UINT8  Reserved[3];
} ACPI_RSDP;
