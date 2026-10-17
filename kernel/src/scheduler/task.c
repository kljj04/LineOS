// kernel/src/scheduler/task.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/task.h>
#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRTypes.h>
#include <arch/AMD64/CPU.h>
#include <debug/debug.h>
#include <memory/PMM.h>
#include <timer/TSC.h>

STATIC TASK    TaskPool[LBPWRR_MAX_TASKS];
STATIC BOOLEAN TaskUsed[LBPWRR_MAX_TASKS];

STATIC UINTN  TaskCount;
STATIC UINT16 NextPID;

STATIC NORETURN VOID TaskExit(VOID)
{
    TASK *Task;

    Task = LBPWRRGetCurrentTask();

    if (Task != NULL)
    {
        TaskTerminate(Task);
    }

    for (;;)
    {
        LBPWRRYield();
    }
}

STATIC NORETURN VOID TaskIdle(VOID)
{
    while (TRUE)
    {
        ASM("sti; hlt" ::: "memory");
    }
}

TASK *TaskCreateIdle(VOID)
{
    TASK *Task;

    Task = TaskCreate(TaskIdle);

    if (Task != NULL)
    {
        Task->Idle = TRUE;
    }

    return Task;
}

STATIC BOOLEAN TaskGetIndex(TASK *Task, UINTN *TaskIndex)
{
    UINTN Index;

    if (Task == NULL || TaskIndex == NULL)
    {
        return FALSE;
    }

    for (Index = 0; Index < LBPWRR_MAX_TASKS; Index++)
    {
        if (&TaskPool[Index] == Task)
        {
            *TaskIndex = Index;
            return TRUE;
        }
    }

    return FALSE;
}

STATIC UINT8 PToW(UINT8 Priority)
{
    if (Priority < 1 || Priority > 6)
    {
        return 0;
    }

    return Priority;
}

STATIC TASK *TaskAllocate(VOID)
{
    UINTN Index;

    for (Index = 0; Index < LBPWRR_MAX_TASKS; Index++)
    {
        if (!TaskUsed[Index])
        {
            TaskUsed[Index] = TRUE;
            KMemSet(&TaskPool[Index], 0, sizeof(TASK));

            return &TaskPool[Index];
        }
    }

    return NULL;
}

STATIC BOOLEAN TaskPIDUsed(UINT16 PID)
{
    UINTN Index;

    if (PID == 0)
    {
        return TRUE;
    }

    for (Index = 0; Index < LBPWRR_MAX_TASKS; Index++)
    {
        if (!TaskUsed[Index])
        {
            continue;
        }

        if (TaskPool[Index].PID == PID)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC BOOLEAN TaskAllocatePID(UINT16 *PID)
{
    UINT32 Retry;
    UINT16 Candidate;

    if (PID == NULL)
    {
        return FALSE;
    }

    Candidate = NextPID;

    for (Retry = 0; Retry < 0xFFFF; Retry++)
    {
        if (Candidate == 0)
        {
            Candidate = 1;
        }

        if (!TaskPIDUsed(Candidate))
        {
            *PID = Candidate;

            Candidate++;

            if (Candidate == 0)
            {
                Candidate = 1;
            }

            NextPID = Candidate;

            return TRUE;
        }

        Candidate++;
    }

    return FALSE;
}

STATIC VOID TaskFreeSlot(TASK *Task)
{
    UINTN Index;

    if (!TaskGetIndex(Task, &Index))
    {
        return;
    }

    KMemSet(Task, 0, sizeof(TASK));
    TaskUsed[Index] = FALSE;
}

STATIC VOID InitTaskContext(TASK *Task, VOID (*Entry)(VOID))
{
    UINT64 *ReturnAddress;

    KMemSet(&Task->Context, 0, sizeof(TASK_CONTEXT));

    ReturnAddress = (UINT64 *) Task->InitialRSP - 1;
    *ReturnAddress = (UINT64) TaskExit;
    Task->InitialRSP = (UINT64) ReturnAddress;

    Task->Context.RIP = (UINT64) Entry;
    Task->Context.CS = 0x18;
    Task->Context.RFLAGS = 0x202;
    Task->Context.RSP = Task->InitialRSP;
    Task->Context.SS = 0x10;

    Task->RSP = (UINT64) &Task->Context;
}

BOOLEAN TaskInit(VOID)
{
    KMemSet(TaskPool, 0, sizeof(TaskPool));
    KMemSet(TaskUsed, 0, sizeof(TaskUsed));

    TaskCount = 0;
    NextPID = 1;

    return TRUE;
}

TASK *TaskCreate(VOID (*Entry)(VOID))
{
    TASK *Task;

    if (Entry == NULL)
    {
        return NULL;
    }

    Task = TaskAllocate();

    if (Task == NULL)
    {
        return NULL;
    }

    if (!TaskAllocatePID(&Task->PID))
    {
        TaskFreeSlot(Task);
        return NULL;
    }

    Task->Priority = 3;
    Task->Weight = PToW(Task->Priority);

    Task->StackBase = (UINT64) KAllocPagesBelow(TASK_STACK_PAGES, TASK_STACK_ADDRESS_LIMIT);

    if (Task->StackBase == 0)
    {
        TaskFreeSlot(Task);
        return NULL;
    }

    Task->StackSize = TASK_STACK_PAGES * PAGE_SIZE;
    Task->InitialRSP = Task->StackBase + Task->StackSize;

    Task->State = TASK_READY;

    InitTaskContext(Task, Entry);

    Task->Modified = FALSE;
    Task->Idle = FALSE;
    Task->Scheduled = FALSE;

    TaskCount++;

    return Task;
}

BOOLEAN TaskSetPriority(TASK *Task, UINT8 Priority)
{
    if (Task == NULL || Priority < 1 || Priority > 6)
    {
        return FALSE;
    }

    Task->Priority = Priority;
    Task->Weight = PToW(Task->Priority);
    Task->Modified = TRUE;

    if (Task->Scheduled && !LBPWRRNotifyTaskChanged(Task))
    {
        return FALSE;
    }

    return TRUE;
}

VOID TaskSleepMls(UINT64 Milliseconds)
{
    TASK  *Task;
    UINT64 Ticks;
    UINT64 Frequency;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        SleepMls(Milliseconds);
        return;
    }

    if (Milliseconds == 0)
    {
        LBPWRRYield();
        return;
    }

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return;
    }

    Ticks = (UINT64) (((UINT128) Frequency * Milliseconds + 999ULL) / 1000ULL);

    Task->WakeDeadlineTSC = RDTSC() + Ticks;
    Task->State = TASK_SLEEPING;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
    LBPWRRYield();
}

