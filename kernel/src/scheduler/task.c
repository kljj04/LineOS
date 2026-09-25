// kernel/src/scheduler/task.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/task.h>
#include <scheduler/lbpwrr_types.h>
#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <memory/memory.h>

STATIC TASK TaskPool[LBPWRR_MAX_TASKS];
STATIC BOOLEAN TaskUsed[LBPWRR_MAX_TASKS];

STATIC UINTN TaskCount;
STATIC UINT16 NextPID;

STATIC BOOLEAN TaskGetIndex(TASK *task, UINTN *Index)
{
    UINTN index;

    if (task == NULL || Index == NULL)
    {
        return FALSE;
    }

    for (index = 0; index < LBPWRR_MAX_TASKS; index++)
    {
        if (&TaskPool[index] == task)
        {
            *Index = index;
            return TRUE;
        }
    }

    return FALSE;
}

STATIC UINT8 PToW(UINT8 priority)
{
    if (priority > 5)
    {
        return 0;
    }

    return 6 - priority;
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
    KMemSet(&task->Context, 0, sizeof(TASK_CONTEXT));

    task->Context.RIP = (UINT64)entry;
    task->Context.CS = 0x18;
    task->Context.RFLAGS = 0x202;
    task->Context.RSP = task->InitialRSP;
    task->Context.SS = 0x10;

    task->RSP = (UINT64)&task->Context;
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

    task->StackBase = (UINT64)KAllocPages(TASK_STACK_PAGES);

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

    TaskCount++;

    return task;
}

BOOLEAN TaskSetPriority(TASK *task, UINT8 priority)
{
    if (task == NULL || priority > 5)
    {
        return FALSE;
    }

    task->Priority = priority;
    task->Weight = PToW(task->Priority);
    task->Modified = TRUE;

    return TRUE;
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

    if (task->State != TASK_TERMINATED &&
        task->State != TASK_KILLED)
    {
        return FALSE;
    }

    if (task->StackBase != 0)
    {
        KMemFreePages((VOID *)task->StackBase, TASK_STACK_PAGES);
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
}

VOID TaskTerminate(TASK *task)
{
    if (task == NULL)
    {
        return;
    }

    task->State = TASK_TERMINATED;
    task->Modified = TRUE;
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