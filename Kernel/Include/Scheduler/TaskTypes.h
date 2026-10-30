// Kernel/Include/Scheduler/TaskTypes.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Memory/VMM.h>
#include <Syscall/Asset/Asset.h>
#include <FS/VFS/VFSTypes.h>

#define TASK_FILE_LIMIT 16U

typedef struct OPEN_FILE OPEN_FILE;

typedef struct
{
    UINT64 Handle;
    UINT32 Index;
    UINT32 References;
} TASK_ASSET;

typedef enum
{
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_TERMINATED,
    TASK_KILLED
} TASK_STATE;

typedef enum
{
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
    UINT64                  RSP;
    UINT64                  InitialRSP;
    UINT64                  StackBase;
    UINT64                  StackSize;
    UINT64                  KernelStackBase;
    UINT64                  KernelStackSize;
    TASK_STATE              State;
    TASK_CONTEXT            Context;
    UINT32                  CPUID;
    UINT8                   Priority;
    UINT8                   Weight;
    UINT16                  PID;
    UINT64                  WakeDeadlineTSC;
    TASK_SIGNAL             Signal;
    BOOLEAN                 Modified;
    BOOLEAN                 Idle;
    BOOLEAN                 Scheduled;
    UINT64                  CPUTimeTSC;
    UINT64                  LastCPUTimeTSC;
    UINT32                  CPUUsage;
    BOOLEAN                 OnCPU;
    struct WAIT_QUEUE      *WaitQueue;
    struct TASK            *WaitNext;
    VMM_ADDRESS_SPACE      *AddressSpace;
    struct MMAP_REGION     *Mappings;
    struct ELF64_EXECUTION *Execution;
    UINT64                  UserStackBase;
    UINT64                  UserStackSize;
    VFS_CONTEXT            *FileContext;
    BOOLEAN                 AssetPublisher;
    TASK_ASSET              Assets[LOS_ASSET_MAPPING_LIMIT];
    OPEN_FILE              *Files[TASK_FILE_LIMIT];
    CHAR8                   Name[16];
    UINT32                  RealUID;
    UINT32                  RealGID;
    UINT64                  AuthSession;
} TASK;
