// Libraries/LOSLib/Source/System/SystemMonitor.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <SystemMonitor.h>
#include "../Internal.h"

int LOSSystemMonitor(SYSTEM_MONITOR_INFO *Info)
{
    return (int) LOSSyscallResult(LOSSyscall2(SYSCALL_SYSTEM_MONITOR, (UINT64) Info, sizeof(*Info)));
}
