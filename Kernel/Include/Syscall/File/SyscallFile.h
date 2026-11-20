// Kernel/Include/Syscall/File/SyscallFile.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/TaskTypes.h>

INT64   SyscallOpen(UINT64 Path, UINT64 Flags, UINT64 Mode);
INT64   SyscallRead(UINT64 FD, UINT64 Buffer, UINT64 Count);
INT64   SyscallFileWritev(UINT64 FD, UINT64 Vectors, UINT64 Count);
INT64   SyscallFileWrite(UINT64 FD, UINT64 Buffer, UINT64 Count);
INT64   SyscallClose(UINT64 FD);
BOOLEAN SyscallFilesInherit(TASK *Child, TASK *Parent);
VOID    SyscallFilesRelease(TASK *Task);

INT64 SyscallOpenat(INT32 FD, UINT64 Path, UINT64 Flags, UINT64 Mode);
INT64 SyscallAccess(UINT64 Path, UINT64 Mode);
VOID  SyscallFilesExec(TASK *Task);

INT64 SyscallPread64(UINT64 FD, UINT64 Buffer, UINT64 Count, INT64 Offset);

INT64 SyscallFcntl(UINT64 FD, INT32 Command, UINT64 Argument);

INT64 SyscallPoll(UINT64 Descriptors, UINT64 Count, INT32 Timeout);
