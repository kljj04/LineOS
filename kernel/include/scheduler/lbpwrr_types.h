// kernel/include/scheduler/lbpwrr_types.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <scheduler/task_types.h>

#define LBPWRR_MAX_TASKS 1024

typedef struct LBPWRR_CPU
{
    VOLATILE UINT64  CurrentTask;
    VOLATILE UINT64  SwitchStack;
    VOLATILE BOOLEAN CurrentTaskValid;
    TASK            *RunQueue[LBPWRR_MAX_TASKS];
    UINT32           RunQueueCount;
    UINT32           RunQueueIndex;
} LBPWRR_CPU;

typedef struct CPU_USAGE
{
    VOLATILE UINT64 BusyTSC;
    VOLATILE UINT64 IdleTSC;
    VOLATILE UINT64 LastSampleBusyTSC;
    VOLATILE UINT64 LastSampleIdleTSC;
    VOLATILE UINTN  CPUUsage;
} CPU_USAGE;