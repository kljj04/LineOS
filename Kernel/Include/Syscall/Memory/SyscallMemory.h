// Kernel/Include/Syscall/Memory/SyscallMemory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>
#include <Scheduler/TaskTypes.h>

INT64   SyscallMmap(SYSCALL_CONTEXT *Context);
INT64   SyscallMunmap(SYSCALL_CONTEXT *Context);
VOID    SyscallMemoryRelease(TASK *Task);
BOOLEAN SyscallMemoryShared(TASK *Task, UINT64 Address);
BOOLEAN SyscallMemoryClone(TASK *Child, TASK *Parent);
INT64   SyscallMapDevice(INT64 FD, UINT64 Length, UINT64 Protection, UINT64 Flags, UINT64 Offset, UINT64 *Physical);

INT64 SyscallMprotect(SYSCALL_CONTEXT *Context);

INT64 SyscallBrk(SYSCALL_CONTEXT *Context);
