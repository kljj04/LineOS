// kernel/src/scheduler/lbpwrr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/lbpwrr.h>
#include <scheduler/task.h>
#include <multicore/smp.h>
#include <arch/x86_64/cpu.h>
#include <memory/memory.h>

STATIC LBPWRR_RUNQUEUE RunQueues[SMP_MAX_CPUS];
STATIC TASK           *CurrentTasks[SMP_MAX_CPUS];
STATIC CPU_USAGE       CPUUsages[SMP_MAX_CPUS];
STATIC UINT32          ActiveCPUCount;

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

STATIC VOID LBPWRRInitRunQueue(LBPWRR_RUNQUEUE *RunQueue)
{
    KMemSet(RunQueue, 0, sizeof(LBPWRR_RUNQUEUE));
    SpinLockInit(&RunQueue->Lock);
}

STATIC UINT32 LBPWRRGetActiveCPUCount(VOID)
{
    if (ActiveCPUCount == 0)
    {
        return 1;
    }

    return ActiveCPUCount;
}

STATIC BOOLEAN LBPWRRBuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    INT32  Scores[LBPWRR_MAX_TASKS];
    UINT32 TotalWeight;
    UINT32 Index;
    UINT32 ChunkIndex;
    UINT32 SelectedIndex;
    INT32  SelectedScore;
    UINT32 Weight;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    TotalWeight = 0;

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        Weight = RunQueue->Runnable[Index]->Weight;

        if (Weight == 0)
        {
            continue;
        }

        if (TotalWeight + Weight > CHUNK_MAX_COUNT)
        {
            return FALSE;
        }

        TotalWeight += Weight;
    }

    if (TotalWeight == 0)
    {
        RunQueue->Chunk.Count = 0;
        RunQueue->CurrentIndex = 0;

        return TRUE;
    }

    KMemSet(Scores, 0, sizeof(Scores));

    for (ChunkIndex = 0; ChunkIndex < TotalWeight; ChunkIndex++)
    {
        SelectedIndex = 0;
        SelectedScore = 0;

        for (Index = 0; Index < RunQueue->RunnableCount; Index++)
        {
            Weight = RunQueue->Runnable[Index]->Weight;

            if (Weight == 0)
            {
                continue;
            }

            Scores[Index] += (INT32)Weight;

            if (Scores[Index] > SelectedScore)
            {
                SelectedScore = Scores[Index];
                SelectedIndex = Index;
            }
        }

        RunQueue->Chunk.Tasks[ChunkIndex] =
            RunQueue->Runnable[SelectedIndex];

        Scores[SelectedIndex] -= (INT32)TotalWeight;
    }

    RunQueue->Chunk.Count = TotalWeight;
    RunQueue->CurrentIndex = 0;

    return TRUE;
}

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

    ActiveCPUCount = CPUCount;

    for (CPUID = 0; CPUID < ActiveCPUCount; CPUID++)
    {
        LBPWRRInitRunQueue(&RunQueues[CPUID]);
        CurrentTasks[CPUID] = NULL;
        KMemSet(&CPUUsages[CPUID], 0, sizeof(CPU_USAGE));
    }

    return TRUE;
}

BOOLEAN LBPWRRAddTask(TASK *task)
{
    UINT32           CPUID;
    UINT32           TargetCPUID;
    UINT32           CPUCount;
    LBPWRR_RUNQUEUE *RunQueue;

    if (task == NULL)
    {
        return FALSE;
    }

    CPUCount = LBPWRRGetActiveCPUCount();
    TargetCPUID = 0;

    for (CPUID = 1; CPUID < CPUCount; CPUID++)
    {
        if (RunQueues[CPUID].RunnableCount <
            RunQueues[TargetCPUID].RunnableCount)
        {
            TargetCPUID = CPUID;
        }
    }

    RunQueue = &RunQueues[TargetCPUID];

    if (RunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        return FALSE;
    }

    task->CPUID = TargetCPUID;
    task->State = TASK_READY;

    RunQueue->Runnable[RunQueue->RunnableCount] = task;
    RunQueue->RunnableCount++;

    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    if (!LBPWRRBuildChunk(RunQueue))
    {
        RunQueue->RunnableCount--;

        RunQueue->Generation++;
        RunQueue->RebuildPending = TRUE;

        return FALSE;
    }

    return TRUE;
}

