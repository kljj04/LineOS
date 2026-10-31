// Kernel/Source/Syscall/System/SyscallMonitor.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/System/SyscallMonitor.h>
#include <Syscall/System/SystemMonitor.h>
#include <Syscall/Errno.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Scheduler/Usage.h>
#include <Multicore/SMP.h>
#include <Memory/PMM.h>
#include <Memory/VMM.h>
#include <Timer/Clock.h>

INT64 SyscallSystemMonitor(UINT64 UserInfo, UINT64 Size)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (Size != sizeof(SYSTEM_MONITOR_INFO))
    {
        return -EINVAL;
    }

    SYSTEM_MONITOR_INFO *Info = KAlloc(sizeof(*Info));

    if (Info == NULL)
    {
        return -ENOMEM;
    }

    KMemSet(Info, 0, sizeof(*Info));
    Info->Version = SYSTEM_MONITOR_VERSION;
    Info->CPUCount = SMPGetCPUCount();

    if (Info->CPUCount > SYSTEM_MONITOR_CPUS)
    {
        Info->CPUCount = SYSTEM_MONITOR_CPUS;
    }

    Info->CPUUsage = UsageGetTotal();
    Info->UptimeMls = ClockGetUptimeMls();

    UINT64 Managed;
    UINT64 Free;

    KGetManagedMemory(&Managed, &Free);
    Info->ManagedBytes = Managed;
    Info->FreeBytes = Free;
    Info->TaskCount = TaskGetLiveCount();
    Info->IdleTaskCount = TaskGetIdleCount();
    Info->TasksReturned = TaskGetSnapshot(Info->Tasks, SYSTEM_MONITOR_TASKS);

    for (UINT32 Index = 0; Index < Info->CPUCount; Index++)
    {
        Info->CPUs[Index] = UsageGetCPU(Index);

        UINT64 Busy;
        UINT64 Idle;

        UsageGetCounters(Index, &Busy, &Idle);
        Info->BusyTicks[Index] = Busy;
        Info->IdleTicks[Index] = Idle;
    }

    BOOLEAN Copied = VMMCopyToUser(Task->AddressSpace, UserInfo, Info, sizeof(*Info));

    KFree(Info);

    return Copied ? 0 : -EFAULT;
}
