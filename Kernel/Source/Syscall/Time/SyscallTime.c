// Kernel/Source/Syscall/Time/SyscallTime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Time/SyscallTime.h>
#include <Syscall/Time/Time.h>
#include <Syscall/Errno.h>
#include <Timer/Clock.h>
#include <Memory/VMM.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>

_Static_assert(sizeof(TIME_SPEC) == 16, "timespec ABI");

INT64 SyscallClockGetTime(INT64 ClockID, UINT64 UserTime)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    CLOCK_TIME Time;
    BOOLEAN    Result;

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

INT64 SyscallNanosleep(UINT64 Duration, UINT64 Remaining)
{
    (VOID) Remaining;

    TASK     *Task = LBPWRRGetCurrentTask();
    TIME_SPEC Time;

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (!VMMCopyFromUser(Task->AddressSpace, &Time, Duration, sizeof(Time)))
    {
        return -EFAULT;
    }

    if (Time.Seconds < 0 || Time.Nanoseconds < 0 || Time.Nanoseconds >= 1000000000)
    {
        return -EINVAL;
    }

    if ((UINT64) Time.Seconds > (UINT64_MAX - (UINT64) Time.Nanoseconds) / 1000000000ULL)
    {
        return -EOVERFLOW;
    }

    TaskSleepNs((UINT64) Time.Seconds * 1000000000ULL + Time.Nanoseconds);

    return 0;
}
