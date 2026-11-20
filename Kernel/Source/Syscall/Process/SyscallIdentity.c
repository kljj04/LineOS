// Kernel/Source/Syscall/Process/SyscallIdentity.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Process/SyscallIdentity.h>
#include <FS/VFS/VFS.h>
#include <Scheduler/LBPWRR.h>
#include <Syscall/Errno.h>

STATIC UINT64 NextAuthSession;

INT64 SyscallSetUID(SYSCALL_CONTEXT *Context)
{
    UINT64 UID = Context->Args.RDI;

    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->FileContext == NULL || UID > UINT32_MAX)
    {
        return -EINVAL;
    }

    VFS_CONTEXT *FileContext = Task->Process->FileContext;

    if (FileContext->Credentials.UID != 0 && FileContext->Credentials.UID != (UINT32) UID)
    {
        return -EPERM;
    }

    if (Task->Process->AuthSession == 0)
    {
        Task->Process->AuthSession = ATOMIC_FETCH_ADD(&NextAuthSession, 1, ATOMIC_RELAXED) + 1;
    }

    Task->Process->RealUID                 = (UINT32) UID;
    FileContext->Credentials.UID           = (UINT32) UID;
    FileContext->Credentials.Administrator = UID == 0;

    return 0;
}

INT64 SyscallSetGID(SYSCALL_CONTEXT *Context)
{
    UINT64 GID = Context->Args.RDI;

    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->FileContext == NULL || GID > UINT32_MAX)
    {
        return -EINVAL;
    }

    VFS_CREDENTIALS *Credentials = &Task->Process->FileContext->Credentials;

    if (Credentials->UID != 0 && Credentials->GID != (UINT32) GID)
    {
        return -EPERM;
    }

    Task->Process->RealGID = (UINT32) GID;
    Credentials->GID       = (UINT32) GID;

    return 0;
}

INT64 SyscallGetUID(SYSCALL_CONTEXT *Context)
{
    BOOLEAN Effective = Context->Number == SYSCALL_GETEUID;

    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->Process->FileContext == NULL)
    {
        return -ESRCH;
    }

    return Effective ? Task->Process->FileContext->Credentials.UID : Task->Process->RealUID;
}
