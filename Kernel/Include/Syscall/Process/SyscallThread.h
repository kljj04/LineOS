// Kernel/Include/Syscall/Process/SyscallThread.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Interrupt/IDT.h>
#include <Scheduler/TaskTypes.h>

INT64 SyscallClone(INTERRUPT_FRAME *Frame);
INT64 SyscallClone3(INTERRUPT_FRAME *Frame);
INT64 SyscallGetTID(VOID);
INT64 SyscallFutex(UINT64 Address, UINT64 Operation, UINT64 Value, UINT64 Timeout, UINT64 Address2, UINT64 Value3);
INT64 SyscallSetTIDAddress(UINT64 Address);
INT64 SyscallArchPrctl(UINT64 Code, UINT64 Address);
INT64 SyscallSetRobustList(UINT64 Head, UINT64 Length);
INT64 SyscallGetRobustList(UINT64 TID, UINT64 HeadAddress, UINT64 LengthAddress);
INT64 SyscallExitGroup(UINT64 Status);
VOID  SyscallClearTIDAddress(TASK *Task);
VOID  SyscallReleaseRobustList(TASK *Task);

INT64 SyscallRTSigprocmask(INT32 How, UINT64 Set, UINT64 OldSet, UINT64 Size);
