// Kernel/Source/Scheduler/WaitQueue.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/WaitQueue.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/LBPWRRRunQueue.h>
#include <Scheduler/LBPWRRChunk.h>

STATIC SPIN_LOCK WaitLock;

STATIC VOID WaitQueueDetach(TASK *Task)
{
    WAIT_QUEUE *Queue;
    TASK       *Previous;
    TASK       *Current;

    Queue = Task->WaitQueue;

    if (Queue == NULL)
    {
        return;
    }

    Previous = NULL;
    Current = Queue->Head;

    while (Current != NULL && Current != Task)
    {
        Previous = Current;
        Current = Current->WaitNext;
    }

    if (Current != NULL)
    {
        if (Previous == NULL)
        {
            Queue->Head = Task->WaitNext;
        }
        else
        {
            Previous->WaitNext = Task->WaitNext;
        }

        if (Queue->Tail == Task)
        {
            Queue->Tail = Previous;
        }
    }

    Task->WaitDeadlineTSC = 0;
    Task->WaitQueue = NULL;
    Task->WaitNext = NULL;
}

VOID WaitQueueInit(WAIT_QUEUE *Queue)
{
    Queue->Head = NULL;
    Queue->Tail = NULL;
}

STATIC BOOLEAN WaitQueueSleepInternal(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock, BOOLEAN Interruptible, UINT64 Deadline, BOOLEAN *TimedOut)
{
    TASK            *Task;
    LBPWRR_RUNQUEUE *RunQueue;

    Task = LBPWRRGetCurrentTask();

    if (Queue == NULL || ConditionLock == NULL || Task == NULL || Task->Idle)
    {
        return FALSE;
    }

    RunQueue = LBPWRRGetRunQueue(Task->CPUID);
    SpinLockAcquire(&WaitLock);
    SpinLockAcquire(&RunQueue->Lock);

    if (!Task->Scheduled || Task->State != TASK_RUNNING || Task->WaitQueue != NULL || (Interruptible && ATOMIC_LOAD(&Task->Process->GroupExiting, ATOMIC_ACQUIRE)))
    {
        SpinLockRelease(&RunQueue->Lock);
        SpinLockRelease(&WaitLock);

        return FALSE;
    }

    Task->WaitDeadlineTSC = Deadline;
    Task->WaitTimedOut = FALSE;
    Task->WaitQueue = Queue;
    Task->WaitNext = NULL;

    if (Queue->Tail == NULL)
    {
        Queue->Head = Task;
    }
    else
    {
        Queue->Tail->WaitNext = Task;
    }

    Queue->Tail = Task;
    Task->State = TASK_BLOCKED;
    SpinLockRelease(&RunQueue->Lock);
    SpinLockRelease(&WaitLock);
    SpinLockRelease(ConditionLock);
    LBPWRRYield();
    SpinLockAcquire(ConditionLock);

    if (TimedOut != NULL)
    {
        *TimedOut = Task->WaitTimedOut;
    }

    return !Interruptible || !ATOMIC_LOAD(&Task->Process->GroupExiting, ATOMIC_ACQUIRE);
}

BOOLEAN WaitQueueSleep(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock)
{
    return WaitQueueSleepInternal(Queue, ConditionLock, TRUE, 0, NULL);
}

BOOLEAN WaitQueueSleepUninterruptible(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock)
{
    return WaitQueueSleepInternal(Queue, ConditionLock, FALSE, 0, NULL);
}

STATIC BOOLEAN WaitQueueReady(TASK *Task)
{
    WaitQueueDetach(Task);

    if (!Task->Scheduled || Task->State != TASK_BLOCKED)
    {
        return FALSE;
    }

    Task->State = LBPWRRGetCurrentTaskFromCPU(Task->CPUID) == Task ? TASK_RUNNING : TASK_READY;

    return TRUE;
}

