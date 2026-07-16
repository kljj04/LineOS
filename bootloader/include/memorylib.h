// memorylib.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Uefi.h>

VOID *CopyMem(VOID *Destination, CONST VOID *Source, UINTN Length);
VOID *SetMem(VOID *Buffer, UINTN Length, UINT8 Value);