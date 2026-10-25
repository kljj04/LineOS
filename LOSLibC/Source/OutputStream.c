// LOSLibC/Source/OutputStream.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <OutputStream.h>
#include <syscall.h>

ssize_t LOSOutputStreamRead(void *Buffer, size_t Count)
{
    return (ssize_t) LineOSSyscall2(10, (UINT64) Buffer, (UINT64) Count);
}
