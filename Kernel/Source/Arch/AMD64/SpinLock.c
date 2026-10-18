// Kernel/Source/Arch/AMD64/SpinLock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Arch/AMD64/SpinLock.h>
#include <LineOS/TypeInfo.h>

VOID SpinLockInit(SPIN_LOCK *Lock)
{
    Lock->Locked = 0;
}

VOID SpinLockAcquire(SPIN_LOCK *Lock)
{
    UINT32 Value;

    while (TRUE)
    {
        Value = 1;

        ASM("xchgl %0, %1" : "+r"(Value), "+m"(Lock->Locked) : : "memory");

        if (Value == 0)
        {
            return;
        }

        while (Lock->Locked != 0)
        {
            ASM("pause" ::: "memory");
        }
    }
}

VOID SpinLockRelease(SPIN_LOCK *Lock)
{
    ASM("" ::: "memory");
    Lock->Locked = 0;
}

UINT64 SpinLockAcquireIRQSave(SPIN_LOCK *Lock)
{
    UINT64 Flags;

    ASM("pushfq\n"
        "popq %0\n"
        "cli"
        : "=r"(Flags)
        :
        : "memory");

    SpinLockAcquire(Lock);

    return Flags;
}

VOID SpinLockReleaseIRQRestore(SPIN_LOCK *Lock, UINT64 Flags)
{
    SpinLockRelease(Lock);

    if ((Flags & (1ULL << 9)) != 0)
    {
        ASM("sti" ::: "memory");
    }
}