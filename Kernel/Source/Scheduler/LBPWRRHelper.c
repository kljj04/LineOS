// Kernel/Source/Scheduler/LBPWRRHelper.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/LBPWRRHelper.h>
#include <Scheduler/LBPWRRChunk.h>
#include <Arch/AMD64/CPU.h>

BOOLEAN LBPWRRTaskIsRunnable(TASK *Task)
{
    if (Task == NULL)
    {
        return FALSE;
    }

    return Task->State == TASK_READY || Task->State == TASK_RUNNING;
}

BOOLEAN LBPWRRWakeSleepingTasks(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32  Index;
    UINT64  Now;
    TASK   *Task;
    BOOLEAN WokeTask;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    Now = RDTSC();
    WokeTask = FALSE;

    for (Index = 0; Index < RunQueue->RunnableCount; Index++)
    {
        Task = RunQueue->Runnable[Index];

        if (Task == NULL || Task->State != TASK_SLEEPING)
        {
            continue;
        }

        if ((INT64) (Now - Task->WakeDeadlineTSC) < 0)
        {
            continue;
        }

        if (!LBPWRRModifyChunk(RunQueue, Task, Task->Weight))
        {
            continue;
        }

        Task->WakeDeadlineTSC = 0;
        Task->State = TASK_READY;
        Task->Modified = FALSE;
        WokeTask = TRUE;
    }

    return WokeTask;
}