// acpi.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <acpi/acpi.h>

STATIC LINEOS_RSDP* RSDP = NULL;
STATIC ACPI_XSDT* XSDT = NULL;
STATIC ACPI_RSDT* RSDT = NULL;
STATIC ACPI_HPET* HPET = NULL;

BOOLEAN ACPIChecksum(VOID* table, UINTN length)
{
    UINT8 sum = 0;
    UINT8* data = (UINT8*)table;

    if (table == NULL || length == 0)
    {
        return FALSE;
    }

    for (UINTN index = 0; index < length; index++)
    {
        sum += data[index];
    }

    return sum == 0;
}

VOID ACPIInit(VOID)
{
    RSDP = ACPIGetRSDP();
    XSDT = NULL;
    RSDT = NULL;
    HPET = NULL;

    if (RSDP == NULL || RSDP->Signature != 0x2052545020445352ULL)
    {
        return;
    }

    if (!ACPIChecksum(RSDP, RSDP->Revision >= 2 ? RSDP->Length : 20))
    {
        return;
    }

    if (RSDP->Revision >= 2 && RSDP->XSDTAddress >= 0x1000)
    {
        XSDT = (ACPI_XSDT*)RSDP->XSDTAddress;

        if (XSDT->Header.Length < sizeof(ACPI_SDT_HEADER) || !ACPIChecksum(XSDT, XSDT->Header.Length))
        {
            XSDT = NULL;
        }
    }

    if (RSDP->RSDTAddress >= 0x1000)
    {
        RSDT = (ACPI_RSDT*)(UINT64)RSDP->RSDTAddress;

        if (RSDT->Header.Length < sizeof(ACPI_SDT_HEADER) || !ACPIChecksum(RSDT, RSDT->Header.Length))
        {
            RSDT = NULL;
        }
    }

    HPET = (ACPI_HPET*)ACPIFindTable(ACPI_SIGNATURE_HPET);
}

ACPI_SDT_HEADER* ACPIFindTable(UINT32 Signature)
{
    if (XSDT != NULL)
    {
        UINTN EntryCount = (XSDT->Header.Length - sizeof(ACPI_SDT_HEADER)) / sizeof(UINT64);

        for (UINTN index = 0; index < EntryCount; index++)
        {
            ACPI_SDT_HEADER* Header = (ACPI_SDT_HEADER*)XSDT->Entry[index];

            if (Header != NULL && Header->Length >= sizeof(ACPI_SDT_HEADER) && Header->Signature == Signature &&
                ACPIChecksum(Header, Header->Length))
            {
                return Header;
            }
        }
    }

    if (RSDT != NULL)
    {
        UINTN EntryCount = (RSDT->Header.Length - sizeof(ACPI_SDT_HEADER)) / sizeof(UINT32);

        for (UINTN index = 0; index < EntryCount; index++)
        {
            ACPI_SDT_HEADER* Header = (ACPI_SDT_HEADER*)(UINT64)RSDT->Entry[index];

            if (Header != NULL && Header->Length >= sizeof(ACPI_SDT_HEADER) && Header->Signature == Signature &&
                ACPIChecksum(Header, Header->Length))
            {
                return Header;
            }
        }
    }

    return NULL;
}

ACPI_HPET* HPETGetTable(VOID)
{
    return HPET;
}

UINT64 HPETGetBaseAddress(VOID)
{
    if (HPET == NULL)
    {
        return 0;
    }

    return HPET->BaseAddress.Address;
}

BOOLEAN HPETIsAvailable(VOID)
{
    return HPET != NULL && HPET->BaseAddress.Address != 0;
}
