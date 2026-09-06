// kernel/include/scheduler/lbpwrr_types.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <arch/x86_64/spinlock.h>
#include <lineos/typeinfo.h>

typedef enum
{
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
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

typedef struct LBPWRR_CPU
{
    VOLATILE UINT64  CurrentTask;
    VOLATILE UINT64  SwitchStack;
    VOLATILE BOOLEAN CurrentTaskValid;

    VOLATILE UINT64  LastTSC;
    VOLATILE UINT64  BusyTSC;
    VOLATILE UINT64  IdleTSC;
    VOLATILE UINT64  LastSampleBusyTSC;
    VOLATILE UINT64  LastSampleIdleTSC;
    VOLATILE UINTN   CPUUsage;
} LBPWRR_CPU;
