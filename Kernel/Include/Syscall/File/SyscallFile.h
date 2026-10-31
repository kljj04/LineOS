// Kernel/Include/Syscall/File/SyscallFile.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/TaskTypes.h>

INT64   SyscallOpen(UINT64 Path, UINT64 Flags, UINT64 Mode);
INT64   SyscallRead(UINT64 FD, UINT64 Buffer, UINT64 Count);
INT64   SyscallFileWrite(UINT64 FD, UINT64 Buffer, UINT64 Count);
INT64   SyscallClose(UINT64 FD);
BOOLEAN SyscallFilesInherit(TASK *Child, TASK *Parent);
VOID    SyscallFilesRelease(TASK *Task);
