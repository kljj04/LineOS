// Kernel/Source/Scheduler/LBPWRRRunQueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/LBPWRR.h>
#include <Scheduler/LBPWRRRunQueue.h>
#include <Scheduler/LBPWRRChunk.h>
#include <Multicore/SMP.h>
#include <Memory/PMM.h>

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

VOID LBPWRRSetCurrentTask(UINT32 CPUID, TASK *Task)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    CurrentTasks[CPUID] = Task;
}

BOOLEAN LBPWRRAddTask(TASK *Task)
{
    UINT32           CPUID;
    UINT32           Index;
    UINT32           TargetCPUID;
    UINT32           CPUCount;
    UINT64           Flags;
    LBPWRR_RUNQUEUE *RunQueue;

    if (Task == NULL || Task->Scheduled)
    {
        return FALSE;
    }

    if (Task->Weight == 0 || Task->Weight > 6)
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

    if (Task->Scheduled || RunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        if (RunQueue->Runnable[Index] == Task)
        {
            SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
            return FALSE;
        }
    }

    if (!LBPWRRModifyChunk(RunQueue, Task, Task->Weight))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    RunQueue->Runnable[RunQueue->RunnableCount] = Task;
    RunQueue->RunnableCount++;

    Task->CPUID = TargetCPUID;
    Task->State = TASK_READY;
    Task->Modified = FALSE;
    Task->Scheduled = TRUE;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return TRUE;
}

BOOLEAN LBPWRRRemoveTask(TASK *Task)
{
    UINT32           CPUID;
    UINT32           Index;
    UINT64           Flags;
    LBPWRR_RUNQUEUE *RunQueue;

    if (Task == NULL || !Task->Scheduled)
    {
        return FALSE;
    }

    CPUID = Task->CPUID;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return FALSE;
    }

    RunQueue = &RunQueues[CPUID];
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (!Task->Scheduled || Task->CPUID != CPUID || CurrentTasks[CPUID] == Task || Task->OnCPU || (Task->State != TASK_TERMINATED && Task->State != TASK_KILLED))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        if (RunQueue->Runnable[Index] == Task)
        {
            break;
        }
    }

    if (Index == RunQueue->RunnableCount)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    if (!LBPWRRModifyChunk(RunQueue, Task, 0))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    RunQueue->RunnableCount--;
    RunQueue->Runnable[Index] = RunQueue->Runnable[RunQueue->RunnableCount];
    RunQueue->Runnable[RunQueue->RunnableCount] = NULL;

    Task->Scheduled = FALSE;
    Task->Modified = FALSE;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return TRUE;
}

BOOLEAN LBPWRRMoveTask(TASK *Task, UINT32 TargetCPUID)
{
    UINT32           SourceCPUID;
    UINT32           CPUCount;
    UINT32           Index;
    UINT32           SourceIndex;
    UINT32           LockCPUID;
    UINT32           OtherCPUID;
    UINT64           Flags;
    LBPWRR_RUNQUEUE *SourceRunQueue;
    LBPWRR_RUNQUEUE *TargetRunQueue;

    if (Task == NULL || !Task->Scheduled)
    {
        return FALSE;
    }

    CPUCount = LBPWRRGetActiveCPUCount();
    SourceCPUID = Task->CPUID;

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

    if (!Task->Scheduled || Task->CPUID != SourceCPUID || Task->State != TASK_READY || Task->Idle || Task->OnCPU || CurrentTasks[SourceCPUID] == Task || TargetRunQueue->RunnableCount >= LBPWRR_MAX_TASKS || Task->Weight == 0 || Task->Weight > 6)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
        return FALSE;
    }

    SourceIndex = SourceRunQueue->RunnableCount;

    for (Index = 0; Index < SourceRunQueue->RunnableCount; Index++)
    {
        if (SourceRunQueue->Runnable[Index] == Task)
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

    for (Index = 0; Index < TargetRunQueue->RunnableCount; Index++)
    {
        if (TargetRunQueue->Runnable[Index] == Task)
        {
            SpinLockRelease(&RunQueues[OtherCPUID].Lock);
            SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
            return FALSE;
        }
    }

    if (!LBPWRRModifyChunk(TargetRunQueue, Task, Task->Weight))
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
        return FALSE;
    }

    if (!LBPWRRModifyChunk(SourceRunQueue, Task, 0))
    {
        LBPWRRModifyChunk(TargetRunQueue, Task, 0);

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
        return FALSE;
    }

    SourceRunQueue->RunnableCount--;
    SourceRunQueue->Runnable[SourceIndex] = SourceRunQueue->Runnable[SourceRunQueue->RunnableCount];
    SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = NULL;

    TargetRunQueue->Runnable[TargetRunQueue->RunnableCount] = Task;
    TargetRunQueue->RunnableCount++;

    Task->CPUID = TargetCPUID;
    Task->Modified = FALSE;

    SpinLockRelease(&RunQueues[OtherCPUID].Lock);
    SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);
    return TRUE;
}

BOOLEAN LBPWRRNotifyTaskChanged(TASK *Task)
{
    UINT32           CPUID;
    UINT32           Index;
    UINT64           Flags;
    UINT8            Weight;
    BOOLEAN          Exists;
    BOOLEAN          Result;
    LBPWRR_RUNQUEUE *RunQueue;

    if (Task == NULL || !Task->Scheduled)
    {
        return FALSE;
    }

    CPUID = Task->CPUID;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return FALSE;
    }

    RunQueue = &RunQueues[CPUID];
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (!Task->Scheduled || Task->CPUID != CPUID)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    Exists = FALSE;

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        if (RunQueue->Runnable[Index] == Task)
        {
            Exists = TRUE;
            break;
        }
    }

    if (!Exists)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return FALSE;
    }

    Weight = 0;

    if (Task->State == TASK_READY || Task->State == TASK_RUNNING || (Task->State == TASK_BLOCKED && Task->WaitQueue != NULL))
    {
        Weight = Task->Weight;
    }

    Result = LBPWRRModifyChunk(RunQueue, Task, Weight);

    if (Result)
    {
        Task->Modified = FALSE;
    }
    else
    {
        Task->Modified = TRUE;
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return Result;
}

TASK *LBPWRRGetTaskFromCPU(UINT32 CPUID)
{
    UINT32           Index;
    UINT64           Flags;
    TASK            *Task;
    LBPWRR_RUNQUEUE *RunQueue;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return NULL;
    }

    RunQueue = &RunQueues[CPUID];
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);
    Task = NULL;

    for (Index = RunQueue->RunnableCount; Index > 0; Index--)
    {
        TASK *Candidate = RunQueue->Runnable[Index - 1];

        if (Candidate != NULL && Candidate->State == TASK_READY && !Candidate->Idle && !Candidate->OnCPU && Candidate != CurrentTasks[CPUID])
        {
            Task = Candidate;
            break;
        }
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    return Task;
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