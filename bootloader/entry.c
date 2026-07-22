// entry.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <acpi.h>
#include <elf.h>
#include <gop.h>
#include <handoff.h>
#include <memory.h>
#include <lineosuefi.h>

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    UEFIImageHandle = ImageHandle;
    UEFISystemTable = SystemTable;
    UEFIBootServices = SystemTable->BootServices;

    // Disable watchdog
    UEFIBootServices->SetWatchdogTimer(0, 0, 0, NULL);

    MemorySetImageHandle(ImageHandle);

    if (!GOPInit())
    {
        return EFI_ABORTED;
    }

    if (!MemoryInit())
    {
        return EFI_ABORTED;
    }

    if (!ACPIInit())
    {
        return EFI_ABORTED;
    }

    if (!LoadKernel())
    {
        return EFI_ABORTED;
    }

    if (!CreateBootInfo())
    {
        return EFI_ABORTED;
    }

    if (!ExitBootServices())
    {
        return EFI_ABORTED;
    }

    JumpKernel();

    return EFI_SUCCESS;
}
