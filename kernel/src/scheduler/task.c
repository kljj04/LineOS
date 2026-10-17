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
    TASK *task;

    task = LBPWRRGetCurrentTask();

    if (task != NULL)
    {
        TaskTerminate(task);
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
    TASK *task;

    task = TaskCreate(TaskIdle);

    if (task != NULL)
    {
        task->Idle = TRUE;
    }

    return task;
}

STATIC BOOLEAN TaskGetIndex(TASK *task, UINTN *TaskIndex)
{
    UINTN index;

    if (task == NULL || TaskIndex == NULL)
    {
        return FALSE;
    }

    for (index = 0; index < LBPWRR_MAX_TASKS; index++)
    {
        if (&TaskPool[index] == task)
        {
            *TaskIndex = index;
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
    UINTN index;

    for (index = 0; index < LBPWRR_MAX_TASKS; index++)
    {
        if (!TaskUsed[index])
        {
            TaskUsed[index] = TRUE;
            KMemSet(&TaskPool[index], 0, sizeof(TASK));

            return &TaskPool[index];
        }
    }

    return NULL;
}

STATIC BOOLEAN TaskPIDUsed(UINT16 PID)
{
    UINTN index;

    if (PID == 0)
    {
        return TRUE;
    }

    for (index = 0; index < LBPWRR_MAX_TASKS; index++)
    {
        if (!TaskUsed[index])
        {
            continue;
        }

        if (TaskPool[index].PID == PID)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC BOOLEAN TaskAllocatePID(UINT16 *PID)
{
    UINT32 retry;
    UINT16 candidate;

    if (PID == NULL)
    {
        return FALSE;
    }

    candidate = NextPID;

    for (retry = 0; retry < 0xFFFF; retry++)
    {
        if (candidate == 0)
        {
            candidate = 1;
        }

        if (!TaskPIDUsed(candidate))
        {
            *PID = candidate;

            candidate++;

            if (candidate == 0)
            {
                candidate = 1;
            }

            NextPID = candidate;

            return TRUE;
        }

        candidate++;
    }

    return FALSE;
}

STATIC VOID TaskFreeSlot(TASK *task)
{
    UINTN index;

    if (!TaskGetIndex(task, &index))
    {
        return;
    }

    KMemSet(task, 0, sizeof(TASK));
    TaskUsed[index] = FALSE;
}

STATIC VOID InitTaskContext(TASK *task, VOID (*entry)(VOID))
{
    UINT64 *ReturnAddress;

    KMemSet(&task->Context, 0, sizeof(TASK_CONTEXT));

    ReturnAddress = (UINT64 *) task->InitialRSP - 1;
    *ReturnAddress = (UINT64) TaskExit;
    task->InitialRSP = (UINT64) ReturnAddress;

    task->Context.RIP = (UINT64) entry;
    task->Context.CS = 0x18;
    task->Context.RFLAGS = 0x202;
    task->Context.RSP = task->InitialRSP;
    task->Context.SS = 0x10;

    task->RSP = (UINT64) &task->Context;
}

BOOLEAN TaskInit(VOID)
{
    KMemSet(TaskPool, 0, sizeof(TaskPool));
    KMemSet(TaskUsed, 0, sizeof(TaskUsed));

    TaskCount = 0;
    NextPID = 1;

    return TRUE;
}

TASK *TaskCreate(VOID (*entry)(VOID))
{
    TASK *task;

    if (entry == NULL)
    {
        return NULL;
    }

    task = TaskAllocate();

    if (task == NULL)
    {
        return NULL;
    }

    if (!TaskAllocatePID(&task->PID))
    {
        TaskFreeSlot(task);
        return NULL;
    }

    task->Priority = 3;
    task->Weight = PToW(task->Priority);

    task->StackBase = (UINT64) KAllocPagesBelow(TASK_STACK_PAGES, TASK_STACK_ADDRESS_LIMIT);

    if (task->StackBase == 0)
    {
        TaskFreeSlot(task);
        return NULL;
    }

    task->StackSize = TASK_STACK_PAGES * PAGE_SIZE;
    task->InitialRSP = task->StackBase + task->StackSize;

    task->State = TASK_READY;

    InitTaskContext(task, entry);

    task->Modified = FALSE;
    task->Idle = FALSE;
    task->Scheduled = FALSE;

    TaskCount++;

    return task;
}

BOOLEAN TaskSetPriority(TASK *task, UINT8 priority)
{
    if (task == NULL || priority < 1 || priority > 6)
    {
        return FALSE;
    }

    task->Priority = priority;
    task->Weight = PToW(task->Priority);
    task->Modified = TRUE;

    if (task->Scheduled && !LBPWRRNotifyTaskChanged(task))
    {
        return FALSE;
    }

    return TRUE;
}

VOID TaskSleepMls(UINT64 milliseconds)
{
    TASK  *task;
    UINT64 ticks;
    UINT64 frequency;

    task = LBPWRRGetCurrentTask();

    if (task == NULL)
    {
        SleepMls(milliseconds);
        return;
    }

    if (milliseconds == 0)
    {
        LBPWRRYield();
        return;
    }

    frequency = TSCGetFrequency();

    if (frequency == 0)
    {
        return;
    }

    ticks = (UINT64) (((UINT128) frequency * milliseconds + 999ULL) / 1000ULL);

    task->WakeDeadlineTSC = RDTSC() + ticks;
    task->State = TASK_SLEEPING;
    task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(task);
    LBPWRRYield();
}

VOID TaskSleepMis(UINT64 microseconds)
{
    TASK  *task;
    UINT64 ticks;
    UINT64 frequency;

    task = LBPWRRGetCurrentTask();

    if (task == NULL)
    {
        SleepMis(microseconds);
        return;
    }

    if (microseconds == 0)
    {
        LBPWRRYield();
        return;
    }

    frequency = TSCGetFrequency();

    if (frequency == 0)
    {
        return;
    }

    ticks = (UINT64) (((UINT128) frequency * microseconds + 999999ULL) / 1000000ULL);

    task->WakeDeadlineTSC = RDTSC() + ticks;
    task->State = TASK_SLEEPING;
    task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(task);
    LBPWRRYield();
}

VOID TaskSleepNs(UINT64 nanoseconds)
{
    TASK  *task;
    UINT64 ticks;
    UINT64 frequency;

    task = LBPWRRGetCurrentTask();

    if (task == NULL)
    {
        SleepNs(nanoseconds);
        return;
    }

    if (nanoseconds == 0)
    {
        LBPWRRYield();
        return;
    }

    frequency = TSCGetFrequency();

    if (frequency == 0)
    {
        return;
    }

    ticks = (UINT64) (((UINT128) frequency * nanoseconds + 999999999ULL) / 1000000000ULL);

    task->WakeDeadlineTSC = RDTSC() + ticks;
    task->State = TASK_SLEEPING;
    task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(task);
    LBPWRRYield();
}

BOOLEAN TaskDestroy(TASK *task)
{
    UINTN index;

    if (task == NULL)
    {
        return FALSE;
    }

    if (!TaskGetIndex(task, &index) || !TaskUsed[index])
    {
        return FALSE;
    }

    if (task->State != TASK_TERMINATED && task->State != TASK_KILLED)
    {
        return FALSE;
    }

    if (task->Scheduled && !LBPWRRRemoveTask(task))
    {
        return FALSE;
    }

    if (task->StackBase != 0)
    {
        KMemFreePages((VOID *) task->StackBase, TASK_STACK_PAGES);
    }

    TaskFreeSlot(task);

    if (TaskCount > 0)
    {
        TaskCount--;
    }

    return TRUE;
}

VOID TaskKill(TASK *task)
{
    if (task == NULL)
    {
        return;
    }

    task->State = TASK_KILLED;
    task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(task);
}

VOID TaskTerminate(TASK *task)
{
    if (task == NULL)
    {
        return;
    }

    task->State = TASK_TERMINATED;
    task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(task);
}

TASK *TaskGetByPID(UINT16 PID)
{
    UINTN index;

    for (index = 0; index < LBPWRR_MAX_TASKS; index++)
    {
        if (!TaskUsed[index])
        {
            continue;
        }

        if (TaskPool[index].PID == PID)
        {
            return &TaskPool[index];
        }
    }

    return NULL;
}

UINTN TaskGetCount(VOID)
{
    return TaskCount;
}
