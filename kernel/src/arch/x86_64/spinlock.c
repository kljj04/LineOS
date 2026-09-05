// kernel/src/arch/x86_64/spinlock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/spinlock.h>
#include <lineos/typeinfo.h>

VOID SpinLockInit(SPIN_LOCK *lock)
{
    lock->Locked = 0;
}

VOID SpinLockAcquire(SPIN_LOCK *lock)
{
    UINT32 value;

    while (TRUE)
    {
        value = 1;

        ASM(
            "xchgl %0, %1"
            : "+r"(value), "+m"(lock->Locked)
            :
            : "memory"
        );

        if (value == 0)
        {
            return;
        }

        while (lock->Locked != 0)
        {
            ASM("pause" ::: "memory");
        }
    }
}

VOID SpinLockRelease(SPIN_LOCK *lock)
{
    ASM("" ::: "memory");
    lock->Locked = 0;
}

UINT64 SpinLockAcquireIRQSave(SPIN_LOCK *lock)
{
    UINT64 flags;

    ASM(
        "pushfq\n"
        "popq %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    SpinLockAcquire(lock);

    return flags;
}

VOID SpinLockReleaseIRQRestore(SPIN_LOCK *lock, UINT64 flags)
{
    SpinLockRelease(lock);

    if ((flags & (1ULL << 9)) != 0)
    {
        ASM("sti" ::: "memory");
    }
}