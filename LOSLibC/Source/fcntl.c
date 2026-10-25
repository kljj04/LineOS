// LOSLibC/Source/fcntl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <fcntl.h>
#include <syscall.h>

int open(const char *Path, int Flags, ...)
{
    return (int) LineOSSyscall2(3, (UINT64) Path, (UINT64) Flags);
}
