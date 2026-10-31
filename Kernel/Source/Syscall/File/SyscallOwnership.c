// Kernel/Source/Syscall/File/SyscallOwnership.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/SyscallOwnership.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

INT64 SyscallChown(UINT64 Path, UINT64 UID, UINT64 GID)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;

    if (Task == NULL || Task->FileContext == NULL)
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
        Result = SyscallVFSError(VFSSetOwner(Task->FileContext, Name, (UINT32) UID, (UINT32) GID));
        KFree(Name);
    }

    return Result;
}
