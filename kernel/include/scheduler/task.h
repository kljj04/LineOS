// kernel/include/scheduler/task.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <scheduler/TaskTypes.h>

BOOLEAN TaskInit(VOID);
TASK   *TaskCreate(VOID (*entry)(VOID));
BOOLEAN TaskSetPriority(TASK *task, UINT8 priority);
VOID    TaskSleepMls(UINT64 milliseconds);
VOID    TaskKill(TASK *task);
VOID    TaskTerminate(TASK *task);
BOOLEAN TaskDestroy(TASK *task);
TASK   *TaskGetByPID(UINT16 PID);
UINTN   TaskGetCount(VOID);
