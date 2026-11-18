// LOSLibC/crt0.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdlib.h>
#include <syscall.h>

#define ARCH_SET_FS 0x1002U

STATIC int MainErrno;

EXTERN INT32 main(INT32 Argc, char **Argv, char **Envp);

VOID NORETURN _Start(INT32 Argc, char **Argv, char **Envp)
{
    INT32 Status;

    if (LineOSSyscall2(SYSCALL_ARCH_PRCTL, ARCH_SET_FS, (UINT64) &MainErrno) != 0)
    {
        exit(127);
    }

    environ = Envp;

    Status = main(Argc, Argv, Envp);

    exit(Status);
}
