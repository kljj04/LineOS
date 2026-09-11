// kernel/include/scheduler/task.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

typedef enum
{
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD,
    TASK_SLEEPING
} TASK_STATE;

typedef struct TASK
{
    UINT64     RSP;
    UINT64     InitialRSP;
    UINT64     StackBase;
    UINT64     StackSize;
    TASK_STATE State;
    UINT32     CPUID;
    UINT8      Priority;
    UINT8      Weight;
    UINT16     Reserved;
    UINT32     Quantum;
    BOOLEAN    IsIdle;
} TASK;
