// kernel/include/scheduler/task_types.h
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

typedef struct TASK_CONTEXT
{
    UINT64 R15;
    UINT64 R14;
    UINT64 R13;
    UINT64 R12;
    UINT64 R11;
    UINT64 R10;
    UINT64 R9;
    UINT64 R8;
    UINT64 RBP;
    UINT64 RDI;
    UINT64 RSI;
    UINT64 RDX;
    UINT64 RCX;
    UINT64 RBX;
    UINT64 RAX;

    UINT64 Vector;
    UINT64 ErrorCode;

    UINT64 RIP;
    UINT64 CS;
    UINT64 RFLAGS;
} TASK_CONTEXT;

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
    UINT16     PID;
} TASK;