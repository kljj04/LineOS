// Kernel/Source/Syscall/SyscallExit.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Syscall.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>

INT64 SyscallExit(UINT64 ExitCode)
{
    TASK *CurrentTask;

    CurrentTask = LBPWRRGetCurrentTask();

    if (CurrentTask == NULL || CurrentTask->Idle)
    {
        return -1;
    }

    TaskTerminate(CurrentTask);

    LBPWRRYield();

    return -1;
}