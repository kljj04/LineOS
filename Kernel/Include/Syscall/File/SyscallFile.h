// Kernel/Include/Syscall/File/SyscallFile.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/Syscall.h>

#include <Scheduler/TaskTypes.h>

INT64   SyscallOpen(SYSCALL_CONTEXT *Context);
INT64   SyscallRead(SYSCALL_CONTEXT *Context);
INT64   SyscallFileWritev(SYSCALL_CONTEXT *Context);
INT64   SyscallFileWrite(SYSCALL_CONTEXT *Context);
INT64   SyscallClose(SYSCALL_CONTEXT *Context);
BOOLEAN SyscallFilesInherit(TASK *Child, TASK *Parent);
VOID    SyscallFilesRelease(TASK *Task);

INT64 SyscallOpenat(SYSCALL_CONTEXT *Context);
INT64 SyscallAccess(SYSCALL_CONTEXT *Context);
VOID  SyscallFilesExec(TASK *Task);

INT64 SyscallPread64(SYSCALL_CONTEXT *Context);

INT64 SyscallFcntl(SYSCALL_CONTEXT *Context);

INT64 SyscallPoll(SYSCALL_CONTEXT *Context);
