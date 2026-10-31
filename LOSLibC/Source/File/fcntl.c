// LOSLibC/Source/File/fcntl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <fcntl.h>
#include <syscall.h>
#include <stdarg.h>

int open(const char *Path, int Flags, ...)
{
    UINT64 Mode = 0;

    if ((Flags & O_CREAT) != 0)
    {
        va_list Arguments;
        va_start(Arguments, Flags);
        Mode = (UINT64) va_arg(Arguments, unsigned int);
        va_end(Arguments);
    }

    return (int) LineOSSyscallResult(LineOSSyscall3(SYSCALL_OPEN, (UINT64) Path, (UINT64) Flags, Mode));
}
