// Kernel/Source/Syscall/Process/SyscallExit.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Process/SyscallExit.h>
#include <Syscall/Process/SyscallProcess.h>
#include <Scheduler/LBPWRR.h>

INT64 SyscallExit(UINT64 ExitCode)
{
    return SyscallProcessExit(LBPWRRGetCurrentTask(), ExitCode, FALSE);
}

VOID SyscallCheckGroupExit(VOID)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task != NULL && ATOMIC_LOAD(&Task->Process->GroupExiting, ATOMIC_ACQUIRE))
    {
        SyscallProcessExit(Task, 0, FALSE);
    }
}
