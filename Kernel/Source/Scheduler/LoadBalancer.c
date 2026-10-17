// kernel/src/scheduler/LoadBalancer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LineOS/TypeInfo.h>
#include <Scheduler/LoadBalancer.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/TaskTypes.h>
#include <Multicore/SMP.h>

STATIC UINT32 LoadBalancerTicks;

STATIC BOOLEAN LoadBalancerFindSourceCPU(UINT32 *CPUID)
{
    UINT32 CPUCount;
    UINT32 CurrentCPUID;
    UINT32 Count;
    UINT32 MaxCount;

    if (CPUID == NULL)
    {
        return FALSE;
    }

    CPUCount = SMPGetCPUCount();

    if (CPUCount == 0)
    {
        return FALSE;
    }

    *CPUID = 0;
    MaxCount = 0;

    for (CurrentCPUID = 0; CurrentCPUID < CPUCount; CurrentCPUID++)
    {
        Count = LBPWRRGetCPUAssignedTaskCount(CurrentCPUID);

        if (Count > MaxCount)
        {
            MaxCount = Count;
            *CPUID = CurrentCPUID;
        }
    }

    return MaxCount > 0;
}

STATIC BOOLEAN LoadBalancerFindTargetCPU(UINT32 *CPUID)
{
    UINT32 CPUCount;
    UINT32 CurrentCPUID;
    UINT32 Count;
    UINT32 MinCount;

    if (CPUID == NULL)
    {
        return FALSE;
    }

    CPUCount = SMPGetCPUCount();

    if (CPUCount == 0)
    {
        return FALSE;
    }

    *CPUID = 0;
    MinCount = 0xFFFFFFFF;

    for (CurrentCPUID = 0; CurrentCPUID < CPUCount; CurrentCPUID++)
    {
        Count = LBPWRRGetCPUAssignedTaskCount(CurrentCPUID);

        if (Count < MinCount)
        {
            MinCount = Count;
            *CPUID = CurrentCPUID;
        }
    }

    return TRUE;
}

STATIC BOOLEAN LoadBalancerMoveTask(UINT32 SourceCPUID, UINT32 TargetCPUID, TASK *Task)
{
    if (Task == NULL)
    {
        return FALSE;
    }

    if (SourceCPUID == TargetCPUID)
    {
        return FALSE;
    }

    if (Task->CPUID != SourceCPUID)
    {
        return FALSE;
    }

    return LBPWRRMoveTask(Task, TargetCPUID);
}

VOID LoadBalancerInit(VOID)
{
    LoadBalancerTicks = 0;
}

VOID LoadBalancerTick(VOID)
{
    LoadBalancerTicks++;

    if (LoadBalancerTicks < 20)
    {
        return;
    }

    LoadBalancerTicks = 0;

    LoadBalancerBalance();
}

VOID LoadBalancerBalance(VOID)
{
    UINT32 SourceCPUID;
    UINT32 TargetCPUID;
    TASK  *Task;

    if (!LoadBalancerFindSourceCPU(&SourceCPUID))
    {
        return;
    }

    if (!LoadBalancerFindTargetCPU(&TargetCPUID))
    {
        return;
    }

    if (SourceCPUID == TargetCPUID)
    {
        return;
    }

    Task = LBPWRRGetTaskFromCPU(SourceCPUID);

    if (Task == NULL)
    {
        return;
    }

    LoadBalancerMoveTask(SourceCPUID, TargetCPUID, Task);
}
