// LOSLibC/Source/fcntl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <syscall.h>
#include <errno.h>
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

int rename(const char *OldPath, const char *NewPath)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_RENAME, (UINT64) OldPath, (UINT64) NewPath));
}

int remove(const char *Path)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_REMOVE, (UINT64) Path));
}

int mkdir(const char *Path, mode_t Mode)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_MKDIR, (UINT64) Path, (UINT64) Mode));
}
