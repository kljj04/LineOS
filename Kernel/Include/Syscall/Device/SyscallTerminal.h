// Kernel/Include/Syscall/Device/SyscallTerminal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <Scheduler/TaskTypes.h>

INT64 TerminalRead(TASK *Task, BOOLEAN Master, UINT64 Buffer, UINT64 Count);
INT64 TerminalWrite(TASK *Task, BOOLEAN Master, UINT64 Buffer, UINT64 Count);
VOID  TerminalKey(UINT16 Code, BOOLEAN Pressed, UINT8 Modifiers);

BOOLEAN TerminalAttach(VOID);
VOID    TerminalDetach(VOID);

INT64 TerminalWaitOutput(UINT32 TimeoutMls);

INT64 TerminalGraphics(TASK *Task, BOOLEAN Begin);
VOID  TerminalGraphicsRelease(VOID *Owner);

INT64 TerminalPending(TASK *Task, BOOLEAN Master, UINT64 UserCount);

INT64 TerminalWindowSize(TASK *Task, UINT64 UserSize, BOOLEAN Set);

INT64 TerminalCompositor(TASK *Task, UINT64 UserEndpoint, BOOLEAN Set);

INT64 TerminalInjectCLI(TASK *Task, UINT64 UserByte);
