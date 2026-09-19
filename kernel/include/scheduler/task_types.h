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
    TASK_SLEEPING,
    TASK_TERMINATED,
    TASK_KILLED
} TASK_STATE;

typedef enum {
    RESERVED,
    SIGHUP,
    SIGINT,
    SIGQUIT,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGKILL,
    SIGUSR1,
    SIGSEGV,
    SIGUSR2,
    SIGPIPE,
    SIGALRM,
    SIGTERM,
    SIGSTKFLT,
    SIGCHLD,
    SIGCONT,
    SIGSTOP,
    SIGTSTP,
    SIGTTIN,
    SIGTTOU,
    SIGURG,
    SIGXCPU,
    SIGXFSZ,
    SIGVTALRM,
    SIGPROF,
    SIGWINCH,
    SIGIO,
    SIGPWR,
    SIGSYS,
    SIGRTMIN,
    SIGRTMAX
} TASK_SIGNAL;

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
    UINT64 RSP;
    UINT64 SS;
} TASK_CONTEXT;

typedef struct TASK
{
    UINT64       RSP;
    UINT64       InitialRSP;
    UINT64       StackBase;
    UINT64       StackSize;
    TASK_STATE   State;
    TASK_CONTEXT Context;
    UINT32       CPUID;
    UINT8        Priority;
    UINT8        Weight;
    UINT8        Credit;
    UINT16       PID;
    TASK_SIGNAL  Signal;
} TASK;