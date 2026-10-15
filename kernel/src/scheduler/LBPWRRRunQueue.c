// kernel/src/scheduler/LBPWRRRunQueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <scheduler/LBPWRRChunk.h>
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
    UINT32           Index;
    UINT32           TargetCPUID;
    UINT32           CPUCount;
    UINT32           OldCPUID;
    UINT64           Flags;
    TASK_STATE       OldState;
    BOOLEAN          OldModified;
    BOOLEAN          OldScheduled;
    BOOLEAN          BuildSucceeded;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL || task->Scheduled)
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
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (RunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    // Prevent inserting the same task twice into this run queue.
    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        if (RunQueue->Runnable[Index] == task)
        {
            SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
            return FALSE;
        }
    }

    OldCPUID = task->CPUID;
    OldState = task->State;
    OldModified = task->Modified;
    OldScheduled = task->Scheduled;

    task->CPUID = TargetCPUID;
    task->State = TASK_READY;
    task->Modified = TRUE;

    RunQueue->Runnable[RunQueue->RunnableCount] = task;
    RunQueue->RunnableCount++;
    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    if (RunQueue->Chunk.Count == 0 && CurrentTasks[TargetCPUID] == NULL)
    {
        BuildSucceeded = LBPWRRBuildChunk(RunQueue);

        if (BuildSucceeded)
        {
            RunQueue->RebuildPending = FALSE;
            task->Modified = FALSE;
        }
    }
    else
    {
        BuildSucceeded = LBPWRRRebuildChunk(RunQueue);
    }

    if (!BuildSucceeded)
    {
        RunQueue->RunnableCount--;
        RunQueue->Runnable[RunQueue->RunnableCount] = NULL;

        task->CPUID = OldCPUID;
        task->State = OldState;
        task->Modified = OldModified;
        task->Scheduled = OldScheduled;

        RunQueue->Generation++;
        RunQueue->RebuildPending = TRUE;

        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    task->Scheduled = TRUE;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return TRUE;
}

BOOLEAN LBPWRRRemoveTask(TASK *task)
{
    UINT32           CPUID;
    UINT32           Index;
    UINT64           Flags;
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
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (CurrentTasks[CPUID] == task ||
        (task->State != TASK_TERMINATED && task->State != TASK_KILLED))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        if (RunQueue->Runnable[Index] == task)
        {
            break;
        }
    }

    if (Index == RunQueue->RunnableCount)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    RunQueue->RunnableCount--;
    RunQueue->Runnable[Index] = RunQueue->Runnable[RunQueue->RunnableCount];
    RunQueue->Runnable[RunQueue->RunnableCount] = NULL;
    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    if (!LBPWRRRebuildChunk(RunQueue))
    {
        RunQueue->Runnable[RunQueue->RunnableCount] = RunQueue->Runnable[Index];
        RunQueue->Runnable[Index] = task;
        RunQueue->RunnableCount++;
        RunQueue->Generation++;
        RunQueue->RebuildPending = TRUE;
        LBPWRRRebuildChunk(RunQueue);

        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    LBPWRRFlushRebuild(RunQueue, NULL);
    task->Scheduled = FALSE;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return TRUE;
}

BOOLEAN LBPWRRMoveTask(TASK *task, UINT32 TargetCPUID)
{
    UINT32           SourceCPUID;
    UINT32           CPUCount;
    UINT32           Index;
    UINT32           SourceIndex;
    UINT32           LockCPUID;
    UINT32           OtherCPUID;
    UINT64           Flags;
    BOOLEAN          OldModified;
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

    Flags = SpinLockAcquireIRQSave(&RunQueues[LockCPUID].Lock);
    SpinLockAcquire(&RunQueues[OtherCPUID].Lock);

    // Only a ready, non-running task can move. Check again after locking.
    if (task->CPUID != SourceCPUID || task->State != TASK_READY ||
        CurrentTasks[SourceCPUID] == task || TargetRunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
        return FALSE;
    }

    SourceIndex = SourceRunQueue->RunnableCount;

    for (Index = 0; Index < SourceRunQueue->RunnableCount; Index++)
    {
        if (SourceRunQueue->Runnable[Index] == task)
        {
            SourceIndex = Index;
            break;
        }
    }

    if (SourceIndex == SourceRunQueue->RunnableCount)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
        return FALSE;
    }

    // Capacity and state checks happen before touching either queue.
    OldModified = task->Modified;
    SourceRunQueue->RunnableCount--;
    SourceRunQueue->Runnable[SourceIndex] = SourceRunQueue->Runnable[SourceRunQueue->RunnableCount];
    SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = NULL;

    TargetRunQueue->Runnable[TargetRunQueue->RunnableCount] = task;
    TargetRunQueue->RunnableCount++;

    task->CPUID = TargetCPUID;
    task->Modified = TRUE;

    SourceRunQueue->Generation++;
    TargetRunQueue->Generation++;
    SourceRunQueue->RebuildPending = TRUE;
    TargetRunQueue->RebuildPending = TRUE;

    if (!LBPWRRRebuildChunk(SourceRunQueue) || !LBPWRRRebuildChunk(TargetRunQueue))
    {
        // Restore both arrays, counts and the task owner if rebuilding fails.
        TargetRunQueue->RunnableCount--;
        TargetRunQueue->Runnable[TargetRunQueue->RunnableCount] = NULL;

        if (SourceIndex < SourceRunQueue->RunnableCount)
        {
            SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = SourceRunQueue->Runnable[SourceIndex];
        }

        SourceRunQueue->Runnable[SourceIndex] = task;
        SourceRunQueue->RunnableCount++;

        task->CPUID = SourceCPUID;
        task->Modified = OldModified;

        SourceRunQueue->Generation++;
        TargetRunQueue->Generation++;
        SourceRunQueue->RebuildPending = TRUE;
        TargetRunQueue->RebuildPending = TRUE;

        LBPWRRRebuildChunk(SourceRunQueue);
        LBPWRRRebuildChunk(TargetRunQueue);

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
        return FALSE;
    }

    SpinLockRelease(&RunQueues[OtherCPUID].Lock);
    SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
    return TRUE;
}

BOOLEAN LBPWRRNotifyTaskChanged(TASK *task)
{
    UINT32           CPUID;
    UINT64           Flags;
    BOOLEAN          Rebuilt;
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
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (task->CPUID != CPUID)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    task->Modified = TRUE;
    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    Rebuilt = LBPWRRRebuildChunk(RunQueue);

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return Rebuilt;
}

TASK *LBPWRRGetTaskFromCPU(UINT32 CPUID)
{
    UINT32           Index;
    UINT64           Flags;
    TASK            *task;
    LBPWRR_RUNQUEUE *RunQueue;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    RunQueue = &RunQueues[CPUID];
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);
    task = NULL;

    for (Index = RunQueue->RunnableCount; Index > 0; Index--)
    {
        TASK *Candidate = RunQueue->Runnable[Index - 1];

        if (Candidate != NULL && Candidate->State == TASK_READY && Candidate != CurrentTasks[CPUID])
        {
            task = Candidate;
            break;
        }
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
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
