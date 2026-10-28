// Kernel/Include/Syscall/SyscallError.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <FS/VFS/VFS.h>
#include <Scheduler/TaskTypes.h>
#include <LineOS/Errno.h>

INT64 SyscallVFSError(VFS_ERROR Error);
INT64 SyscallCopyPath(TASK *Task, UINT64 Path, CHAR8 **Name);
