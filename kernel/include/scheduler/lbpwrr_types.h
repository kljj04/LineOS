// kernel/include/scheduler/lbpwrr_types.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <arch/x86_64/spinlock.h>
#include <lineos/typeinfo.h>
#include <scheduler/task_types.h>

#define LBPWRR_MAX_TASKS 1024
#define CHUNK_MAX_COUNT  1024
#define TASK_STACK_PAGES 16
#define PAGE_SIZE        4096

typedef struct CHUNK
{
    TASK  *Tasks[CHUNK_MAX_COUNT];
    UINT32 Count;
} CHUNK;

typedef struct LBPWRR_RUNQUEUE
{
    TASK            *Runnable[LBPWRR_MAX_TASKS];
    UINT32           RunnableCount;
    TASK            *Unrunnable[LBPWRR_MAX_TASKS];
    UINT32           UnrunnableCount;
    CHUNK            Chunk;
    CHUNK            BuildChunk;
    UINT32           CurrentIndex;
    UINT64           Generation;
    UINT64           BuildGeneration;
    VOLATILE BOOLEAN RebuildPending;
    VOLATILE BOOLEAN RebuildRunning;
    SPIN_LOCK        Lock;
} LBPWRR_RUNQUEUE;

typedef struct CPU_USAGE
{
    VOLATILE UINT64 BusyTSC;
    VOLATILE UINT64 IdleTSC;
    VOLATILE UINT64 LastSampleBusyTSC;
    VOLATILE UINT64 LastSampleIdleTSC;
    VOLATILE UINTN  CPUUsage;
} CPU_USAGE;