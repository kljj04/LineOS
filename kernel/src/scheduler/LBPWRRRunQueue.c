// kernel/src/scheduler/LBPWRRRunQueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <scheduler/LBPWRRChunk.h>
#include <multicore/SMP.h>
#include <memory/PMM.h>

STATIC LBPWRR_RUNQUEUE RunQueues[SMP_MAX_CPUS];
STATIC TASK           *CurrentTasks[SMP_MAX_CPUS];
STATIC UINT32          ActiveCPUCount;

STATIC VOID LBPWRRInitRunQueue(LBPWRR_RUNQUEUE *RunQueue)
{
    KMemSet(RunQueue, 0, sizeof(LBPWRR_RUNQUEUE));
    SpinLockInit(&RunQueue->Lock);
}

VOID LBPWRRInitRunQueues(UINT32 CPUCount)
{
    UINT32 CPUID;

    if (CPUCount == 0)
    {
        CPUCount = 1;
    }

    if (CPUCount > SMP_MAX_CPUS)
    {
        CPUCount = SMP_MAX_CPUS;
    }

    ActiveCPUCount = CPUCount;

    for (CPUID = 0; CPUID < ActiveCPUCount; CPUID++)
    {
        LBPWRRInitRunQueue(&RunQueues[CPUID]);
        CurrentTasks[CPUID] = NULL;
    }
}

UINT32 LBPWRRGetActiveCPUCount(VOID)
{
    if (ActiveCPUCount == 0)
    {
        return 1;
    }

    return ActiveCPUCount;
}

LBPWRR_RUNQUEUE *LBPWRRGetRunQueue(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    return &RunQueues[CPUID];
}

TASK *LBPWRRGetCurrentTaskFromCPU(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    return CurrentTasks[CPUID];
}

VOID LBPWRRSetCurrentTask(UINT32 CPUID, TASK *task)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    CurrentTasks[CPUID] = task;
}

BOOLEAN LBPWRRAddTask(TASK *task)
{
    UINT32           CPUID;
    UINT32           index;
    UINT32           TargetCPUID;
    UINT32           CPUCount;
    UINT64           flags;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL || task->Scheduled)
    {
        return FALSE;
    }

    if (task->Weight == 0 || task->Weight > 6)
    {
        return FALSE;
    }

    CPUCount = LBPWRRGetActiveCPUCount();
    TargetCPUID = 0;

    for (CPUID = 1; CPUID < CPUCount; CPUID++)
    {
        if (RunQueues[CPUID].RunnableCount < RunQueues[TargetCPUID].RunnableCount)
        {
            TargetCPUID = CPUID;
        }
    }

    RunQueue = &RunQueues[TargetCPUID];
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (task->Scheduled || RunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        if (RunQueue->Runnable[index] == task)
        {
            SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
            return FALSE;
        }
    }

    if (!LBPWRRModifyChunk(RunQueue, task, task->Weight))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    RunQueue->Runnable[RunQueue->RunnableCount] = task;
    RunQueue->RunnableCount++;

    task->CPUID = TargetCPUID;
    task->State = TASK_READY;
    task->Modified = FALSE;
    task->Scheduled = TRUE;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
    return TRUE;
}

BOOLEAN LBPWRRRemoveTask(TASK *task)
{
    UINT32           CPUID;
    UINT32           index;
    UINT64           flags;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL || !task->Scheduled)
    {
        return FALSE;
    }

    CPUID = task->CPUID;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return FALSE;
    }

    RunQueue = &RunQueues[CPUID];
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (!task->Scheduled || task->CPUID != CPUID || CurrentTasks[CPUID] == task || (task->State != TASK_TERMINATED && task->State != TASK_KILLED))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        if (RunQueue->Runnable[index] == task)
        {
            break;
        }
    }

    if (index == RunQueue->RunnableCount)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    if (!LBPWRRModifyChunk(RunQueue, task, 0))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    RunQueue->RunnableCount--;
    RunQueue->Runnable[index] = RunQueue->Runnable[RunQueue->RunnableCount];
    RunQueue->Runnable[RunQueue->RunnableCount] = NULL;

    task->Scheduled = FALSE;
    task->Modified = FALSE;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
    return TRUE;
}

