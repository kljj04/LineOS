// handoff.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include "handoff.h"
#include "elf.h"
#include "gop.h"
#include "memory.h"
#include "acpi.h"


LINEOS_BOOT_INFO *BootInfo = NULL;


BOOLEAN CreateBootInfo(void)
{
    EFI_STATUS Status;

    Status = gBS->AllocatePool(
        EfiLoaderData,
        sizeof(LINEOS_BOOT_INFO),
        (VOID**)&BootInfo
    );

    if(EFI_ERROR(Status))
        return FALSE;


    BootInfo->Magic = LINEOS_BOOT_MAGIC;
    BootInfo->Version = 1;
    BootInfo->Size = sizeof(LINEOS_BOOT_INFO);

    BootInfo->GOP = &GOP;
    BootInfo->MemoryMap = &MemoryMap;
    BootInfo->RSDP = RSDP;

    return TRUE;
}


VOID JumpKernel(void)
{
    void (*KernelEntry)(LINEOS_BOOT_INFO*);

    KernelEntry =
        (void (*)(LINEOS_BOOT_INFO*))
        Kernel.Entry;

    KernelEntry(BootInfo);
}