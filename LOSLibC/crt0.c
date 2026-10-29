// LOSLibC/crt0.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdlib.h>

EXTERN INT32 main(INT32 Argc, CHAR8 **Argv);

VOID NORETURN _Start(INT32 Argc, CHAR8 **Argv, CHAR8 **Envp)
{
    INT32 Status;

    (VOID) Envp;

    Status = main(Argc, Argv);

    exit(Status);
}