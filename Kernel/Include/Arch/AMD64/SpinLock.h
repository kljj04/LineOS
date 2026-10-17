// kernel/include/arch/AMD64/SpinLock.h
// LineOS
// Project
// Copyright
// (C) 2026
// LineOS
// Developer
// kljj04

#pragma once

#include <LineOS/TypeInfo.h>

typedef struct SPIN_LOCK
{
    VOLATILE UINT32 Locked;
} SPIN_LOCK;

VOID   SpinLockInit(SPIN_LOCK *Lock);
VOID   SpinLockAcquire(SPIN_LOCK *Lock);
VOID   SpinLockRelease(SPIN_LOCK *Lock);
UINT64 SpinLockAcquireIRQSave(SPIN_LOCK *Lock);
VOID   SpinLockReleaseIRQRestore(SPIN_LOCK *Lock, UINT64 Flags);
