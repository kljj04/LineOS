// Kernel/Include/Syscall/Process/SyscallThread.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <Interrupt/IDT.h>
#include <Scheduler/TaskTypes.h>

INT64 SyscallClone(SYSCALL_CONTEXT *Context);
INT64 SyscallClone3(SYSCALL_CONTEXT *Context);
INT64 SyscallGetTID(SYSCALL_CONTEXT *Context);
INT64 SyscallFutex(SYSCALL_CONTEXT *Context);
INT64 SyscallSetTIDAddress(SYSCALL_CONTEXT *Context);
INT64 SyscallArchPrctl(SYSCALL_CONTEXT *Context);
INT64 SyscallSetRobustList(SYSCALL_CONTEXT *Context);
INT64 SyscallGetRobustList(SYSCALL_CONTEXT *Context);
INT64 SyscallExitGroup(SYSCALL_CONTEXT *Context);
VOID  SyscallClearTIDAddress(TASK *Task);
VOID  SyscallReleaseRobustList(TASK *Task);

INT64 SyscallRTSigprocmask(SYSCALL_CONTEXT *Context);
