// LOSLibC/Source/unistd.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

ssize_t write(int FD, const void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscall3(2, (UINT64) FD, (UINT64) Buffer, (UINT64) Count);
}

ssize_t read(int FD, void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscall3(1, (UINT64) FD, (UINT64) Buffer, (UINT64) Count);
}

int close(int FD)
{
    return (int) LineOSSyscall1(4, (UINT64) FD);
}

int setuid(uid_t UID)
{
    return (int) LineOSSyscall1(20, UID);
}

int setgid(gid_t GID)
{
    return (int) LineOSSyscall1(21, GID);
}
