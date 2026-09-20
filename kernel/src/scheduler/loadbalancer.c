// kernel/src/scheduler/loadbalancer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/typeinfo.h>
#include <scheduler/loadbalancer.h>
#include <scheduler/lbpwrr.h>
#include <scheduler/task_types.h>
#include <multicore/smp.h>

STATIC UINT32 LoadBalancerTicks;

STATIC BOOLEAN LoadBalancerFindSourceCPU(UINT32 *CPUID)
{
    UINT32 CPUCount;
    UINT32 CurrentCPUID;
    UINT32 TaskCount;
    UINT32 MaxTaskCount;

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
    MaxTaskCount = 0;

    for (CurrentCPUID = 0; CurrentCPUID < CPUCount; CurrentCPUID++)
    {
        TaskCount = LBPWRRGetCPUAssignedTaskCount(CurrentCPUID);

        if (TaskCount > MaxTaskCount)
        {
            MaxTaskCount = TaskCount;
            *CPUID = CurrentCPUID;
        }
    }

    return MaxTaskCount > 0;
}

STATIC BOOLEAN LoadBalancerFindTargetCPU(UINT32 *CPUID)
{
    UINT32 CPUCount;
    UINT32 CurrentCPUID;
    UINT32 TaskCount;
    UINT32 MinTaskCount;

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
    MinTaskCount = 0xFFFFFFFF;

    for (CurrentCPUID = 0; CurrentCPUID < CPUCount; CurrentCPUID++)
    {
        TaskCount = LBPWRRGetCPUAssignedTaskCount(CurrentCPUID);

        if (TaskCount < MinTaskCount)
        {
            MinTaskCount = TaskCount;
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
    TASK   *task;

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