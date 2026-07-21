// acpi.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <acpi.h>
#include <lineosuefi.h>

LINEOS_ACPI_RSDP* RSDP = NULL;

STATIC BOOLEAN IsGuidEqual(EFI_GUID* a, EFI_GUID* b)
{
    return
        a->Data1 == b->Data1 &&
        a->Data2 == b->Data2 &&
        a->Data3 == b->Data3 &&
        a->Data4[0] == b->Data4[0] &&
        a->Data4[1] == b->Data4[1] &&
        a->Data4[2] == b->Data4[2] &&
        a->Data4[3] == b->Data4[3] &&
        a->Data4[4] == b->Data4[4] &&
        a->Data4[5] == b->Data4[5] &&
        a->Data4[6] == b->Data4[6] &&
        a->Data4[7] == b->Data4[7];
}

BOOLEAN ACPIInit(VOID)
{
    EFI_CONFIGURATION_TABLE* table;

    UINTN count;

    table = UEFISystemTable->ConfigurationTable;
    count = UEFISystemTable->NumberOfTableEntries;

    for (UINTN i = 0; i < count; i++)
    {
        EFI_GUID* guid = &table[i].VendorGuid;

        if (IsGuidEqual(guid, &gEfiAcpi20TableGuid))
        {
            RSDP = (LINEOS_ACPI_RSDP*)table[i].VendorTable;
            return TRUE;
        }
    }

    return RSDP != NULL;
}
