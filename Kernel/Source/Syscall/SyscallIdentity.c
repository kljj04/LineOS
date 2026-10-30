// Kernel/Source/Syscall/SyscallIdentity.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallFile.h>
#include <FS/VFS/VFS.h>
#include <Scheduler/LBPWRR.h>
#include <LineOS/Errno.h>

STATIC UINT64 NextAuthSession;

INT64 SyscallSetUID(UINT64 UID)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->FileContext == NULL || UID > UINT32_MAX)
    {
        return -EINVAL;
    }
    VFS_CONTEXT *Context = Task->FileContext;
    if (Context->Credentials.UID != 0 && Context->Credentials.UID != (UINT32) UID)
    {
        return -EPERM;
    }
    if (Task->AuthSession == 0)
    {
        Task->AuthSession = ATOMIC_FETCH_ADD(&NextAuthSession, 1, ATOMIC_RELAXED) + 1;
    }
    Task->RealUID = (UINT32) UID;
    Context->Credentials.UID = (UINT32) UID;
    Context->Credentials.Administrator = UID == 0;
    return 0;
}

INT64 SyscallSetGID(UINT64 GID)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->FileContext == NULL || GID > UINT32_MAX)
    {
        return -EINVAL;
    }
    VFS_CREDENTIALS *Credentials = &Task->FileContext->Credentials;
    if (Credentials->UID != 0 && Credentials->GID != (UINT32) GID)
    {
        return -EPERM;
    }
    Task->RealGID = (UINT32) GID;
    Credentials->GID = (UINT32) GID;
    return 0;
}

INT64 SyscallGetUID(BOOLEAN Effective)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }
    return Effective ? Task->FileContext->Credentials.UID : Task->RealUID;
}
