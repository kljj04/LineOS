// kernel/src/acpi/acpi.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <acpi/acpi.h>
#include <lineos/typeinfo.h>
#include <lineos/bootinfo.h>

typedef struct PACKED
{
    ACPI_SDT_HEADER Header;
    UINT64          Entries[];
} ACPI_XSDT;

STATIC BOOLEAN ACPISignatureEqual(CONST CHAR8 *a, CONST CHAR8 *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

VOID *ACPIFindTable(LINEOS_BOOT_INFO *BootInfo, CONST CHAR8 *Signature)
{
    ACPI_XSDT *XSDT;
    UINT32     EntryCount;

    if (BootInfo == NULL || BootInfo->RSDP == NULL || Signature == NULL)
    {
        return NULL;
    }

    if (BootInfo->RSDP->XSDTAddress == 0)
    {
        return NULL;
    }

    XSDT = (ACPI_XSDT *) BootInfo->RSDP->XSDTAddress;

    if (XSDT->Header.Length < sizeof(ACPI_SDT_HEADER))
    {
        return NULL;
    }

    EntryCount = (XSDT->Header.Length - sizeof(ACPI_SDT_HEADER)) / sizeof(UINT64);

    for (UINT32 i = 0; i < EntryCount; i++)
    {
        ACPI_SDT_HEADER *Header;

        Header = (ACPI_SDT_HEADER *) XSDT->Entries[i];

        if (Header == NULL)
        {
            continue;
        }

        if (ACPISignatureEqual(Header->Signature, Signature))
        {
            return Header;
        }
    }

    return NULL;
}