STATIC LBPWRR_RUNQUEUE *WaitQueueLockTask(TASK *Task)
{
    while (Task->Scheduled)
    {
        UINT32           CPUID = Task->CPUID;
        LBPWRR_RUNQUEUE *RunQueue = LBPWRRGetRunQueue(CPUID);

        SpinLockAcquire(&RunQueue->Lock);

        if (Task->CPUID == CPUID)
        {
            return RunQueue;
        }

        SpinLockRelease(&RunQueue->Lock);
    }

    return NULL;
}

VOID WaitQueueInterruptTask(TASK *Task)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&WaitLock);

    if (Task->WaitQueue != NULL)
    {
        LBPWRR_RUNQUEUE *RunQueue = WaitQueueLockTask(Task);

        if (RunQueue != NULL)
        {
            WaitQueueReady(Task);
            SpinLockRelease(&RunQueue->Lock);
        }
    }

    SpinLockReleaseIRQRestore(&WaitLock, Flags);
}

UINT32 WaitQueueWakeMatching(WAIT_QUEUE *Queue, UINT32 Count, BOOLEAN (*Match)(TASK *, CONST VOID *), CONST VOID *Context)
{
    TASK            *Task;
    LBPWRR_RUNQUEUE *RunQueue;
    UINT64           Flags;
    UINT32           Woken = 0;

    Flags = SpinLockAcquireIRQSave(&WaitLock);

    for (TASK *Next = Queue->Head; Next != NULL && Woken < Count;)
    {
        Task = Next;
        Next = Task->WaitNext;

        if (Match != NULL && !Match(Task, Context))
        {
            continue;
        }

        RunQueue = WaitQueueLockTask(Task);

        if (RunQueue != NULL)
        {
            Woken += WaitQueueReady(Task);
            SpinLockRelease(&RunQueue->Lock);
        }
        else
        {
            WaitQueueDetach(Task);
        }
    }

    SpinLockReleaseIRQRestore(&WaitLock, Flags);

    return Woken;
}

VOID WaitQueueWakeOne(WAIT_QUEUE *Queue)
{
    WaitQueueWakeMatching(Queue, 1, NULL, NULL);
}

VOID WaitQueueWakeAll(WAIT_QUEUE *Queue)
{
    WaitQueueWakeMatching(Queue, (UINT32) -1, NULL, NULL);
}

VOID WaitQueueFinishTask(TASK *Task, TASK_STATE State)
{
    LBPWRR_RUNQUEUE *RunQueue;
    UINT64           Flags;

    Flags = SpinLockAcquireIRQSave(&WaitLock);
    RunQueue = WaitQueueLockTask(Task);

    WaitQueueDetach(Task);
    Task->State = State;
    Task->Modified = TRUE;

    if (RunQueue != NULL)
    {
        Task->Modified = !LBPWRRModifyChunk(RunQueue, Task, 0);
        SpinLockRelease(&RunQueue->Lock);
    }

    SpinLockReleaseIRQRestore(&WaitLock, Flags);
}

BOOLEAN WaitQueueSleepTimed(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock, UINT64 Deadline, BOOLEAN *TimedOut)
{
    return WaitQueueSleepInternal(Queue, ConditionLock, TRUE, Deadline, TimedOut);
}

VOID WaitQueueExpireCPU(UINT32 CPUID, UINT64 Now)
{
    LBPWRR_RUNQUEUE *RunQueue = LBPWRRGetRunQueue(CPUID);

    SpinLockAcquire(&WaitLock);
    SpinLockAcquire(&RunQueue->Lock);

    for (UINT32 Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        TASK *Task = RunQueue->Runnable[Index];

        if (Task->WaitDeadlineTSC != 0 && (INT64) (Now - Task->WaitDeadlineTSC) >= 0)
        {
            Task->WaitTimedOut = TRUE;
            WaitQueueReady(Task);
        }
    }

    SpinLockRelease(&RunQueue->Lock);
    SpinLockRelease(&WaitLock);
}
