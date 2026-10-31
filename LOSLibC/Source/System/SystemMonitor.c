// LOSLibC/Source/System/SystemMonitor.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <SystemMonitor.h>
#include <syscall.h>

int LOSSystemMonitor(SYSTEM_MONITOR_INFO *Info)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_SYSTEM_MONITOR, (UINT64) Info, sizeof(*Info)));
}
