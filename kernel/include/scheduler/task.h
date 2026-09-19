// kernel/include/scheduler/task.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <scheduler/task_types.h>

BOOLEAN TaskInit(VOID);
TASK   *TaskCreate(VOID (*entry)(VOID));
VOID    TaskKill(TASK *task);
VOID    TaskTerminate(TASK *task);
BOOLEAN TaskDestroy(TASK *task);
TASK   *TaskGetByPID(UINT16 PID);
UINTN   TaskGetCount(VOID);
