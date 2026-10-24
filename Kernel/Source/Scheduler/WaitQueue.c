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

    Task->WaitQueue = NULL;
    Task->WaitNext = NULL;
}

VOID WaitQueueInit(WAIT_QUEUE *Queue)
{
    Queue->Head = NULL;
    Queue->Tail = NULL;
}

BOOLEAN WaitQueueSleep(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock)
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

    if (!Task->Scheduled || Task->State != TASK_RUNNING || Task->WaitQueue != NULL)
    {
        SpinLockRelease(&RunQueue->Lock);
        SpinLockRelease(&WaitLock);
        return FALSE;
    }

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
    return TRUE;
}

STATIC VOID WaitQueueWake(WAIT_QUEUE *Queue, BOOLEAN All)
{
    TASK            *Task;
    LBPWRR_RUNQUEUE *RunQueue;
    UINT64           Flags;

    Flags = SpinLockAcquireIRQSave(&WaitLock);

    while (Queue->Head != NULL)
    {
        Task = Queue->Head;
        RunQueue = LBPWRRGetRunQueue(Task->CPUID);
        SpinLockAcquire(&RunQueue->Lock);
        WaitQueueDetach(Task);

        if (Task->Scheduled && Task->State == TASK_BLOCKED)
        {
            if (LBPWRRGetCurrentTaskFromCPU(Task->CPUID) == Task)
            {
                Task->State = TASK_RUNNING;
            }
            else
            {
                Task->State = TASK_READY;
            }
        }

        SpinLockRelease(&RunQueue->Lock);

        if (!All)
        {
            break;
        }
    }

    SpinLockReleaseIRQRestore(&WaitLock, Flags);
}

VOID WaitQueueWakeOne(WAIT_QUEUE *Queue)
{
    WaitQueueWake(Queue, FALSE);
}

VOID WaitQueueWakeAll(WAIT_QUEUE *Queue)
{
    WaitQueueWake(Queue, TRUE);
}

VOID WaitQueueFinishTask(TASK *Task, TASK_STATE State)
{
    LBPWRR_RUNQUEUE *RunQueue;
    UINT64           Flags;
    UINT32           CPUID;

    Flags = SpinLockAcquireIRQSave(&WaitLock);
    RunQueue = NULL;

    while (Task->Scheduled)
    {
        CPUID = Task->CPUID;
        RunQueue = LBPWRRGetRunQueue(CPUID);
        SpinLockAcquire(&RunQueue->Lock);

        if (Task->CPUID == CPUID)
        {
            break;
        }

        SpinLockRelease(&RunQueue->Lock);
        RunQueue = NULL;
    }

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
