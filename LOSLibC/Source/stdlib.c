// LOSLibC/Source/stdlib.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

VOID NORETURN exit(INT8 ErrorCode)
{
    LineOSSyscall1(0, (UINT64) ErrorCode);

    for (;;)
    {
        ASM("int3");
    }
}