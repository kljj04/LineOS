// kernel/src/scheduler/LBPWRR.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>

#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <scheduler/LBPWRRHelper.h>
#include <scheduler/task.h>
#include <multicore/SMP.h>
#include <memory/memory.h>
#include <timer/TSC.h>
#include <arch/AMD64/CPU.h>

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

STATIC TASK *IdleTasks[SMP_MAX_CPUS];

BOOLEAN LBPWRRInit(VOID)
{
    UINT32 CPUCount;
    UINT32 CPUID;

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

    for (CPUID = 0; CPUID < CPUCount; CPUID++)
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

    task = NULL;

    for (index = 0; index < RunQueue->Chunk.Count; index++)
    {
        TASK *candidate;

        candidate = RunQueue->Chunk.Tasks[index];

        if (candidate == NULL || candidate->State != TASK_READY || candidate->Idle)
        {
            continue;
        }

        task = candidate;
        RunQueue->CurrentIndex = index;
        break;
    }

    if (task == NULL)
    {
        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            TASK *candidate;

            candidate = RunQueue->Chunk.Tasks[index];

            if (candidate == NULL || candidate->State != TASK_READY || !candidate->Idle)
            {
                continue;
            }

            task = candidate;
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

    TSCSetDeadline(RDTSC() + TSCGetFrequency() / 200);

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
    UINT32           Index;
    UINT32           StartIndex;
    UINT32           CandidateIndex;
    UINT32           ChunkCount;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *CurrentTask;
    TASK            *NextTask;
    TASK            *Candidate;
    UINT64           Flags;
    UINT64           NextRSP;

    if (frame == NULL)
    {
        return 0;
    }

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount() || CPUID >= SMP_MAX_CPUS)
    {
        return (UINT64) frame;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);

    if (RunQueue == NULL)
    {
        return (UINT64) frame;
    }

    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    CurrentTask = LBPWRRGetCurrentTaskFromCPU(CPUID);

    LBPWRRWakeSleepingTasks(RunQueue);

    if (CurrentTask != NULL)
    {
        CurrentTask->RSP = (UINT64) frame;

        if (CurrentTask->State == TASK_RUNNING)
        {
            CurrentTask->State = TASK_READY;
        }
    }

    NextTask = NULL;
    ChunkCount = RunQueue->Chunk.Count;
    StartIndex = RunQueue->CurrentIndex;

    if (StartIndex >= ChunkCount)
    {
        StartIndex = 0;
    }

    if (ChunkCount > 0)
    {
        for (Index = 0; Index < ChunkCount; Index++)
        {
            if (CurrentTask == NULL)
            {
                CandidateIndex = (StartIndex + Index) % ChunkCount;
            }
            else
            {
                CandidateIndex = (StartIndex + Index + 1) % ChunkCount;
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

    if (NextTask == NULL && ChunkCount > 0)
    {
        for (Index = 0; Index < ChunkCount; Index++)
        {
            CandidateIndex = (StartIndex + Index) % ChunkCount;

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

            NextTask = CurrentTask;
            NextRSP = (UINT64) frame;
        }
        else
        {
            NextRSP = (UINT64) frame;
        }
    }
    else
    {
        LBPWRRSetCurrentTask(CPUID, NextTask);

        NextTask->State = TASK_RUNNING;
        NextRSP = NextTask->RSP;
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

    return NextRSP;
}
