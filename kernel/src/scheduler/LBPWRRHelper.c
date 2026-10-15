// kernel/src/scheduler/LBPWRRHelper.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRRHelper.h>
#include <scheduler/LBPWRRChunk.h>
#include <arch/AMD64/CPU.h>

BOOLEAN LBPWRRTaskIsRunnable(TASK *task)
{
    if (task == NULL)
    {
        return FALSE;
    }

    return task->State == TASK_READY || task->State == TASK_RUNNING;
}

BOOLEAN LBPWRRWakeSleepingTasks(LBPWRR_RUNQUEUE *RunQueue)
{
    UINT32  index;
    UINT64  now;
    TASK   *task;
    BOOLEAN WokeTask;

    if (RunQueue == NULL)
    {
        return FALSE;
    }

    now = RDTSC();
    WokeTask = FALSE;

    for (index = 0; index < RunQueue->RunnableCount; index++)
    {
        task = RunQueue->Runnable[index];

        if (task == NULL || task->State != TASK_SLEEPING)
        {
            continue;
        }

        if ((INT64) (now - task->WakeDeadlineTSC) < 0)
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
    }

    return WokeTask;
}
