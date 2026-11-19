// Libraries/LOSLibC/Source/Process/exit.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

VOID (*LibCFinalizer)(VOID);

VOID NORETURN exit(int ErrorCode)
{
    VOID (*Finalizer)(VOID) = LibCFinalizer;
    LibCFinalizer = NULL;

    if (Finalizer != NULL)
    {
        Finalizer();
    }

    LineOSSyscall1(SYSCALL_EXIT_GROUP, (UINT64) ErrorCode);

    for (;;)
    {
        ASM("int3");
    }
}
