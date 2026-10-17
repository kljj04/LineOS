// bootloader/include/HandOff.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/BootInfo.h>

BOOLEAN CreateBootInfo(VOID);
VOID    JumpKernel(VOID);

EXTERN LINEOS_BOOT_INFO *BootInfo;
