// Kernel/Include/Syscall/SyscallMemory.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Scheduler/TaskTypes.h>

INT64   SyscallMmap(UINT64 Address, UINT64 Length, UINT64 Protection, UINT64 Flags, INT64 FD, UINT64 Offset);
INT64   SyscallMunmap(UINT64 Address, UINT64 Length);
VOID    SyscallMemoryRelease(TASK *Task);
BOOLEAN SyscallMemoryShared(TASK *Task, UINT64 Address);
BOOLEAN SyscallMemoryClone(TASK *Child, TASK *Parent);
INT64   SyscallMapDevice(INT64 FD, UINT64 Length, UINT64 Protection, UINT64 Flags, UINT64 Offset, UINT64 *Physical);
