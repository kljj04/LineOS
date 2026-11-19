// Libraries/LOSLibC/Source/Runtime/errno.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <errno.h>
#include <syscall.h>

STATIC __thread int ThreadErrno;

int *__errno_location(void)
{
    return &ThreadErrno;
}

void *__tls_get_addr(UINT64 *Index)
{
    UINT64 *DTV;
    ASM("movq %%fs:8, %0" : "=r"(DTV));
    return (VOID *) (DTV[Index[0] * 2] + Index[1]);
}

INT64 LineOSSyscallResult(UINT64 Result)
{
    if (Result >= (UINT64) -4095)
    {
        errno = (int) -(INT64) Result;

        return -1;
    }

    return (INT64) Result;
}
