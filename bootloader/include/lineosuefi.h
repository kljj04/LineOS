// lineosuefi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>

extern EFI_SYSTEM_TABLE *UEFISystemTable;
extern EFI_BOOT_SERVICES *UEFIBootServices;
extern EFI_HANDLE UEFIImageHandle;
extern EFI_STATUS LineOSLastWatchdogStatus;

BOOLEAN LineOSDisableWatchdog(VOID);
VOID LineOSHaltWithMessage(CONST CHAR16 *Message, EFI_STATUS Status);
