// kernel/src/scheduler/lbpwrr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/task.h>
#include <multicore/SMP.h>
#include <arch/AMD64/CPU.h>
#include <memory/memory.h>

STATIC LBPWRR_RUNQUEUE RunQueues[SMP_MAX_CPUS];
STATIC TASK           *CurrentTasks[SMP_MAX_CPUS];
STATIC CPU_USAGE       CPUUsages[SMP_MAX_CPUS];
STATIC UINT32          ActiveCPUCount;

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

STATIC BOOLEAN LBPWRRTaskIsRunnable(TASK *task);
STATIC BOOLEAN LBPWRRRebuildChunk(LBPWRR_RUNQUEUE *RunQueue);

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

STATIC VOID LBPWRRAccountCPUTime(UINT32 CPUID, BOOLEAN Busy)
{
    UINT64 Now;
    UINT64 Delta;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    Now = RDTSC();

    if (CPUUsages[CPUID].LastTSC == 0)
    {
        CPUUsages[CPUID].LastTSC = Now;
        return;
    }

    Delta = Now - CPUUsages[CPUID].LastTSC;
    CPUUsages[CPUID].LastTSC = Now;

    if (Busy)
    {
        CPUUsages[CPUID].BusyTSC += Delta;
    }
    else
    {
        CPUUsages[CPUID].IdleTSC += Delta;
    }
}

STATIC VOID LBPWRRSampleCPUUsage(UINT32 CPUID, BOOLEAN Busy)
{
    UINT64 BusyTSC;
    UINT64 IdleTSC;
    UINT64 BusyDelta;
    UINT64 IdleDelta;
    UINT64 TotalDelta;
    UINT64 Now;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    BusyTSC = CPUUsages[CPUID].BusyTSC;
    IdleTSC = CPUUsages[CPUID].IdleTSC;
    Now = RDTSC();

    if (CPUUsages[CPUID].LastTSC != 0)
    {
        if (Busy)
        {
            BusyTSC += Now - CPUUsages[CPUID].LastTSC;
        }
        else
        {
            IdleTSC += Now - CPUUsages[CPUID].LastTSC;
        }
    }

    BusyDelta = BusyTSC - CPUUsages[CPUID].LastSampleBusyTSC;
    IdleDelta = IdleTSC - CPUUsages[CPUID].LastSampleIdleTSC;
    TotalDelta = BusyDelta + IdleDelta;

    if (TotalDelta == 0)
    {
        return;
    }

    CPUUsages[CPUID].CPUUsage = (UINTN) ((BusyDelta * 100ULL) / TotalDelta);
    CPUUsages[CPUID].LastSampleBusyTSC = BusyTSC;
    CPUUsages[CPUID].LastSampleIdleTSC = IdleTSC;
}

STATIC BOOLEAN LBPWRRWakeSleepingTasks(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32  Index;
    UINT64  Now;
    TASK   *task;
    BOOLEAN WokeTask;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    Now = RDTSC();
    WokeTask = FALSE;

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        task = RunQueue->Runnable[Index];

        if (task == NULL || task->State != TASK_SLEEPING)
        {
            continue;
        }

        if ((INT64) (Now - task->WakeDeadlineTSC) < 0)
        {
            continue;
        }

        task->WakeDeadlineTSC = 0;
        task->State = TASK_READY;
        task->Modified = TRUE;
        WokeTask = TRUE;
    }

    if (WokeTask)
    {
        RunQueue->Generation++;
        RunQueue->RebuildPending = TRUE;
        LBPWRRRebuildChunk(RunQueue);
    }

    return WokeTask;
}

STATIC BOOLEAN LBPWRRTaskIsRunnable(TASK *task)
{
    if (task == NULL)
    {
        return FALSE;
    }

    return task->State == TASK_READY || task->State == TASK_RUNNING;
}

