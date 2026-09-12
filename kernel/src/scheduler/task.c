// kernel/src/scheduler/task.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/task.h>
#include <debug/debug.h>
#include <memory/memory.h>

#define TASK_MAX_COUNT      1024
#define TASK_STACK_PAGES    16
#define PAGE_SIZE           4096
#define TASK_KERNEL_CS      0x18
#define TASK_INITIAL_RFLAGS 0x2

STATIC TASK TaskPool[TASK_MAX_COUNT];
STATIC BOOLEAN TaskUsed[TASK_MAX_COUNT];

STATIC UINT16 NextPID = 1;
STATIC UINTN TaskCount = 0;

STATIC UINT16 TaskAllocatePID(VOID)
{
    UINT16 StartPID;
    UINT16 PID;
    UINTN i;
    BOOLEAN Used;

    StartPID = NextPID;

    while (TRUE)
    {
        PID = NextPID++;

        if (NextPID == 0)
        {
            NextPID = 1;
        }

        Used = FALSE;

        for (i = 0; i < TASK_MAX_COUNT; i++)
        {
            if (TaskUsed[i] && TaskPool[i].PID == PID)
            {
                Used = TRUE;
                break;
            }
        }

        if (!Used)
        {
            return PID;
        }

        if (NextPID == StartPID)
        {
            return 0;
        }
    }
}

STATIC BOOLEAN TaskSetupContext(TASK *task, VOID (*entry)(VOID))
{
    TASK_CONTEXT *Context;
    UINT64 StackTop;

    if (task == NULL || entry == NULL)
    {
        return FALSE;
    }

    StackTop = task->StackBase + task->StackSize;

    StackTop -= sizeof(UINT64);
    *((UINT64 *) StackTop) = 0;

    StackTop -= sizeof(TASK_CONTEXT);

    Context = (TASK_CONTEXT *) StackTop;

    KMemSet(Context, 0, sizeof(TASK_CONTEXT));

    Context->RIP = (UINT64) entry;
    Context->CS = TASK_KERNEL_CS;
    Context->RFLAGS = TASK_INITIAL_RFLAGS;

    task->RSP = StackTop;
    task->InitialRSP = StackTop;

    DebugWrite("TASK CTX base=");
    DebugWriteHex(task->StackBase);
    DebugWrite(" size=");
    DebugWriteHex(task->StackSize);
    DebugWrite(" rsp=");
    DebugWriteHex(task->RSP);
    DebugWrite(" ret=");
    DebugWriteHex(task->StackBase + task->StackSize - sizeof(UINT64));
    DebugWrite(" rip=");
    DebugWriteHex(Context->RIP);
    DebugWrite(" cs=");
    DebugWriteHex(Context->CS);
    DebugWrite(" rflags=");
    DebugWriteHex(Context->RFLAGS);
    DebugWrite(" ctx-size=");
    DebugWriteDec(sizeof(TASK_CONTEXT));
    DebugWrite("\n");

    return TRUE;
}

BOOLEAN TaskInit(VOID)
{
    KMemSet(TaskPool, 0, sizeof(TaskPool));
    KMemSet(TaskUsed, 0, sizeof(TaskUsed));

    NextPID = 1;
    TaskCount = 0;

    return TRUE;
}

TASK *TaskCreate(VOID (*entry)(VOID))
{
    TASK *task;
    VOID *Stack;
    UINT16 PID;
    UINTN i;

    if (entry == NULL)
    {
        return NULL;
    }

    task = NULL;

    for (i = 0; i < TASK_MAX_COUNT; i++)
    {
        if (!TaskUsed[i])
        {
            task = &TaskPool[i];
            break;
        }
    }

    if (task == NULL)
    {
        return NULL;
    }

    PID = TaskAllocatePID();

    if (PID == 0)
    {
        return NULL;
    }

    Stack = KAllocPages(TASK_STACK_PAGES);

    if (Stack == NULL)
    {
        return NULL;
    }

    KMemSet(task, 0, sizeof(TASK));

    task->StackBase = (UINT64) Stack;
    task->StackSize = TASK_STACK_PAGES * PAGE_SIZE;
    task->State = TASK_READY;
    task->CPUID = 0;
    task->Priority = 0;
    task->Weight = 1;
    task->Quantum = 0;
    task->IsIdle = FALSE;
    task->PID = PID;

    if (!TaskSetupContext(task, entry))
    {
        KMemFreePages(Stack, TASK_STACK_PAGES);
        KMemSet(task, 0, sizeof(TASK));

        return NULL;
    }

    TaskUsed[i] = TRUE;
    TaskCount++;

    return task;
}

VOID TaskKill(TASK *task)
{
    if (task == NULL)
    {
        return;
    }

    if (task->State == TASK_DEAD)
    {
        return;
    }

    task->State = TASK_DEAD;
}

VOID TaskReap(VOID)
{
    UINTN i;

    for (i = 0; i < TASK_MAX_COUNT; i++)
    {
        if (!TaskUsed[i])
        {
            continue;
        }

        if (TaskPool[i].State != TASK_DEAD)
        {
            continue;
        }

        if (TaskPool[i].StackBase != 0)
        {
            KMemFreePages((VOID *) TaskPool[i].StackBase, TASK_STACK_PAGES);
        }

        KMemSet(&TaskPool[i], 0, sizeof(TASK));

        TaskUsed[i] = FALSE;

        if (TaskCount > 0)
        {
            TaskCount--;
        }
    }
}

TASK *TaskGetByPID(UINT16 PID)
{
    UINTN i;

    if (PID == 0)
    {
        return NULL;
    }

    for (i = 0; i < TASK_MAX_COUNT; i++)
    {
        if (!TaskUsed[i])
        {
            continue;
        }

        if (TaskPool[i].State == TASK_DEAD)
        {
            continue;
        }

        if (TaskPool[i].PID == PID)
        {
            return &TaskPool[i];
        }
    }

    return NULL;
}

UINTN TaskGetCount(VOID)
{
    return TaskCount;
}
