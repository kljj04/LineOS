// LOSLibC/Source/File/directory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

#include <sys/stat.h>

int mkdir(const char *Path, mode_t Mode)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_MKDIR, (UINT64) Path, (UINT64) Mode));
}

ssize_t getdents64(int FD, void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_GETDENTS64, (UINT64) FD, (UINT64) Buffer, Count));
}

int chdir(const char *Path)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_CHDIR, (UINT64) Path));
}

char *getcwd(char *Buffer, size_t Size)
{
    return LineOSSyscallResult(LineOSSyscall2(SYSCALL_GETCWD, (UINT64) Buffer, Size)) < 0 ? NULL : Buffer;
}
