// kernel/src/scheduler/lbpwrr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/lbpwrr.h>
#include <scheduler/task.h>
#include <multicore/smp.h>
#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <memory/memory.h>

STATIC LBPWRR_RUNQUEUE RunQueues[SMP_MAX_CPUS];
STATIC TASK            *CurrentTasks[SMP_MAX_CPUS];
STATIC CPU_USAGE        CPUUsages[SMP_MAX_CPUS];
STATIC UINT32           ActiveCPUCount;
STATIC UINT32           DebugSwitchCount;

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

STATIC VOID LBPWRRInitRunQueue(LBPWRR_RUNQUEUE *RunQueue)
{
    KMemSet(RunQueue, 0, sizeof(LBPWRR_RUNQUEUE));
}

STATIC UINT32 LBPWRRGetActiveCPUCount(VOID)
{
    if (ActiveCPUCount == 0)
    {
        return 1;
    }

    return ActiveCPUCount;
}

STATIC TASK *LBPWRRSelectRunnableTask(LBPWRR_RUNQUEUE *RunQueue, UINT32 StartIndex)
{
    UINT32 Count;
    UINT32 Index;
    TASK  *task;

    if (RunQueue == NULL || RunQueue->RunnableCount == 0)
    {
        return NULL;
    }

    for (Count = 0; Count < RunQueue->RunnableCount; Count++)
    {
        Index = StartIndex + Count;

        if (Index >= RunQueue->RunnableCount)
        {
            Index -= RunQueue->RunnableCount;
        }

        task = RunQueue->Runnable[Index];

        if (task == NULL)
        {
            continue;
        }

        if (task->State == TASK_READY || task->State == TASK_RUNNING)
        {
            RunQueue->CurrentIndex = Index;
            return task;
        }
    }

    return NULL;
}

STATIC UINT32 LBPWRRGetRunQueueIndex(LBPWRR_RUNQUEUE *RunQueue, TASK *task)
{
    UINT32 Index;

    if (RunQueue == NULL || task == NULL)
    {
        return UINT32_MAX;
    }

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        if (RunQueue->Runnable[Index] == task)
        {
            return Index;
        }
    }

    return UINT32_MAX;
}

STATIC VOID LBPWRRDebugSwitch(LBPWRR_RUNQUEUE *RunQueue, TASK *CurrentTask, TASK *NextTask, UINT32 StartIndex)
{
    if (DebugSwitchCount >= 16)
    {
        return;
    }

    DebugSwitchCount++;

    DebugWrite("SW cur=");
    DebugWriteHex((UINT64)CurrentTask);
    DebugWrite(" curi=");
    DebugWriteHex(LBPWRRGetRunQueueIndex(RunQueue, CurrentTask));
    DebugWrite(" curs=");
    DebugWriteHex(CurrentTask == NULL ? UINT64_MAX : CurrentTask->State);
    DebugWrite(" currsp=");
    DebugWriteHex(CurrentTask == NULL ? 0 : CurrentTask->RSP);
    DebugWrite(" rqidx=");
    DebugWriteHex(RunQueue->CurrentIndex);
    DebugWrite(" start=");
    DebugWriteHex(StartIndex);
    DebugWrite(" next=");
    DebugWriteHex((UINT64)NextTask);
    DebugWrite(" nexti=");
    DebugWriteHex(LBPWRRGetRunQueueIndex(RunQueue, NextTask));
    DebugWrite(" nexts=");
    DebugWriteHex(NextTask == NULL ? UINT64_MAX : NextTask->State);
    DebugWrite(" nextrsp=");
    DebugWriteHex(NextTask == NULL ? 0 : NextTask->RSP);
    DebugWrite("\n");
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
    DebugSwitchCount = 0;

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
        if (RunQueues[CPUID].RunnableCount < RunQueues[TargetCPUID].RunnableCount)
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

    if (RunQueue->RunnableCount == 0)
    {
        return;
    }

    if (RunQueue->CurrentIndex >= RunQueue->RunnableCount)
    {
        RunQueue->CurrentIndex = 0;
    }

    task = LBPWRRSelectRunnableTask(RunQueue, RunQueue->CurrentIndex);

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
UINT64  LBPWRRTick(INTERRUPT_FRAME *frame)
{
    UINT32           CPUID;
    UINT32           StartIndex;
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

    if (RunQueue->RunnableCount == 0)
    {
        CurrentTasks[CPUID] = NULL;
        return (UINT64)frame;
    }

    StartIndex = RunQueue->CurrentIndex;

    if (CurrentTask != NULL && RunQueue->RunnableCount > 1)
    {
        StartIndex++;

        if (StartIndex >= RunQueue->RunnableCount)
        {
            StartIndex = 0;
        }
    }

    NextTask = LBPWRRSelectRunnableTask(RunQueue, StartIndex);

    LBPWRRDebugSwitch(RunQueue, CurrentTask, NextTask, StartIndex);

    if (NextTask == NULL)
    {
        if (CurrentTask != NULL)
        {
            CurrentTask->State = TASK_RUNNING;
            return CurrentTask->RSP;
        }

        return (UINT64)frame;
    }

    CurrentTasks[CPUID] = NextTask;
    NextTask->State = TASK_RUNNING;

    return NextTask->RSP;
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

    return RunQueues[CPUID].RunnableCount + RunQueues[CPUID].UnrunnableCount;
}
