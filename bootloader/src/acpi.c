// acpi.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "acpi.h"

LINEOS_ACPI_RSDP *RSDP = NULL;

static BOOLEAN IsGuidEqual(EFI_GUID *A, EFI_GUID *B)
{
    return
        A->Data1 == B->Data1 &&
        A->Data2 == B->Data2 &&
        A->Data3 == B->Data3 &&
        A->Data4[0] == B->Data4[0] &&
        A->Data4[1] == B->Data4[1] &&
        A->Data4[2] == B->Data4[2] &&
        A->Data4[3] == B->Data4[3] &&
        A->Data4[4] == B->Data4[4] &&
        A->Data4[5] == B->Data4[5] &&
        A->Data4[6] == B->Data4[6] &&
        A->Data4[7] == B->Data4[7];
}

BOOLEAN ACPIInit(void)
{
    EFI_CONFIGURATION_TABLE *Table;

    UINTN Count;

    Table = gST->ConfigurationTable;
    Count = gST->NumberOfTableEntries;

    for (UINTN i = 0; i < Count; i++)
    {
        EFI_GUID *Guid = &Table[i].VendorGuid;

        if (IsGuidEqual(Guid, &gEfiAcpi20TableGuid))
        {
            RSDP = (LINEOS_ACPI_RSDP *)Table[i].VendorTable;
            return TRUE;
        }
    }

    return RSDP != NULL;
}