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

// Initialize before publication; the queue must outlive all waiting tasks.
VOID WaitQueueInit(WAIT_QUEUE *Queue);
// Caller holds ConditionLock with IRQs disabled and has checked its predicate.
// Releases ConditionLock, sleeps, then reacquires it with IRQs still disabled.
// Caller rechecks its predicate and restores the original IRQ flags itself.
BOOLEAN WaitQueueSleep(WAIT_QUEUE *Queue, SPIN_LOCK *ConditionLock);
// IRQ-safe; publish the predicate under its condition lock before waking.
VOID WaitQueueWakeOne(WAIT_QUEUE *Queue);
VOID WaitQueueWakeAll(WAIT_QUEUE *Queue);
// Detaches a waiter and changes its terminal state under scheduler synchronization.
VOID WaitQueueFinishTask(TASK *Task, TASK_STATE State);