VOID LBPWRRStart(VOID)
{
    UINT32           CPUID;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *task;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        CPUID = 0;
    }

    RunQueue = &RunQueues[CPUID];

    if (RunQueue->Chunk.Count == 0)
    {
        return;
    }

    if (RunQueue->CurrentIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }

    task = RunQueue->Chunk.Tasks[RunQueue->CurrentIndex];

    if (task == NULL)
    {
        return;
    }

    CurrentTasks[CPUID] = task;
    task->State = TASK_RUNNING;

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

    if (CurrentTasks[CPUID] == NULL)
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

    if (frame == NULL)
    {
        return 0;
    }

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return (UINT64)frame;
    }

    RunQueue = &RunQueues[CPUID];
    CurrentTask = CurrentTasks[CPUID];

    if (CurrentTask != NULL)
    {
        CurrentTask->RSP = (UINT64)frame;

        if (CurrentTask->State == TASK_RUNNING)
        {
            CurrentTask->State = TASK_READY;
        }
    }

    if (RunQueue->Chunk.Count == 0)
    {
        CurrentTasks[CPUID] = NULL;
        return (UINT64)frame;
    }

    RunQueue->CurrentIndex++;

    if (RunQueue->CurrentIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }

    NextTask = RunQueue->Chunk.Tasks[RunQueue->CurrentIndex];

    if (NextTask == NULL)
    {
        return (UINT64)frame;
    }

    CurrentTasks[CPUID] = NextTask;
    NextTask->State = TASK_RUNNING;

    return NextTask->RSP;
}

BOOLEAN LBPWRRMoveTask(TASK *task, UINT32 TargetCPUID)
{
    UINT32           SourceCPUID;
    UINT32           CPUCount;
    UINT32           Index;
    UINT32           LockCPUID;
    UINT32           OtherCPUID;
    UINT64           Flags;
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

    Flags = SpinLockAcquireIRQSave(&RunQueues[LockCPUID].Lock);
    SpinLockAcquire(&RunQueues[OtherCPUID].Lock);

    MovedTask = NULL;

    for (Index = 0; Index < SourceRunQueue->RunnableCount; Index++)
    {
        if (SourceRunQueue->Runnable[Index] == task)
        {
            MovedTask = SourceRunQueue->Runnable[Index];

            SourceRunQueue->Runnable[Index] =
                SourceRunQueue->Runnable[
                    SourceRunQueue->RunnableCount - 1
                ];

            SourceRunQueue->RunnableCount--;

            break;
        }
    }

    if (MovedTask == NULL)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(
            &RunQueues[LockCPUID].Lock,
            Flags
        );

        return FALSE;
    }

    if (TargetRunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SourceRunQueue->Runnable[
            SourceRunQueue->RunnableCount
        ] = MovedTask;

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(
            &RunQueues[LockCPUID].Lock,
            Flags
        );

        return FALSE;
    }

    TargetRunQueue->Runnable[
        TargetRunQueue->RunnableCount
    ] = MovedTask;

    TargetRunQueue->RunnableCount++;

    task->CPUID = TargetCPUID;

    SourceRunQueue->Generation++;
    TargetRunQueue->Generation++;

    SourceRunQueue->RebuildPending = TRUE;
    TargetRunQueue->RebuildPending = TRUE;

    SpinLockRelease(&RunQueues[OtherCPUID].Lock);
    SpinLockReleaseIRQRestore(
        &RunQueues[LockCPUID].Lock,
        Flags
    );

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

    return RunQueue->Runnable[
        RunQueue->RunnableCount - 1
    ];
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

UINTN LBPWRRGetCPUUsage(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return 0;
    }

    return CPUUsages[CPUID].CPUUsage;
}

UINT32 LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return 0;
    }

    return RunQueues[CPUID].RunnableCount +
           RunQueues[CPUID].UnrunnableCount;
}