STATIC BOOLEAN LBPWRRBuildChunkInto(LBPWRR_RUNQUEUE *RunQueue, CHUNK *Chunk)
{
    INT32  scores[LBPWRR_MAX_TASKS];
    UINT32 TotalWeight;
    UINT32 index;
    UINT32 ChunkIndex;
    UINT32 SelectedIndex;
    INT32  SelectedScore;
    UINT32 weight;
    TASK  *task;

    if (RunQueue == NULL || Chunk == NULL)
    {
        return FALSE;
    }

    TotalWeight = 0;

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        task = RunQueue->Runnable[index];

        if (!LBPWRRTaskIsRunnable(task))
        {
            continue;
        }

        weight = task->Weight;

        if (weight == 0)
        {
            continue;
        }

        if (TotalWeight + weight > CHUNK_MAX_COUNT)
        {
            return FALSE;
        }

        TotalWeight += weight;
    }

    if (TotalWeight == 0)
    {
        Chunk->Count = 0;

        return TRUE;
    }

    KMemSet(scores, 0, sizeof(scores));

    for (ChunkIndex = 0; ChunkIndex < TotalWeight; ChunkIndex++)
    {
        SelectedIndex = 0;
        SelectedScore = 0;

        for (index = 0; index < RunQueue->RunnableCount; index++)
        {
            task = RunQueue->Runnable[index];

            if (!LBPWRRTaskIsRunnable(task))
            {
                continue;
            }

            weight = task->Weight;

            if (weight == 0)
            {
                continue;
            }

            scores[index] += (INT32) weight;

            if (scores[index] > SelectedScore)
            {
                SelectedScore = scores[index];
                SelectedIndex = index;
            }
        }

        Chunk->Tasks[ChunkIndex] = RunQueue->Runnable[SelectedIndex];

        scores[SelectedIndex] -= (INT32) TotalWeight;
    }

    Chunk->Count = TotalWeight;

    return TRUE;
}

STATIC BOOLEAN LBPWRRBuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    if (!LBPWRRBuildChunkInto(RunQueue, &RunQueue->Chunk))
    {
        return FALSE;
    }

    RunQueue->CurrentIndex = 0;

    return TRUE;
}

STATIC BOOLEAN CheckRebuildPending(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32 index;
    UINT32 Count;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    Count = RunQueue->RunnableCount;

    if (RunQueue->UnrunnableCount > Count)
    {
        Count = RunQueue->UnrunnableCount;
    }

    for (index = 0; index < Count; index++)
    {
        if ((index < RunQueue->RunnableCount && RunQueue->Runnable[index]->Modified) || (index < RunQueue->UnrunnableCount && RunQueue->Unrunnable[index]->Modified))
        {
            RunQueue->RebuildPending = TRUE;
            break;
        }
    }

    return RunQueue->RebuildPending;
}

STATIC BOOLEAN LBPWRRRebuildChunk(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32 index;
    UINT32 Count;
    UINT64 BuildGeneration;

    if (CheckRebuildPending(RunQueue) == FALSE)
    {
        return FALSE;
    }

    if (RunQueue->RebuildRunning)
    {
        return TRUE;
    }

    RunQueue->RebuildRunning = TRUE;
    BuildGeneration = RunQueue->Generation;

    if (!LBPWRRBuildChunkInto(RunQueue, &RunQueue->BuildChunk))
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildRunning = FALSE;
        RunQueue->RebuildPending = TRUE;

        return FALSE;
    }

    RunQueue->BuildGeneration = BuildGeneration;
    RunQueue->FlushReady = TRUE;
    RunQueue->RebuildPending = FALSE;
    RunQueue->RebuildRunning = FALSE;

    Count = RunQueue->RunnableCount;

    if (RunQueue->UnrunnableCount > Count)
    {
        Count = RunQueue->UnrunnableCount;
    }

    for (index = 0; index < Count; index++)
    {
        if (index < RunQueue->RunnableCount && RunQueue->Runnable[index] != NULL)
        {
            RunQueue->Runnable[index]->Modified = FALSE;
        }

        if (index < RunQueue->UnrunnableCount && RunQueue->Unrunnable[index] != NULL)
        {
            RunQueue->Unrunnable[index]->Modified = FALSE;
        }
    }

    return TRUE;
}

