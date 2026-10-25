// LOSLibC/Source/unistd.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <unistd.h>
#include <LOSLibC/TypeInfo.h>

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