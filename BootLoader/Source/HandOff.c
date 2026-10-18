// BootLoader/Source/HandOff.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <ACPI.h>
#include <ELF.h>
#include <GOP.h>
#include <HandOff.h>
#include <LineOSUEFI.h>
#include <Memory.h>

LINEOS_BOOT_INFO *BootInfo = NULL;

BOOLEAN CreateBootInfo(VOID)
{
    EFI_STATUS Status;

    Status = UEFIBootServices->AllocatePool(EfiLoaderData, sizeof(LINEOS_BOOT_INFO), (VOID **) &BootInfo);

    if (EFI_ERROR(Status))
        return FALSE;

    BootInfo->Magic = LINEOS_BOOT_MAGIC;
    BootInfo->Version = 1;
    BootInfo->Size = sizeof(LINEOS_BOOT_INFO);

    BootInfo->GOP = &GOP;
    BootInfo->MemoryMap = &MemoryMap;
    BootInfo->RSDP = RSDP;
    BootInfo->Kernel.Base = Kernel.Base;
    BootInfo->Kernel.Size = Kernel.Size;
    BootInfo->Kernel.Entry = Kernel.Entry;

    return TRUE;
}

VOID JumpKernel(VOID)
{
    typedef VOID(MS_ABI * KERNEL_ENTRY)(LINEOS_BOOT_INFO *);
    KERNEL_ENTRY KernelEntry;

    KernelEntry = (KERNEL_ENTRY) Kernel.Entry;

    KernelEntry(BootInfo);
}
