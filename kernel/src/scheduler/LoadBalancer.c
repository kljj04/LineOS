// kernel/src/scheduler/LoadBalancer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LineOS/TypeInfo.h>
#include <scheduler/LoadBalancer.h>
#include <scheduler/LBPWRR.h>
#include <scheduler/TaskTypes.h>
#include <multicore/SMP.h>

STATIC UINT32 LoadBalancerTicks;

STATIC BOOLEAN LoadBalancerFindSourceCPU(UINT32 *CPUID)
{
    UINT32 CPUCount;
    UINT32 CurrentCPUID;
    UINT32 count;
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
        count = LBPWRRGetCPUAssignedTaskCount(CurrentCPUID);

        if (count > MaxCount)
        {
            MaxCount = count;
            *CPUID = CurrentCPUID;
        }
    }

    return MaxCount > 0;
}

STATIC BOOLEAN LoadBalancerFindTargetCPU(UINT32 *CPUID)
{
    UINT32 CPUCount;
    UINT32 CurrentCPUID;
    UINT32 count;
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
        count = LBPWRRGetCPUAssignedTaskCount(CurrentCPUID);

        if (count < MinCount)
        {
            MinCount = count;
            *CPUID = CurrentCPUID;
        }
    }

    return TRUE;
}

STATIC BOOLEAN LoadBalancerMoveTask(UINT32 SourceCPUID, UINT32 TargetCPUID, TASK *task)
{
    if (task == NULL)
    {
        return FALSE;
    }

    if (SourceCPUID == TargetCPUID)
    {
        return FALSE;
    }

    if (task->CPUID != SourceCPUID)
    {
        return FALSE;
    }

    return LBPWRRMoveTask(task, TargetCPUID);
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
    TASK  *task;

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

    task = LBPWRRGetTaskFromCPU(SourceCPUID);

    if (task == NULL)
    {
        return;
    }

    LoadBalancerMoveTask(SourceCPUID, TargetCPUID, task);
}
