// LOSLibC/crt0.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdlib.h>

EXTERN INT32 main(INT32 Argc, char **Argv, char **Envp);

VOID NORETURN _Start(INT32 Argc, char **Argv, char **Envp)
{
    INT32 Status;

    environ = Envp;

    Status = main(Argc, Argv, Envp);

    exit(Status);
}
