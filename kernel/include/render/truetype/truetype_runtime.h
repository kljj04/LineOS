// truetype_runtime.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <memory/memory.h>

INT32   KFloor(FLOAT64 Value);
INT32   KCeil(FLOAT64 Value);
FLOAT64 KSqrt(FLOAT64 Value);
FLOAT64 KPow(FLOAT64 Base, FLOAT64 Exponent);
FLOAT64 KFMod(FLOAT64 Value, FLOAT64 Divisor);
FLOAT64 KCos(FLOAT64 Value);
FLOAT64 KACos(FLOAT64 Value);
FLOAT64 KFAbs(FLOAT64 Value);
VOID   *KTTFAlloc(UINTN Size, VOID *UserData);
VOID    KTTFFree(VOID *Pointer, VOID *UserData);
VOID    KAssert(BOOLEAN Condition);
UINTN   KStrLen(CONST CHAR8 *String);
