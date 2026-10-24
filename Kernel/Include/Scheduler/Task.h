// Kernel/Include/Scheduler/Task.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Scheduler/TaskTypes.h>

BOOLEAN TaskInit(VOID);
TASK   *TaskCreate(VOID (*Entry)(VOID));
TASK   *TaskCreateIdle(VOID);
BOOLEAN TaskSetPriority(TASK *Task, UINT8 Priority);
VOID    TaskSleepMls(UINT64 Milliseconds);
VOID    TaskSleepMis(UINT64 Microseconds);
VOID    TaskSleepNs(UINT64 Nanoseconds);
VOID    TaskKill(TASK *Task);
VOID    TaskTerminate(TASK *Task);
BOOLEAN TaskDestroy(TASK *Task);
TASK   *TaskGetByPID(UINT16 PID);
UINTN   TaskGetCount(VOID);
VOID    TaskSampleCPUUsage(UINT64 IntervalTSC);