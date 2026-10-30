// Kernel/Source/Syscall/SyscallSysinfo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LineOS/TypeInfo.h>
#include <LineOS/Errno.h>
#include <Syscall/SyscallSysinfo.h>
#include <LineOS/Sysinfo.h>
#include <Timer/Clock.h>
#include <Memory/VMM.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>

INT64 SyscallSysInfo(UINT64 UserInfo)
{
    TASK     *Task;
    SYS_INFO  Info = {0};

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    Info.Uptime  = (INT64)(ClockGetUptimeMls() / 1000);
    Info.MemUnit = 1;
    UINTN Count = TaskGetCount();
    Info.Processes = Count > 65535U ? 65535U : (UINT16) Count;

    if (!VMMCopyToUser(Task->AddressSpace, UserInfo, &Info, sizeof(Info)))
    {
        return -EFAULT;
    }

    return 0;
}