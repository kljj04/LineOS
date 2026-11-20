// Kernel/Source/Syscall/System/SyscallAffinity.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Multicore/SMP.h>
#include <Memory/VMM.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Syscall/Errno.h>
#include <Syscall/System/SyscallAffinity.h>

INT64 SyscallSchedGetAffinity(SYSCALL_CONTEXT *Context)
{
    INT64  PID      = (INT64) Context->Args.RDI;
    UINT64 Size     = Context->Args.RSI;
    UINT64 UserMask = Context->Args.RDX;

    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        return -ESRCH;
    }

    if (PID != 0)
    {
        return -ESRCH;
    }

    UINT64 Mask[4]  = {0};
    UINT32 CPUCount = SMPGetCPUCount();

    for (UINT32 Index = 0; Index < CPUCount && Index < SMP_MAX_CPUS; Index++)
    {
        CPU_INFO *CPU = SMPGetCPU(Index);

        if (CPU != NULL && CPU->Online)
        {
            Mask[Index / 64] |= 1ULL << (Index % 64);
        }
    }

    if (Size < sizeof(Mask))
    {
        return -EINVAL;
    }

    if (!VMMCopyToUser(Task->Process->AddressSpace, UserMask, Mask, sizeof(Mask)))
    {
        return -EFAULT;
    }

    return sizeof(Mask);
}