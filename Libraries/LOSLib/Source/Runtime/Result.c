// Libraries/LOSLib/Source/Runtime/Result.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "../Internal.h"
#include <errno.h>

INT64 LOSSyscallResult(UINT64 Result)
{
    if (Result >= (UINT64) -4095)
    {
        errno = (int) -(INT64) Result;
        return -1;
    }
    return (INT64) Result;
}
