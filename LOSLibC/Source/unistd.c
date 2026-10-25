// LOSLibC/Source/unistd.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>
#include <syscall.h>

#define SYSCALL_WRITE 2ULL

ssize_t write(int FD, const void *Buffer, size_t Count)
{
    unsigned long Result;

    ASM(
        "int $0x80"
        : "=a"(Result)
        : "a"(SYSCALL_WRITE),
          "D"((unsigned long) FD),
          "S"((unsigned long) Buffer),
          "d"((unsigned long) Count)
        : "memory", "cc"
    );

    return (ssize_t) Result;
}

ssize_t read(int FD, void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscall3(1, (UINT64) FD, (UINT64) Buffer, (UINT64) Count);
}

int close(int FD)
{
    return (int) LineOSSyscall1(4, (UINT64) FD);
}
