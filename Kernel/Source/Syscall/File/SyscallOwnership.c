// Kernel/Source/Syscall/File/SyscallOwnership.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/SyscallOwnership.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

INT64 SyscallChown(SYSCALL_CONTEXT *Context)
{
    UINT64 Path = Context->Args.RDI;
    UINT64 UID  = Context->Args.RSI;
    UINT64 GID  = Context->Args.RDX;

    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;

    if (Task == NULL || Task->Process->FileContext == NULL)
    {
        return -ESRCH;
    }

    if (UID > UINT32_MAX || GID > UINT32_MAX)
    {
        return -EINVAL;
    }

    INT64 Result = SyscallCopyPath(Task, Path, &Name);

    if (Result == 0)
    {
        Result = SyscallVFSError(VFSSetOwner(Task->Process->FileContext, Name, (UINT32) UID, (UINT32) GID));
        KFree(Name);
    }

    return Result;
}
