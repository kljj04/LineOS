// LOSLibC/Source/File/io.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

ssize_t write(int FD, const void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_WRITE, (UINT64) FD, (UINT64) Buffer, (UINT64) Count));
}

ssize_t read(int FD, void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_READ, (UINT64) FD, (UINT64) Buffer, (UINT64) Count));
}

int close(int FD)
{
    return (int) LineOSSyscallResult(LineOSSyscall1(SYSCALL_CLOSE, (UINT64) FD));
}
