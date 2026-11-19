// Libraries/LOSLibC/Runtime/Start.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdlib.h>
#include <syscall.h>

EXTERN VOID (*LibCFinalizer)(VOID);

EXTERN INT32 main(INT32 Argc, char **Argv, char **Envp);

VOID NORETURN LibCStart(INT32 Argc, char **Argv, char **Envp, VOID (*Finalizer)(VOID))
{
    INT32 Status;

    environ = Envp;
    LibCFinalizer = Finalizer;

    Status = main(Argc, Argv, Envp);

    exit(Status);
}
