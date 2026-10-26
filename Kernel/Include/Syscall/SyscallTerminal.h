// Kernel/Include/Syscall/SyscallTerminal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Scheduler/TaskTypes.h>

INT64 TerminalRead(TASK *Task, BOOLEAN Master, UINT64 Buffer, UINT64 Count);
INT64 TerminalWrite(TASK *Task, BOOLEAN Master, UINT64 Buffer, UINT64 Count);
VOID  TerminalKey(UINT16 Code, BOOLEAN Pressed, UINT8 Modifiers);

BOOLEAN TerminalAttach(VOID);
VOID    TerminalDetach(VOID);
