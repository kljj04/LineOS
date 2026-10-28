// Kernel/Source/Syscall/SyscallExit.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Syscall.h>
#include <Syscall/SyscallFile.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Debug/Debug.h>
#include <LineOS/Errno.h>

INT64 SyscallExit(UINT64 ExitCode)
{
    TASK *CurrentTask;

    CurrentTask = LBPWRRGetCurrentTask();
    DebugWrite("exit: pid=");
    DebugWriteDec(CurrentTask == NULL ? 0 : CurrentTask->PID);
    DebugWrite(" code=");
    DebugWriteDec(ExitCode);
    DebugWrite("\n");

    if (CurrentTask == NULL || CurrentTask->Idle)
    {
        return -ESRCH;
    }

    SyscallFilesRelease(CurrentTask);
    TaskTerminate(CurrentTask);

    LBPWRRYield();

    return -EIO;
}
