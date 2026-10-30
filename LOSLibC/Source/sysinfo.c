// LOSLibC/Source/sysinfo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <Syscall/System/Sysinfo.h>
#include <Syscall/Numbers.h>
#include <syscall.h>

INT32 sysinfo(SYS_INFO *Info)
{
    return (INT32) LineOSSyscall1(SYSCALL_SYSINFO, (UINT64) Info);
}