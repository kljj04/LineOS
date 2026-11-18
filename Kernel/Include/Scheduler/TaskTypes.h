// Kernel/Include/Scheduler/TaskTypes.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/VFS/VFSTypes.h>
#include <Arch/AMD64/SpinLock.h>
#include <LineOS/TypeInfo.h>
#include <Memory/VMM.h>
#include <Syscall/Asset/Asset.h>

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

typedef struct TASK            TASK;
typedef struct WAIT_QUEUE      WAIT_QUEUE;
typedef struct MMAP_REGION     MMAP_REGION;
typedef struct ELF64_EXECUTION ELF64_EXECUTION;

typedef struct PROCESS
{
    UINT16             PID;
    VMM_ADDRESS_SPACE *AddressSpace;
    MMAP_REGION       *Mappings;
    ELF64_EXECUTION   *Execution;
    VFS_CONTEXT       *FileContext;
    BOOLEAN            AssetPublisher;
    TASK_ASSET         Assets[LOS_ASSET_MAPPING_LIMIT];
    SPIN_LOCK          FileTableLock;
    UINT32             ReservedFiles;
    OPEN_FILE         *Files[TASK_FILE_LIMIT];
    CHAR8              Name[16];
    UINT32             RealUID;
    UINT32             RealGID;
    UINT64             AuthSession;
    TASK              *ThreadHead;
    UINT32             ThreadCount;
    BOOLEAN            Exited;
    BOOLEAN            GroupExiting;
    UINT64             GroupExitCode;
} PROCESS;

typedef struct TASK
{
    UINT64       RSP;
    UINT64       InitialRSP;
    UINT64       StackBase;
    UINT64       StackSize;
    UINT64       FSBase;
    UINT8        FPState[512] __attribute__((aligned(16)));
    UINT64       ClearTIDAddress;
    UINT64       RobustListHead;
    UINT64       FutexAddress;
    UINT64       KernelStackBase;
    UINT64       KernelStackSize;
    TASK_STATE   State;
    TASK_CONTEXT Context;
    UINT32       CPUID;
    UINT8        Priority;
    UINT8        Weight;
    UINT64       WakeDeadlineTSC;
    UINT64       WaitDeadlineTSC;
    BOOLEAN      WaitTimedOut;
    TASK_SIGNAL  Signal;
    BOOLEAN      Modified;
    BOOLEAN      Idle;
    BOOLEAN      Scheduled;
    UINT64       CPUTimeTSC;
    UINT64       LastCPUTimeTSC;
    UINT32       CPUUsage;
    BOOLEAN      OnCPU;
    WAIT_QUEUE  *WaitQueue;
    TASK        *WaitNext;
    UINT64       UserStackBase;
    UINT64       UserStackSize;
    PROCESS     *Process;
    TASK        *ThreadNext;
    TASK        *ThreadPrev;
    UINT32       TID;
} TASK;
