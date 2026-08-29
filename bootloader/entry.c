// bootloader/entry.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <acpi.h>
#include <elf.h>
#include <gop.h>
#include <handoff.h>
#include <lineosuefi.h>
#include <memory.h>

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    UEFIImageHandle = ImageHandle;
    UEFISystemTable = SystemTable;
    UEFIBootServices = SystemTable->BootServices;

    if (!LineOSDisableWatchdog())
    {
        LineOSHaltWithMessage(L"failed to disable UEFI watchdog", LineOSLastWatchdogStatus);
    }

    MemorySetImageHandle(ImageHandle);

    if (!GOPInit())
    {
        LineOSHaltWithMessage(L"GOPInit failed", EFI_ABORTED);
    }

    if (!LineOSDisableWatchdog())
    {
        LineOSHaltWithMessage(L"failed to disable UEFI watchdog after GOP init", LineOSLastWatchdogStatus);
    }

    if (!MemoryInit())
    {
        LineOSHaltWithMessage(L"MemoryInit failed", EFI_ABORTED);
    }

    if (!ACPIInit())
    {
        LineOSHaltWithMessage(L"ACPIInit failed", EFI_ABORTED);
    }

    if (!LoadKernel())
    {
        LineOSHaltWithMessage(L"LoadKernel failed", EFI_ABORTED);
    }

    if (!CreateBootInfo())
    {
        LineOSHaltWithMessage(L"CreateBootInfo failed", EFI_ABORTED);
    }

    if (!LineOSDisableWatchdog())
    {
        LineOSHaltWithMessage(L"failed to disable UEFI watchdog before exit", LineOSLastWatchdogStatus);
    }

    if (!ExitBootServices())
    {
        LineOSHaltWithMessage(L"ExitBootServices failed", EFI_ABORTED);
    }

    JumpKernel();

    LineOSHaltWithMessage(L"kernel returned", EFI_ABORTED);
    return EFI_ABORTED;
}
