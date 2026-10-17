// kernel/src/ACPI/ACPI.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ACPI/ACPI.h>
#include <LineOS/TypeInfo.h>
#include <LineOS/BootInfo.h>

typedef struct PACKED
{
    ACPI_SDT_HEADER Header;
    UINT64          Entries[];
} ACPI_XSDT;

STATIC BOOLEAN ACPISignatureEqual(CONST CHAR8 *A, CONST CHAR8 *B)
{
    return A[0] == B[0] && A[1] == B[1] && A[2] == B[2] && A[3] == B[3];
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

    for (UINT32 I = 0; I < EntryCount; I++)
    {
        ACPI_SDT_HEADER *Header;

        Header = (ACPI_SDT_HEADER *) XSDT->Entries[I];

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
