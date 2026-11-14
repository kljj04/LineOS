// LOSLibC/Source/Process/exit.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

#define EXIT_CALLBACK_LIMIT 32

typedef struct
{
    VOID (*Function)(VOID);
    VOID (*Destructor)(VOID *);
    VOID *Argument;
} EXIT_CALLBACK;

STATIC EXIT_CALLBACK ExitCallbacks[EXIT_CALLBACK_LIMIT];
VOID *__dso_handle = &__dso_handle;
STATIC unsigned int ExitCallbackCount;

int atexit(VOID (*Callback)(VOID))
{
    if (ExitCallbackCount == EXIT_CALLBACK_LIMIT)
    {
        return -1;
    }

    ExitCallbacks[ExitCallbackCount++] = (EXIT_CALLBACK) {.Function = Callback};

    return 0;
}

int __cxa_atexit(VOID (*Destructor)(VOID *), VOID *Argument, VOID *DSO)
{
    (VOID) DSO;

    if (ExitCallbackCount == EXIT_CALLBACK_LIMIT)
    {
        return -1;
    }

    ExitCallbacks[ExitCallbackCount++] = (EXIT_CALLBACK) {.Destructor = Destructor, .Argument = Argument};

    return 0;
}

VOID NORETURN exit(int ErrorCode)
{
    while (ExitCallbackCount != 0)
    {
        EXIT_CALLBACK Callback = ExitCallbacks[--ExitCallbackCount];

        if (Callback.Destructor != NULL)
        {
            Callback.Destructor(Callback.Argument);
        }
        else
        {
            Callback.Function();
        }
    }

    LineOSSyscall1(SYSCALL_EXIT, (UINT64) ErrorCode);

    for (;;)
    {
        ASM("int3");
    }
}

VOID NORETURN abort(VOID)
{
    LineOSSyscall1(SYSCALL_EXIT, 134);

    for (;;)
    {
        ASM("int3");
    }
}