BOOLEAN LBPWRRMoveTask(TASK *task, UINT32 TargetCPUID)
{
    UINT32           SourceCPUID;
    UINT32           CPUCount;
    UINT32           index;
    UINT32           SourceIndex;
    UINT32           LockCPUID;
    UINT32           OtherCPUID;
    UINT64           flags;
    LBPWRR_RUNQUEUE *SourceRunQueue;
    LBPWRR_RUNQUEUE *TargetRunQueue;

    if (task == NULL || !task->Scheduled)
    {
        return FALSE;
    }

    CPUCount = LBPWRRGetActiveCPUCount();
    SourceCPUID = task->CPUID;

    if (SourceCPUID >= CPUCount || TargetCPUID >= CPUCount || SourceCPUID == TargetCPUID)
    {
        return FALSE;
    }

    SourceRunQueue = &RunQueues[SourceCPUID];
    TargetRunQueue = &RunQueues[TargetCPUID];

    if (SourceCPUID < TargetCPUID)
    {
        LockCPUID = SourceCPUID;
        OtherCPUID = TargetCPUID;
    }
    else
    {
        LockCPUID = TargetCPUID;
        OtherCPUID = SourceCPUID;
    }

    flags = SpinLockAcquireIRQSave(&RunQueues[LockCPUID].Lock);
    SpinLockAcquire(&RunQueues[OtherCPUID].Lock);

    if (!task->Scheduled || task->CPUID != SourceCPUID || task->State != TASK_READY || CurrentTasks[SourceCPUID] == task || TargetRunQueue->RunnableCount >= LBPWRR_MAX_TASKS || task->Weight == 0 || task->Weight > 6)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);
        return FALSE;
    }

    SourceIndex = SourceRunQueue->RunnableCount;

    for (index = 0; index < SourceRunQueue->RunnableCount; index++)
    {
        if (SourceRunQueue->Runnable[index] == task)
        {
            SourceIndex = index;
            break;
        }
    }

    if (SourceIndex == SourceRunQueue->RunnableCount)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);
        return FALSE;
    }

    for (index = 0; index < TargetRunQueue->RunnableCount; index++)
    {
        if (TargetRunQueue->Runnable[index] == task)
        {
            SpinLockRelease(&RunQueues[OtherCPUID].Lock);
            SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);
            return FALSE;
        }
    }

    if (!LBPWRRModifyChunk(TargetRunQueue, task, task->Weight))
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);
        return FALSE;
    }

    if (!LBPWRRModifyChunk(SourceRunQueue, task, 0))
    {
        LBPWRRModifyChunk(TargetRunQueue, task, 0);

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);
        return FALSE;
    }

    SourceRunQueue->RunnableCount--;
    SourceRunQueue->Runnable[SourceIndex] = SourceRunQueue->Runnable[SourceRunQueue->RunnableCount];
    SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = NULL;

    TargetRunQueue->Runnable[TargetRunQueue->RunnableCount] = task;
    TargetRunQueue->RunnableCount++;

    task->CPUID = TargetCPUID;
    task->Modified = FALSE;

    SpinLockRelease(&RunQueues[OtherCPUID].Lock);
    SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);
    return TRUE;
}

BOOLEAN LBPWRRNotifyTaskChanged(TASK *task)
{
    UINT32           CPUID;
    UINT32           index;
    UINT64           flags;
    UINT8            weight;
    BOOLEAN          exists;
    BOOLEAN          result;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL || !task->Scheduled)
    {
        return FALSE;
    }

    CPUID = task->CPUID;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return FALSE;
    }

    RunQueue = &RunQueues[CPUID];
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (!task->Scheduled || task->CPUID != CPUID)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    exists = FALSE;

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        if (RunQueue->Runnable[index] == task)
        {
            exists = TRUE;
            break;
        }
    }

    if (!exists)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    weight = 0;

    if (task->State == TASK_READY || task->State == TASK_RUNNING)
    {
        weight = task->Weight;
    }

    result = LBPWRRModifyChunk(RunQueue, task, weight);

    if (result)
    {
        task->Modified = FALSE;
    }
    else
    {
        task->Modified = TRUE;
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
    return result;
}

TASK *LBPWRRGetTaskFromCPU(UINT32 CPUID)
{
    UINT32           index;
    UINT64           flags;
    TASK            *task;
    LBPWRR_RUNQUEUE *RunQueue;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    RunQueue = &RunQueues[CPUID];
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);
    task = NULL;

    for (index = RunQueue->RunnableCount; index > 0; index--)
    {
        TASK *candidate = RunQueue->Runnable[index - 1];

        if (candidate != NULL && candidate->State == TASK_READY && candidate != CurrentTasks[CPUID])
        {
            task = candidate;
            break;
        }
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
    return task;
}

TASK *LBPWRRGetCurrentTask(VOID)
{
    UINT32 CPUID;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    return CurrentTasks[CPUID];
}

UINT32 LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return 0;
    }

    return RunQueues[CPUID].RunnableCount + RunQueues[CPUID].UnrunnableCount;
}