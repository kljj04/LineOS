// kernel/src/scheduler/LBPWRR.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRChunk.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <scheduler/LBPWRRLoadRate.h>
#include <scheduler/LBPWRRHelper.h>
#include <scheduler/task.h>
#include <multicore/SMP.h>

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

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

    return TRUE;
}

VOID LBPWRRStart(VOID)
{
    UINT32           CPUID;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *task;
    UINT64           flags;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        CPUID = 0;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    LBPWRRFlushRebuild(RunQueue, LBPWRRGetCurrentTaskFromCPU(CPUID));

    if (RunQueue->Chunk.Count == 0)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return;
    }

    if (RunQueue->CurrentIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }

    task = RunQueue->Chunk.Tasks[RunQueue->CurrentIndex];

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
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *CurrentTask;
    TASK            *NextTask;
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

    LBPWRRAccountCPUTime(CPUID, CurrentTask != NULL);
    LBPWRRWakeSleepingTasks(RunQueue);

    if (CurrentTask != NULL)
    {
        CurrentTask->RSP = (UINT64) frame;

        if (CurrentTask->State == TASK_RUNNING)
        {
            CurrentTask->State = TASK_READY;
        }
    }

    LBPWRRFlushRebuild(RunQueue, CurrentTask);

    if (RunQueue->Chunk.Count == 0)
    {
        LBPWRRSetCurrentTask(CPUID, NULL);
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return (UINT64) frame;
    }

    RunQueue->CurrentIndex++;

    if (RunQueue->CurrentIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }

    NextTask = RunQueue->Chunk.Tasks[RunQueue->CurrentIndex];

    if (NextTask == NULL)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);
        return (UINT64) frame;
    }

    LBPWRRSetCurrentTask(CPUID, NextTask);
    NextTask->State = TASK_RUNNING;
    NextRSP = NextTask->RSP;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

    return NextRSP;
}
