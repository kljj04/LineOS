// LOSLibC/Source/errno.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <errno.h>
#include <syscall.h>

typedef struct
{
    int Errno;
} LIBC_THREAD_STATE;

STATIC LIBC_THREAD_STATE LibCThreadState;

int *__errno_location(void)
{
    return &LibCThreadState.Errno;
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
