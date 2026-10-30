// LOSLibC/Source/sysinfo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <LineOS/Sysinfo.h>
#include <LineOS/Syscall.h>
#include <syscall.h>

INT32 sysinfo(SYS_INFO *Info)
{
    return (INT32) LineOSSyscall1(SYSCALL_SYSINFO, (UINT64) Info);
}