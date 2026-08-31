// kernel/include/scheduler/prr_types.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

typedef enum
{
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
} TASK_STATE;

typedef struct
{
    UINT64     RSP;
    UINT64     InitialRSP;
    UINT64     StackBase;
    UINT64     StackSize;
    TASK_STATE State;
} TASK;
