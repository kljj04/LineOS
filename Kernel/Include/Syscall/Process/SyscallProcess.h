// Kernel/Include/Syscall/Process/SyscallProcess.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#include <Interrupt/IDT.h>
INT64 SyscallFork(INTERRUPT_FRAME *Frame);
INT64 SyscallExecVE(INTERRUPT_FRAME *Frame);

#include <Scheduler/TaskTypes.h>
INT64 SyscallProcessExit(TASK *Task, UINT64 Code, BOOLEAN Group);
INT64 SyscallWait4(INT64 PID, UINT64 Status, UINT64 Options, UINT64 Usage);
