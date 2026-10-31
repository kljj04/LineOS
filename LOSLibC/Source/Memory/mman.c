// LOSLibC/Source/Memory/mman.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <sys/mman.h>
#include <syscall.h>

void *mmap(void *Address, size_t Length, int Protection, int Flags, int FD, long Offset)
{
    return (void *) LineOSSyscallResult(LineOSSyscall6(SYSCALL_MMAP, (UINT64) Address, Length, Protection, Flags, (UINT64) FD, (UINT64) Offset));
}

int munmap(void *Address, size_t Length)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_MUNMAP, (UINT64) Address, Length));
}