VOID TaskSleepMis(UINT64 Microseconds)
{
    TASK  *Task;
    UINT64 Ticks;
    UINT64 Frequency;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        SleepMis(Microseconds);
        return;
    }

    if (Microseconds == 0)
    {
        LBPWRRYield();
        return;
    }

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return;
    }

    Ticks = (UINT64) (((UINT128) Frequency * Microseconds + 999999ULL) / 1000000ULL);

    Task->WakeDeadlineTSC = RDTSC() + Ticks;
    Task->State = TASK_SLEEPING;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
    LBPWRRYield();
}

VOID TaskSleepNs(UINT64 Nanoseconds)
{
    TASK  *Task;
    UINT64 Ticks;
    UINT64 Frequency;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        SleepNs(Nanoseconds);
        return;
    }

    if (Nanoseconds == 0)
    {
        LBPWRRYield();
        return;
    }

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return;
    }

    Ticks = (UINT64) (((UINT128) Frequency * Nanoseconds + 999999999ULL) / 1000000000ULL);

    Task->WakeDeadlineTSC = RDTSC() + Ticks;
    Task->State = TASK_SLEEPING;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
    LBPWRRYield();
}

BOOLEAN TaskDestroy(TASK *Task)
{
    UINTN Index;

    if (Task == NULL)
    {
        return FALSE;
    }

    if (!TaskGetIndex(Task, &Index) || !TaskUsed[Index])
    {
        return FALSE;
    }

    if (Task->State != TASK_TERMINATED && Task->State != TASK_KILLED)
    {
        return FALSE;
    }

    if (Task->Scheduled && !LBPWRRRemoveTask(Task))
    {
        return FALSE;
    }

    if (Task->StackBase != 0)
    {
        KMemFreePages((VOID *) Task->StackBase, TASK_STACK_PAGES);
    }

    TaskFreeSlot(Task);

    if (TaskCount > 0)
    {
        TaskCount--;
    }

    return TRUE;
}

VOID TaskKill(TASK *Task)
{
    if (Task == NULL)
    {
        return;
    }

    Task->State = TASK_KILLED;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
}

VOID TaskTerminate(TASK *Task)
{
    if (Task == NULL)
    {
        return;
    }

    Task->State = TASK_TERMINATED;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
}

TASK *TaskGetByPID(UINT16 PID)
{
    UINTN Index;

    for (Index = 0; Index < LBPWRR_MAX_TASKS; Index++)
    {
        if (!TaskUsed[Index])
        {
            continue;
        }

        if (TaskPool[Index].PID == PID)
        {
            return &TaskPool[Index];
        }
    }

    return NULL;
}

UINTN TaskGetCount(VOID)
{
    return TaskCount;
}
