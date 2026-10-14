// kernel/src/scheduler/LBPWRRRunQueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRLoadRate.h>
#include <multicore/SMP.h>
#include <memory/memory.h>

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

    ActiveCPUCount = CPUCount;

    for (CPUID = 0; CPUID < ActiveCPUCount; CPUID++)
    {
        LBPWRRInitRunQueue(&RunQueues[CPUID]);
        CurrentTasks[CPUID] = NULL;
        LBPWRRInitCPUUsage(CPUID);
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
    UINT32           TargetCPUID;
    UINT32           CPUCount;
    UINT64           flags;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL)
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

    if (RunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return FALSE;
    }

    task->CPUID = TargetCPUID;
    task->State = TASK_READY;
    task->Modified = TRUE;

    RunQueue->Runnable[RunQueue->RunnableCount] = task;
    RunQueue->RunnableCount++;

    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    if (RunQueue->Chunk.Count == 0 && CurrentTasks[TargetCPUID] == NULL)
    {
        if (!LBPWRRBuildChunk(RunQueue))
        {
            RunQueue->RunnableCount--;
            RunQueue->Generation++;
            RunQueue->RebuildPending = TRUE;

            SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

            return FALSE;
        }

        RunQueue->RebuildPending = FALSE;
        task->Modified = FALSE;
    }
    else if (!LBPWRRRebuildChunk(RunQueue))
    {
        RunQueue->RunnableCount--;

        RunQueue->Generation++;
        RunQueue->RebuildPending = TRUE;

        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

        return FALSE;
    }

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
    TASK            *MovedTask;

    if (task == NULL)
    {
        return FALSE;
    }

    CPUCount = LBPWRRGetActiveCPUCount();

    if (TargetCPUID >= CPUCount)
    {
        return FALSE;
    }

    SourceCPUID = task->CPUID;

    if (SourceCPUID >= CPUCount)
    {
        return FALSE;
    }

    if (SourceCPUID == TargetCPUID)
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

    MovedTask = NULL;
    SourceIndex = LBPWRR_MAX_TASKS;

    for (index = 0; index < SourceRunQueue->RunnableCount; index++)
    {
        if (SourceRunQueue->Runnable[index] == task)
        {
            MovedTask = SourceRunQueue->Runnable[index];
            SourceIndex = index;

            SourceRunQueue->Runnable[index] = SourceRunQueue->Runnable[SourceRunQueue->RunnableCount - 1];

            SourceRunQueue->RunnableCount--;

            break;
        }
    }

    if (MovedTask == NULL)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);

        return FALSE;
    }

    if (TargetRunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = MovedTask;

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);

        return FALSE;
    }

    TargetRunQueue->Runnable[TargetRunQueue->RunnableCount] = MovedTask;

    TargetRunQueue->RunnableCount++;

    task->CPUID = TargetCPUID;
    task->Modified = TRUE;

    SourceRunQueue->Generation++;
    TargetRunQueue->Generation++;

    SourceRunQueue->RebuildPending = TRUE;
    TargetRunQueue->RebuildPending = TRUE;

    if (!LBPWRRRebuildChunk(SourceRunQueue) || !LBPWRRRebuildChunk(TargetRunQueue))
    {
        TargetRunQueue->RunnableCount--;

        if (SourceIndex < SourceRunQueue->RunnableCount)
        {
            SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = SourceRunQueue->Runnable[SourceIndex];
            SourceRunQueue->Runnable[SourceIndex] = MovedTask;
        }
        else
        {
            SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = MovedTask;
        }

        SourceRunQueue->RunnableCount++;

        task->CPUID = SourceCPUID;
        task->Modified = TRUE;

        SourceRunQueue->Generation++;
        TargetRunQueue->Generation++;

        SourceRunQueue->RebuildPending = TRUE;
        TargetRunQueue->RebuildPending = TRUE;

        LBPWRRRebuildChunk(SourceRunQueue);
        LBPWRRRebuildChunk(TargetRunQueue);

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);

        return FALSE;
    }

    SpinLockRelease(&RunQueues[OtherCPUID].Lock);
    SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, flags);

    return TRUE;
}

BOOLEAN LBPWRRNotifyTaskChanged(TASK *task)
{
    UINT32           CPUID;
    UINT64           flags;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL)
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

    task->Modified = TRUE;
    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    LBPWRRRebuildChunk(RunQueue);

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

    return TRUE;
}

TASK *LBPWRRGetTaskFromCPU(UINT32 CPUID)
{
    LBPWRR_RUNQUEUE *RunQueue;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    RunQueue = &RunQueues[CPUID];

    if (RunQueue->RunnableCount == 0)
    {
        return NULL;
    }

    return RunQueue->Runnable[RunQueue->RunnableCount - 1];
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
