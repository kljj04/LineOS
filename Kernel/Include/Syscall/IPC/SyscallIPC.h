// Kernel/Include/Syscall/IPC/SyscallIPC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/TaskTypes.h>

INT64   SyscallIPC(UINT64 Operation, UINT64 UserRequest, UINT64 Size);
VOID    SyscallIPCRelease(TASK *Task);
BOOLEAN SyscallIPCOwns(TASK *Task, UINT64 Endpoint);
