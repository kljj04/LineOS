// BootLoader/Include/Memory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/BootInfo.h>

BOOLEAN MemoryInit(VOID);
BOOLEAN ExitBootServices(VOID);
VOID    MemorySetImageHandle(EFI_HANDLE Handle);

EXTERN LINEOS_MEMORY_MAP MemoryMap;
