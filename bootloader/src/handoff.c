// handoff.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <acpi.h>
#include <elf.h>
#include <gop.h>
#include <handoff.h>
#include <lineosuefi.h>
#include <memory.h>

LINEOS_BOOT_INFO *BootInfo = NULL;

BOOLEAN CreateBootInfo(VOID)
{
    EFI_STATUS status;

    status = UEFIBootServices->AllocatePool(EfiLoaderData, sizeof(LINEOS_BOOT_INFO), (VOID **) &BootInfo);

    if (EFI_ERROR(status))
        return FALSE;

    BootInfo->Magic = LINEOS_BOOT_MAGIC;
    BootInfo->Version = 1;
    BootInfo->Size = sizeof(LINEOS_BOOT_INFO);

    BootInfo->GOP = &GOP;
    BootInfo->MemoryMap = &MemoryMap;
    BootInfo->RSDP = RSDP;
    BootInfo->Kernel.Base = kernel.Base;
    BootInfo->Kernel.Size = kernel.Size;
    BootInfo->Kernel.Entry = kernel.Entry;

    return TRUE;
}

VOID JumpKernel(VOID)
{
    typedef VOID(__attribute__((ms_abi)) * KERNEL_ENTRY)(LINEOS_BOOT_INFO *);
    KERNEL_ENTRY KernelEntry;

    KernelEntry = (KERNEL_ENTRY) kernel.Entry;

    KernelEntry(BootInfo);
}
