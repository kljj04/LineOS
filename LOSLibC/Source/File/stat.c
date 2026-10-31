// LOSLibC/Source/File/stat.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/stat.h>
#include <syscall.h>

_Static_assert(sizeof(struct stat) == sizeof(STAT_INFO), "stat ABI size");
_Static_assert(__builtin_offsetof(struct stat, st_mode) == __builtin_offsetof(STAT_INFO, Mode), "stat ABI mode");

int stat(const char *Path, struct stat *Info)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_STAT, (UINT64) Path, (UINT64) Info));
}

int lstat(const char *Path, struct stat *Info)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_LSTAT, (UINT64) Path, (UINT64) Info));
}

int fstat(int FD, struct stat *Info)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_FSTAT, (UINT64) FD, (UINT64) Info));
}
