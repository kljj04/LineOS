// memory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>
#include <lineos/bootinfo.h>

BOOLEAN MemoryInit(VOID);
BOOLEAN ExitBootServices(VOID);
VOID MemorySetImageHandle(EFI_HANDLE Handle);

extern LINEOS_MEMORY_MAP MemoryMap;
