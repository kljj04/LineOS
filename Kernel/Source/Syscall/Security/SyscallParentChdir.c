// Kernel/Source/Syscall/Security/SyscallParentChdir.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Security/SyscallParentChdir.h>
#include <Syscall/Process/ProcessInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

INT64 SyscallParentChdir(UINT64 Path)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }
    if (Task->FileContext->Credentials.UID != 0)
    {
        return -EPERM;
    }
    CHAR8 *Name;
    INT64 Result = SyscallCopyPath(Task, Path, &Name);
    if (Result < 0)
    {
        return Result;
    }
    Result = ProcessChangeParentDirectory(Task, Name);
    KFree(Name);
    return Result;
}
