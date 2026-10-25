// Libraries/Include/TrueType/TrueTypeRuntime.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>

typedef struct
{
    VOID *(*Allocate)(UINTN Size, VOID *Context);
    VOID  (*Free)(VOID *Pointer, VOID *Context);
    VOID *Context;
} TTF_ALLOCATOR;

INT32   TTFFloor(FLOAT64 Value);
INT32   TTFCeil(FLOAT64 Value);
FLOAT64 TTFSqrt(FLOAT64 Value);
FLOAT64 TTFPow(FLOAT64 Base, FLOAT64 Exponent);
FLOAT64 TTFFMod(FLOAT64 Value, FLOAT64 Divisor);
FLOAT64 TTFCos(FLOAT64 Value);
FLOAT64 TTFACos(FLOAT64 Value);
FLOAT64 TTFFAbs(FLOAT64 Value);
VOID   *TTFAlloc(UINTN Size, VOID *UserData);
VOID    TTFFree(VOID *Pointer, VOID *UserData);
VOID    TTFAssert(BOOLEAN Condition);
UINTN   TTFStrLen(CONST CHAR8 *String);
