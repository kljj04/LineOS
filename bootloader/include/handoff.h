// handoff.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>
#include <lineos/bootinfo.h>

BOOLEAN CreateBootInfo(void);
VOID JumpKernel(void);

extern LINEOS_BOOT_INFO *BootInfo;