// bootloader/include/lineosuefi.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

EXTERN EFI_SYSTEM_TABLE  *UEFISystemTable;
EXTERN EFI_BOOT_SERVICES *UEFIBootServices;
EXTERN EFI_HANDLE         UEFIImageHandle;
EXTERN EFI_STATUS         LineOSLastWatchdogStatus;

BOOLEAN LineOSDisableWatchdog(VOID);
VOID    LineOSHaltWithMessage(CONST CHAR16 *Message, EFI_STATUS Status);
