// kernel/src/timer/hpet.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <timer/hpet.h>
#include <acpi/acpi.h>
#include <lineos/typeinfo.h>
#include <lineos/bootinfo.h>

#define HPET_REG_CAPABILITIES  0x000
#define HPET_REG_CONFIGURATION 0x010
#define HPET_REG_MAIN_COUNTER  0x0F0
#define HPET_CONFIG_ENABLE     (1ULL << 0)
#define HPET_FS_PER_SECOND     1000000000000000ULL

STATIC VOLATILE UINT8 *HPETBase = NULL;
STATIC UINT64          HPETFrequency = 0;

STATIC UINT64 HPETRead64(UINT32 reg)
{
    return *(VOLATILE UINT64 *) (HPETBase + reg);
}

STATIC VOID HPETWrite64(UINT32 reg, UINT64 value)
{
    *(VOLATILE UINT64 *) (HPETBase + reg) = value;
}

BOOLEAN HPETInit(LINEOS_BOOT_INFO *BootInfo)
{
    ACPI_HPET *HPETTable;
    UINT64     capabilities;
    UINT64     period;
    UINT64     configuration;

    HPETTable = (ACPI_HPET *) ACPIFindTable(BootInfo, "HPET");

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

    capabilities = HPETRead64(HPET_REG_CAPABILITIES);
    period = capabilities >> 32;

    if (period == 0)
    {
        HPETBase = NULL;
        return FALSE;
    }

    HPETFrequency = HPET_FS_PER_SECOND / period;

    configuration = HPETRead64(HPET_REG_CONFIGURATION);
    configuration &= ~HPET_CONFIG_ENABLE;
    HPETWrite64(HPET_REG_CONFIGURATION, configuration);

    HPETWrite64(HPET_REG_MAIN_COUNTER, 0);

    configuration |= HPET_CONFIG_ENABLE;
    HPETWrite64(HPET_REG_CONFIGURATION, configuration);

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