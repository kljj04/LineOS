// kernel/include/timer/HPET.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/BootInfo.h>
#include <LineOS/TypeInfo.h>
#include <ACPI/ACPI.h>

typedef struct PACKED
{
    ACPI_SDT_HEADER                Header;
    UINT8                          HardwareRevisionID;
    UINT8                          ComparatorInfo;
    UINT16                         PCIVendorID;
    ACPI_GENERIC_ADDRESS_STRUCTURE Address;
    UINT8                          HPETNumber;
    UINT16                         MinimumTick;
    UINT8                          PageProtection;
} ACPI_HPET;

BOOLEAN HPETInit(LINEOS_BOOT_INFO *BootInfo);
UINT64  HPETReadCounter(VOID);
UINT64  HPETGetFrequency(VOID);