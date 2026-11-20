// Kernel/Include/Syscall/Process/SyscallProcess.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <LineOS/TypeInfo.h>

#include <Interrupt/IDT.h>
INT64 SyscallFork(SYSCALL_CONTEXT *Context);
INT64 SyscallExecVE(SYSCALL_CONTEXT *Context);

#include <Scheduler/TaskTypes.h>
INT64 SyscallProcessExit(TASK *Task, UINT64 Code, BOOLEAN Group);
INT64 SyscallWait4(SYSCALL_CONTEXT *Context);
