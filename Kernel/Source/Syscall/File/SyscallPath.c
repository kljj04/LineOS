// Kernel/Source/Syscall/File/SyscallPath.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/SyscallPath.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

INT64 SyscallRename(UINT64 OldPath, UINT64 NewPath)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *OldName;
    CHAR8 *NewName;
    INT64  Result = SyscallCopyPath(Task, OldPath, &OldName);
    if (Result < 0)
    {
        return Result;
    }
    Result = SyscallCopyPath(Task, NewPath, &NewName);
    if (Result == 0)
    {
        Result = SyscallVFSError(VFSRename(Task->FileContext, OldName, NewName));
        KFree(NewName);
    }
    KFree(OldName);
    return Result;
}

INT64 SyscallRemove(UINT64 Path)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;
    INT64  Result = SyscallCopyPath(Task, Path, &Name);
    if (Result == 0)
    {
        Result = SyscallVFSError(VFSRemove(Task->FileContext, Name));
        KFree(Name);
    }
    return Result;
}

