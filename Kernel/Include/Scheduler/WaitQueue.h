// Kernel/Include/Scheduler/WaitQueue.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Arch/AMD64/SpinLock.h>
#include <Scheduler/TaskTypes.h>

typedef struct WAIT_QUEUE
{
    TASK *Head;
    TASK *Tail;
} WAIT_QUEUE;

VOID    WaitQueueInit(WAIT_QUEUE *Queue);
BOOLEAN WaitQueueSleep(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock);
VOID    WaitQueueWakeOne(WAIT_QUEUE *Queue);
VOID    WaitQueueWakeAll(WAIT_QUEUE *Queue);
VOID    WaitQueueFinishTask(TASK *Task, TASK_STATE State);
