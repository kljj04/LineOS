// kernel/include/debug/debug.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

VOID DebugWrite(CONST char *String);
VOID DebugWriteLine(CONST char *String);
VOID DebugWriteWide(CONST CHAR16 *String);
VOID DebugWriteHex(UINT64 Value);
VOID DebugWriteDec(UINT64 Value);
