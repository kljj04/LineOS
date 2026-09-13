// kernel/src/scheduler/task.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/task.h>
#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <memory/memory.h>

#define TASK_MAX_COUNT   1024
#define TASK_STACK_PAGES 16
#define TASK_PAGE_SIZE   4096

STATIC TASK    TaskPool[TASK_MAX_COUNT];
STATIC BOOLEAN TaskUsed[TASK_MAX_COUNT];

STATIC UINTN  TaskCount = 0;
STATIC UINT16 NextPID = 1;

BOOLEAN TaskInit(VOID)
{
    KMemSet(TaskPool, 0, sizeof(TaskPool));
    KMemSet(TaskUsed, 0, sizeof(TaskUsed));

    TaskCount = 0;
    NextPID = 1;

    return TRUE;
}

STATIC TASK *TaskAllocate(VOID)
{
    UINTN index;

    for (index = 0; index < TASK_MAX_COUNT; index++)
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

STATIC UINT16 TaskAllocatePID(VOID)
{
    UINT16 PID;

    PID = NextPID++;

    if (NextPID == 0)
    {
        NextPID = 1;
    }

    return PID;
}

TASK   *TaskCreate(VOID (*entry)(VOID)) {}

VOID    TaskKill(TASK *task) {}

VOID    TaskReap(VOID) {}

TASK   *TaskGetByPID(UINT16 PID) {}

UINTN   TaskGetCount(VOID) {}