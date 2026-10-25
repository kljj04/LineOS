// Kernel/Include/Render/Console/Console.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

BOOLEAN ConsoleInit(VOID);
UINT64  ConsoleWrite(CONST CHAR8 *Buffer, UINT64 Count);