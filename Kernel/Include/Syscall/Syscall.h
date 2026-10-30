// Kernel/Include/Syscall/Syscall.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Interrupt/IDT.h>
#include <LineOS/TypeInfo.h>
#include <Syscall/Numbers.h>

VOID   SyscallInitCurrentCPU(UINT32 CPUIndex);
VOID   SyscallSetKernelStack(UINT32 CPUIndex, UINT64 StackTop);
UINT64 SyscallDispatch(INTERRUPT_FRAME *Frame);