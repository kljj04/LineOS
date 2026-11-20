// Kernel/Source/Syscall/File/SyscallPath.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/SyscallPath.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

INT64 SyscallRename(SYSCALL_CONTEXT *Context)
{
    UINT64 OldPath = Context->Args.RDI;
    UINT64 NewPath = Context->Args.RSI;

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
        Result = SyscallVFSError(VFSRename(Task->Process->FileContext, OldName, NewName));
        KFree(NewName);
    }

    KFree(OldName);

    return Result;
}

INT64 SyscallRemove(SYSCALL_CONTEXT *Context)
{
    UINT64 Path = Context->Args.RDI;

    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;
    INT64  Result = SyscallCopyPath(Task, Path, &Name);

    if (Result == 0)
    {
        Result = SyscallVFSError(VFSRemove(Task->Process->FileContext, Name));
        KFree(Name);
    }

    return Result;
}
