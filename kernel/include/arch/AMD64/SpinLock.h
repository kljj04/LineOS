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

VOID   SpinLockInit(SPIN_LOCK *lock);
VOID   SpinLockAcquire(SPIN_LOCK *lock);
VOID   SpinLockRelease(SPIN_LOCK *lock);
UINT64 SpinLockAcquireIRQSave(SPIN_LOCK *lock);
VOID   SpinLockReleaseIRQRestore(SPIN_LOCK *lock, UINT64 flags);
