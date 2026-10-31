// LOSLibC/Source/Process/exit.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

VOID NORETURN exit(int ErrorCode)
{
    LineOSSyscall1(SYSCALL_EXIT, (UINT64) ErrorCode);

    for (;;)
    {
        ASM("int3");
    }
}
