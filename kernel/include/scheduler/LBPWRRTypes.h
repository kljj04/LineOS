// kernel/include/scheduler/LBPWRRTypes.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <arch/AMD64/SpinLock.h>
#include <LineOS/TypeInfo.h>
#include <scheduler/TaskTypes.h>

#define LBPWRR_MAX_TASKS         1024
#define CHUNK_MAX_COUNT          1024
#define TASK_STACK_PAGES         16
#define PAGE_SIZE                4096
#define TASK_STACK_ADDRESS_LIMIT 0x100000000ULL

typedef struct CHUNK
{
    TASK  *Tasks[CHUNK_MAX_COUNT];
    UINT32 Count;
} CHUNK;

typedef struct LBPWRR_RUNQUEUE
{
    TASK     *Runnable[LBPWRR_MAX_TASKS];
    UINT32    RunnableCount;
    TASK     *Unrunnable[LBPWRR_MAX_TASKS];
    UINT32    UnrunnableCount;
    CHUNK     Chunk;
    UINT32    CurrentIndex;
    SPIN_LOCK Lock;
} LBPWRR_RUNQUEUE;

typedef struct CPU_USAGE
{
    UINT64 BusyTSC;
    UINT64 IdleTSC;
    UINT64 LastSampleBusyTSC;
    UINT64 LastSampleIdleTSC;
    UINT64 LastTSC;
    UINTN  CPUUsage;
} CPU_USAGE;
