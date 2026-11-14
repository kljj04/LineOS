// LOSLibC/crt0.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdlib.h>

EXTERN INT32 main(INT32 Argc, char **Argv, char **Envp);

EXTERN VOID (*__InitArrayStart[])(VOID);
EXTERN VOID (*__InitArrayEnd[])(VOID);

VOID NORETURN _Start(INT32 Argc, char **Argv, char **Envp)
{
    INT32 Status;

    environ = Envp;

    for (VOID (**Constructor)(VOID) = __InitArrayStart; Constructor != __InitArrayEnd; Constructor++)
    {
        (*Constructor)();
    }

    Status = main(Argc, Argv, Envp);

    exit(Status);
}
