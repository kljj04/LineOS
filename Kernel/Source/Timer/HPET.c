// kernel/src/timer/HPET.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Timer/HPET.h>
#include <ACPI/ACPI.h>
#include <LineOS/TypeInfo.h>
#include <LineOS/BootInfo.h>

#define HPET_REG_CAPABILITIES  0x000
#define HPET_REG_CONFIGURATION 0x010
#define HPET_REG_MAIN_COUNTER  0x0F0
#define HPET_CONFIG_ENABLE     (1ULL << 0)
#define HPET_FS_PER_SECOND     1000000000000000ULL

STATIC VOLATILE UINT8 *HPETBase = NULL;
STATIC UINT64          HPETFrequency = 0;

STATIC UINT64 HPETRead64(UINT32 Reg)
{
    return *(VOLATILE UINT64 *) (HPETBase + Reg);
}

STATIC VOID HPETWrite64(UINT32 Reg, UINT64 Value)
{
    *(VOLATILE UINT64 *) (HPETBase + Reg) = Value;
}

BOOLEAN HPETInit(LINEOS_BOOT_INFO *BootInfo)
{
    ACPI_HPET *HPETTable;
    UINT64     Capabilities;
    UINT64     Period;
    UINT64     Configuration;

    HPETTable = (ACPI_HPET *) ACPIFindTable(BootInfo, (CONST CHAR8 *) "HPET");

    if (HPETTable == NULL)
    {
        return FALSE;
    }

    if (HPETTable->Address.AddressSpaceID != 0)
    {
        return FALSE;
    }

    if (HPETTable->Address.Address == 0)
    {
        return FALSE;
    }

    HPETBase = (VOLATILE UINT8 *) HPETTable->Address.Address;

    Capabilities = HPETRead64(HPET_REG_CAPABILITIES);
    Period = Capabilities >> 32;

    if (Period == 0)
    {
        HPETBase = NULL;
        return FALSE;
    }

    HPETFrequency = HPET_FS_PER_SECOND / Period;

    Configuration = HPETRead64(HPET_REG_CONFIGURATION);
    Configuration &= ~HPET_CONFIG_ENABLE;
    HPETWrite64(HPET_REG_CONFIGURATION, Configuration);

    HPETWrite64(HPET_REG_MAIN_COUNTER, 0);

    Configuration |= HPET_CONFIG_ENABLE;
    HPETWrite64(HPET_REG_CONFIGURATION, Configuration);

    return TRUE;
}

UINT64 HPETReadCounter(VOID)
{
    if (HPETBase == NULL)
    {
        return 0;
    }

    return HPETRead64(HPET_REG_MAIN_COUNTER);
}

UINT64 HPETGetFrequency(VOID)
{
    return HPETFrequency;
}
