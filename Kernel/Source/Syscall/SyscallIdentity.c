// Kernel/Source/Syscall/SyscallIdentity.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallFile.h>
#include <FS/VFS/VFS.h>
#include <Scheduler/LBPWRR.h>

INT64 SyscallSetUID(UINT64 UID)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->FileContext == NULL || UID > UINT32_MAX)
    {
        return -1;
    }
    VFS_CONTEXT *Context = Task->FileContext;
    if (Context->Credentials.UID != 0 && Context->Credentials.UID != (UINT32) UID)
    {
        return -1;
    }
    Context->Credentials.UID = (UINT32) UID;
    Context->Credentials.Administrator = UID == 0;
    if (UID != 0 && Context->AdministratorRoot)
    {
        Context->AdministratorRoot = FALSE;
        Context->Cwd[0] = '/';
        Context->Cwd[1] = 0;
    }
    return 0;
}

INT64 SyscallSetGID(UINT64 GID)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->FileContext == NULL || GID > UINT32_MAX)
    {
        return -1;
    }
    VFS_CREDENTIALS *Credentials = &Task->FileContext->Credentials;
    if (Credentials->UID != 0 && Credentials->GID != (UINT32) GID)
    {
        return -1;
    }
    Credentials->GID = (UINT32) GID;
    return 0;
}
