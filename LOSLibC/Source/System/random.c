// LOSLibC/Source/System/random.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/random.h>
#include <syscall.h>
#include <errno.h>

ssize_t getrandom(void *Buffer, size_t Count, unsigned int Flags)
{
    return (ssize_t) LineOSSyscallResult(LineOSSyscall3(SYSCALL_GETRANDOM, (UINT64) Buffer, (UINT64) Count, Flags));
}
