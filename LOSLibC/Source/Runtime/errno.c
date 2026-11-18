// LOSLibC/Source/Runtime/errno.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <errno.h>
#include <syscall.h>

#define ARCH_GET_FS 0x1003U

STATIC int FallbackErrno;

int *__errno_location(void)
{
    UINT64 Base = 0;

    if (LineOSSyscall2(SYSCALL_ARCH_PRCTL, ARCH_GET_FS, (UINT64) &Base) != 0 || Base == 0)
    {
        return &FallbackErrno;
    }

    return (int *) Base;
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
