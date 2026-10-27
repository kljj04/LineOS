// LOSLibC/Source/fcntl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
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
    return (int) LineOSSyscall3(3, (UINT64) Path, (UINT64) Flags, Mode);
}

int rename(const char *OldPath, const char *NewPath)
{
    return (int) LineOSSyscall2(16, (UINT64) OldPath, (UINT64) NewPath);
}

int remove(const char *Path)
{
    return (int) LineOSSyscall1(17, (UINT64) Path);
}

int mkdir(const char *Path, mode_t Mode)
{
    return (int) LineOSSyscall2(18, (UINT64) Path, (UINT64) Mode);
}
