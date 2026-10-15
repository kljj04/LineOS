
// kernel/src/scheduler/LBPWRR.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <scheduler/LBPWRRHelper.h>
#include <scheduler/task.h>
#include <multicore/SMP.h>

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

STATIC TASK *IdleTasks[SMP_MAX_CPUS];

BOOLEAN LBPWRRInit(VOID)
{
    UINT32 CPUCount;

    if (!TaskInit())
    {
        return FALSE;
    }

    CPUCount = SMPGetCPUCount();

    if (CPUCount == 0)
    {
        CPUCount = 1;
    }

    if (CPUCount > SMP_MAX_CPUS)
    {
        CPUCount = SMP_MAX_CPUS;
    }

    LBPWRRInitRunQueues(CPUCount);

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        IdleTasks[CPUID] = TaskCreateIdle();

        if (IdleTasks[CPUID] == NULL || !LBPWRRAddTask(IdleTasks[CPUID]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

VOID LBPWRRStart(VOID)
{
    UINT32           CPUID;
    UINT32           index;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *task;
    UINT64           flags;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    LBPWRRFlushRebuild(RunQueue, NULL);

    task = NULL;

    for (index = 0; index < RunQueue->Chunk.Count; index++)
    {
        TASK *Candidate;

        Candidate = RunQueue->Chunk.Tasks[index];

        if (Candidate == NULL || Candidate->State != TASK_READY || Candidate->Idle)
        {
            continue;
        }

        task = Candidate;
        RunQueue->CurrentIndex = index;
        break;
    }

    if (task == NULL)
    {
        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            TASK *Candidate;

            Candidate = RunQueue->Chunk.Tasks[index];

            if (Candidate == NULL || Candidate->State != TASK_READY || !Candidate->Idle)
            {
                continue;
            }

            task = Candidate;
            RunQueue->CurrentIndex = index;
            break;
        }
    }

    if (task == NULL)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return;
    }

    LBPWRRSetCurrentTask(CPUID, task);
    task->State = TASK_RUNNING;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

    LBPWRRRestoreContext(task->RSP);
}

VOID LBPWRRJoin(VOID)
{
    UINT32 CPUID;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    if (LBPWRRGetCurrentTaskFromCPU(CPUID) == NULL)
    {
        LBPWRRStart();
    }
}

VOID LBPWRRYield(VOID)
{
    ASM("int $0x43" ::: "memory");
}

UINT64 LBPWRRTick(INTERRUPT_FRAME *frame)
{
    UINT32           CPUID;
    UINT32           index;
    UINT32           StartIndex;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *CurrentTask;
    TASK            *NextTask;
    TASK            *Candidate;
    UINT64           flags;
    UINT64           NextRSP;

    if (frame == NULL)
    {
        return 0;
    }

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return (UINT64) frame;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    CurrentTask = LBPWRRGetCurrentTaskFromCPU(CPUID);

    if (LBPWRRWakeSleepingTasks(RunQueue))
    {
        LBPWRRRebuildChunk(RunQueue);
    }

    if (CurrentTask != NULL)
    {
        CurrentTask->RSP = (UINT64) frame;

        if (CurrentTask->State == TASK_RUNNING)
        {
            CurrentTask->State = TASK_READY;
        }
    }

    LBPWRRFlushRebuild(RunQueue, CurrentTask);

    NextTask = NULL;

    if (RunQueue->Chunk.Count > 0)
    {
        StartIndex = RunQueue->CurrentIndex;

        if (StartIndex >= RunQueue->Chunk.Count)
        {
            StartIndex = 0;
        }

        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            UINT32 CandidateIndex;

            if (CurrentTask == NULL)
            {
                CandidateIndex = (StartIndex + index) % RunQueue->Chunk.Count;
            }
            else
            {
                CandidateIndex = (StartIndex + index + 1) % RunQueue->Chunk.Count;
            }

            Candidate = RunQueue->Chunk.Tasks[CandidateIndex];

            if (Candidate == NULL || Candidate->State != TASK_READY || Candidate->Idle)
            {
                continue;
            }

            NextTask = Candidate;
            RunQueue->CurrentIndex = CandidateIndex;
            break;
        }
    }

    if (NextTask == NULL && RunQueue->Chunk.Count > 0)
    {
        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            UINT32 CandidateIndex;

            CandidateIndex = (StartIndex + index) % RunQueue->Chunk.Count;
            Candidate = RunQueue->Chunk.Tasks[CandidateIndex];

            if (Candidate == NULL || Candidate->State != TASK_READY || !Candidate->Idle)
            {
                continue;
            }

            NextTask = Candidate;
            RunQueue->CurrentIndex = CandidateIndex;
            break;
        }
    }

    if (NextTask == NULL)
    {
        if (CurrentTask != NULL && CurrentTask->State == TASK_READY)
        {
            CurrentTask->State = TASK_RUNNING;

            SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

            return (UINT64) frame;
        }

        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

        return (UINT64) frame;
    }

    LBPWRRSetCurrentTask(CPUID, NextTask);
    NextTask->State = TASK_RUNNING;
    NextRSP = NextTask->RSP;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

    return NextRSP;
}
