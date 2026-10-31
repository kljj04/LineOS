// Kernel/Source/Syscall/Security/SyscallSudo.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Security/SyscallSudo.h>
#include <Syscall/Security/Sudo.h>
#include <Syscall/Errno.h>
#include <Scheduler/LBPWRR.h>
#include <Arch/AMD64/SpinLock.h>
#include <Timer/Clock.h>

#define SUDO_CACHE_LIMIT 128U

typedef struct
{
    UINT64 Session;
    UINT64 Expires;
    UINT32 UID;
} SUDO_CACHE_ENTRY;

STATIC SUDO_CACHE_ENTRY SudoCache[SUDO_CACHE_LIMIT];
STATIC SPIN_LOCK        SudoCacheLock;

INT64 SyscallSudoCache(UINT64 Operation)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }

    if (Operation > SUDO_CACHE_CLEAR)
    {
        return -EINVAL;
    }

    if (Operation == SUDO_CACHE_RECORD && Task->FileContext->Credentials.UID != 0)
    {
        return -EPERM;
    }

    if (Task->AuthSession == 0)
    {
        return 0;
    }

    UINT64            Now = ClockGetUptimeMls();
    UINT64            Flags = SpinLockAcquireIRQSave(&SudoCacheLock);
    SUDO_CACHE_ENTRY *Found = NULL;
    SUDO_CACHE_ENTRY *Oldest = &SudoCache[0];

    for (UINTN Index = 0; Index < SUDO_CACHE_LIMIT; Index++)
    {
        SUDO_CACHE_ENTRY *Entry = &SudoCache[Index];

        if (Entry->Session == Task->AuthSession && Entry->UID == Task->RealUID)
        {
            Found = Entry;
            break;
        }

        if (Entry->Expires < Oldest->Expires)
        {
            Oldest = Entry;
        }
    }

    INT64 Result = 0;

    switch (Operation)
    {
        case SUDO_CACHE_CHECK:
        {
            if (Found != NULL && Now < Found->Expires)
            {
                Result = 1;
            }

            break;
        }

        case SUDO_CACHE_RECORD:
        {
            if (Found == NULL)
            {
                Found = Oldest;
            }

            Found->Session = Task->AuthSession;
            Found->UID = Task->RealUID;
            Found->Expires = Now + SUDO_CACHE_TIMEOUT_MLS;
            break;
        }

        case SUDO_CACHE_CLEAR:
        {
            if (Found != NULL)
            {
                Found->Session = 0;
                Found->Expires = 0;
            }

            break;
        }
    }

    SpinLockReleaseIRQRestore(&SudoCacheLock, Flags);

    return Result;
}
