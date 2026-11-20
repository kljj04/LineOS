// Kernel/Include/Syscall/Syscall.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Interrupt/IDT.h>
#include <LineOS/TypeInfo.h>
#include <Syscall/Numbers.h>

typedef struct
{
    UINT64 KernelStack;
    UINT64 UserStack;
} SYSCALL_CPU;

typedef struct
{
    UINT64 RDI;
    UINT64 RSI;
    UINT64 RDX;
    UINT64 R10;
    UINT64 R8;
    UINT64 R9;
} SYSCALL_ABI_ARGS;

typedef struct
{
    UINT64           Number;
    SYSCALL_ABI_ARGS Args;
    INTERRUPT_FRAME *Frame;
} SYSCALL_CONTEXT;

typedef INT64 (*SYSCALL_HANDLER)(SYSCALL_CONTEXT *Context);

typedef struct
{
    UINT64          Number;
    SYSCALL_HANDLER Handler;
} SYSCALL_ENTRY;

VOID   SyscallInitCurrentCPU(UINT32 CPUIndex);
VOID   SyscallSetKernelStack(UINT32 CPUIndex, UINT64 StackTop);
UINT64 SyscallDispatch(INTERRUPT_FRAME *Frame);