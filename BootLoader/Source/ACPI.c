// BootLoader/Source/ACPI.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <ACPI.h>
#include <LineOSUEFI.h>

ACPI_RSDP *RSDP = NULL;

STATIC BOOLEAN IsGuidEqual(EFI_GUID *A, EFI_GUID *B)
{
    return A->Data1 == B->Data1 && A->Data2 == B->Data2 && A->Data3 == B->Data3 && A->Data4[0] == B->Data4[0] && A->Data4[1] == B->Data4[1] && A->Data4[2] == B->Data4[2] && A->Data4[3] == B->Data4[3] && A->Data4[4] == B->Data4[4] && A->Data4[5] == B->Data4[5] && A->Data4[6] == B->Data4[6] && A->Data4[7] == B->Data4[7];
}

BOOLEAN ACPIInit(VOID)
{
    EFI_CONFIGURATION_TABLE *Table;

    UINTN Count;

    Table = UEFISystemTable->ConfigurationTable;
    Count = UEFISystemTable->NumberOfTableEntries;

    for (UINTN I = 0; I < Count; I++)
    {
        EFI_GUID *GUID = &Table[I].VendorGuid;

        if (IsGuidEqual(GUID, &gEfiAcpi20TableGuid))
        {
            RSDP = (ACPI_RSDP *) Table[I].VendorTable;
            return TRUE;
        }
    }

    return RSDP != NULL;
}
