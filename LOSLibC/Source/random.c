// LOSLibC/Source/random.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/random.h>
#include <syscall.h>

ssize_t getrandom(void *Buffer, size_t Count, unsigned int Flags)
{
    return (ssize_t) LineOSSyscall3(19, (UINT64) Buffer, (UINT64) Count, Flags);
}
