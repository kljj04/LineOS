// Kernel/Include/Syscall/IPC/SyscallIPC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <Scheduler/TaskTypes.h>

INT64   SyscallIPC(SYSCALL_CONTEXT *Context);
VOID    SyscallIPCRelease(TASK *Task);
BOOLEAN SyscallIPCOwns(TASK *Task, UINT64 Endpoint);