STATIC VOID LBPWRRFlushRebuild(LBPWRR_RUNQUEUE *RunQueue, TASK *CurrentTask)
{
    UINT32 index;
    UINT32 OldIndex;

    if (RunQueue == NULL || !RunQueue->FlushReady)
    {
        return;
    }

    if (RunQueue->BuildGeneration != RunQueue->Generation)
    {
        RunQueue->FlushReady = FALSE;
        RunQueue->RebuildPending = TRUE;
        LBPWRRRebuildChunk(RunQueue);

        return;
    }

    OldIndex = RunQueue->CurrentIndex;
    RunQueue->Chunk = RunQueue->BuildChunk;
    RunQueue->FlushReady = FALSE;

    if (RunQueue->Chunk.Count == 0)
    {
        RunQueue->CurrentIndex = 0;
        return;
    }

    if (CurrentTask != NULL)
    {
        for (index = 0; index < RunQueue->Chunk.Count; index++)
        {
            if (RunQueue->Chunk.Tasks[index] == CurrentTask)
            {
                RunQueue->CurrentIndex = index;
                return;
            }
        }
    }

    if (OldIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }
    else
    {
        RunQueue->CurrentIndex = OldIndex;
    }
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
        CPUUsages[CPUID].LastTSC = RDTSC();
    }

    return TRUE;
}

BOOLEAN LBPWRRAddTask(TASK *task)
{
    UINT32           CPUID;
    UINT32           TargetCPUID;
    UINT32           CPUCount;
    UINT64           Flags;
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
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    if (RunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
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

            SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

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

        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

        return FALSE;
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

    return TRUE;
}

VOID LBPWRRStart(VOID)
{
    UINT32           CPUID;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *task;
    UINT64           Flags;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        CPUID = 0;
    }

    RunQueue = &RunQueues[CPUID];
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    LBPWRRFlushRebuild(RunQueue, CurrentTasks[CPUID]);

    if (RunQueue->Chunk.Count == 0)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return;
    }

    if (RunQueue->CurrentIndex >= RunQueue->Chunk.Count)
    {
        RunQueue->CurrentIndex = 0;
    }

    task = RunQueue->Chunk.Tasks[RunQueue->CurrentIndex];

    if (task == NULL)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return;
    }

    CurrentTasks[CPUID] = task;
    task->State = TASK_RUNNING;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

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
    UINT64           Flags;
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

    RunQueue = &RunQueues[CPUID];
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);
    CurrentTask = CurrentTasks[CPUID];

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
        CurrentTasks[CPUID] = NULL;
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
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
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        return (UINT64) frame;
    }

    CurrentTasks[CPUID] = NextTask;
    NextTask->State = TASK_RUNNING;
    NextRSP = NextTask->RSP;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

    return NextRSP;
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
    SourceIndex = LBPWRR_MAX_TASKS;

    for (Index = 0; Index < SourceRunQueue->RunnableCount; Index++)
    {
        if (SourceRunQueue->Runnable[Index] == task)
        {
            MovedTask = SourceRunQueue->Runnable[Index];
            SourceIndex = Index;

            SourceRunQueue->Runnable[Index] = SourceRunQueue->Runnable[SourceRunQueue->RunnableCount - 1];

            SourceRunQueue->RunnableCount--;

            break;
        }
    }

    if (MovedTask == NULL)
    {
        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);

        return FALSE;
    }

    if (TargetRunQueue->RunnableCount >= LBPWRR_MAX_TASKS)
    {
        SourceRunQueue->Runnable[SourceRunQueue->RunnableCount] = MovedTask;

        SpinLockRelease(&RunQueues[OtherCPUID].Lock);
        SpinLockReleaseIRQRestore(&RunQueues[LockCPUID].Lock, Flags);

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

    task->Modified = TRUE;
    RunQueue->Generation++;
    RunQueue->RebuildPending = TRUE;

    LBPWRRRebuildChunk(RunQueue);

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

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

UINTN LBPWRRGetCPUUsage(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return 0;
    }

    LBPWRRSampleCPUUsage(CPUID, CurrentTasks[CPUID] != NULL);

    return CPUUsages[CPUID].CPUUsage;
}

UINT32 LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID)
{
    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return 0;
    }

    return RunQueues[CPUID].RunnableCount + RunQueues[CPUID].UnrunnableCount;
}
