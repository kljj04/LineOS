// bootloader/include/MemoryLib.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

VOID *CopyMem(VOID *destination, CONST VOID *source, UINTN length);
VOID *SetMem(VOID *buffer, UINTN length, UINT8 value);
