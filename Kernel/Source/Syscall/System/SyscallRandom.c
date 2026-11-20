// Kernel/Source/Syscall/System/SyscallRandom.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/System/SyscallRandom.h>
#include <Arch/AMD64/CPU.h>
#include <Memory/VMM.h>
#include <Memory/PMM.h>
#include <Scheduler/LBPWRR.h>
#include <Syscall/Errno.h>

INT64 SyscallGetRandom(SYSCALL_CONTEXT *Context)
{
    UINT64 Buffer = Context->Args.RDI;
    UINT64 Count  = Context->Args.RSI;
    UINT64 Flags  = Context->Args.RDX;

    TASK   *Task = LBPWRRGetCurrentTask();
    UINT8   Bytes[256];
    UINT32  EAX;
    UINT32  EBX;
    UINT32  ECX;
    UINT32  EDX;
    BOOLEAN Seed;
    BOOLEAN Random;
    INT64   Result = -EAGAIN;

    if (Flags != 0)
    {
        return -EINVAL;
    }

    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (Count != 0 && (Buffer == 0 || Buffer >= (1ULL << 47) || Count > (1ULL << 47) - Buffer))
    {
        return -EFAULT;
    }

    if (Count == 0)
    {
        return 0;
    }

    CPUID(0, 0, &EAX, &EBX, &ECX, &EDX);

    UINT32 MaxLeaf = EAX;

    CPUID(1, 0, &EAX, &EBX, &ECX, &EDX);
    Random = (ECX & (1U << 30)) != 0;
    Seed   = FALSE;

    if (MaxLeaf >= 7)
    {
        CPUID(7, 0, &EAX, &EBX, &ECX, &EDX);
        Seed = (EBX & (1U << 18)) != 0;
    }

    if (!Seed && !Random)
    {
        return -EOPNOTSUPP;
    }

    if (Count > sizeof(Bytes))
    {
        Count = sizeof(Bytes);
    }

    for (UINT64 Offset = 0; Offset < Count; Offset += 8)
    {
        UINT64 Value = 0;
        UINT8  Ready = 0;

        for (UINT32 Attempt = 0; Attempt < 256 && !Ready; Attempt++)
        {
            if (Seed)
            {
                ASM("rdseed %0; setc %1" : "=r"(Value), "=qm"(Ready) : : "cc");
            }
            else
            {
                ASM("rdrand %0; setc %1" : "=r"(Value), "=qm"(Ready) : : "cc");
            }

            if (!Ready)
            {
                PAUSE();
            }
        }

        if (!Ready)
        {
            goto Cleanup;
        }

        UINT64 Length = Count - Offset < 8 ? Count - Offset : 8;

        KMemCpy(Bytes + Offset, &Value, Length);
        Value = 0;
    }

    Result = -EFAULT;

    if (VMMCopyToUser(Task->Process->AddressSpace, Buffer, Bytes, Count))
    {
        Result = (INT64) Count;
    }

Cleanup:
    for (UINTN Index = 0; Index < sizeof(Bytes); Index++)
    {
        ((VOLATILE UINT8 *) Bytes)[Index] = 0;
    }

    return Result;
}
