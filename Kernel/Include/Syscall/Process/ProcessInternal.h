// Kernel/Include/Syscall/Process/ProcessInternal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/TaskTypes.h>

INT64 ProcessChangeParentDirectory(TASK *Task, CONST CHAR8 *Name);

BOOLEAN ProcessBegin(VOID);
INT64   ProcessEnd(INT64 Result);
VOID    ProcessReapThreads(PROCESS *Process);
