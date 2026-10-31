// LOSLibC/Source/File/path.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <syscall.h>

int rename(const char *OldPath, const char *NewPath)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_RENAME, (UINT64) OldPath, (UINT64) NewPath));
}

int remove(const char *Path)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_REMOVE, (UINT64) Path));
}
