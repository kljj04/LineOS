// Kernel/Source/Syscall/SyscallTime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallTime.h>
#include <LineOS/Time.h>
#include <LineOS/Errno.h>
#include <Timer/Clock.h>
#include <Memory/VMM.h>
#include <Scheduler/LBPWRR.h>

_Static_assert(sizeof(TIME_SPEC) == 16, "timespec ABI");

INT64 SyscallClockGetTime(INT64 ClockID, UINT64 UserTime)
{
    TASK *Task = LBPWRRGetCurrentTask();
    CLOCK_TIME Time;
    BOOLEAN Result;
    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }
    switch (ClockID)
    {
        case LOS_CLOCK_REALTIME:
        {
            Result = ClockGetTime(&Time);
            break;
        }
        case LOS_CLOCK_MONOTONIC:
        {
            Result = ClockGetMonotonicTime(&Time);
            break;
        }
        default:
        {
            return -EINVAL;
        }
    }
    if (!Result)
    {
        return -EIO;
    }
    if (Time.Seconds > 0x7FFFFFFFFFFFFFFFULL)
    {
        return -EOVERFLOW;
    }
    TIME_SPEC Info = {(INT64) Time.Seconds, Time.Nanoseconds};
    return VMMCopyToUser(Task->AddressSpace, UserTime, &Info, sizeof(Info)) ? 0 : -EFAULT;
}
