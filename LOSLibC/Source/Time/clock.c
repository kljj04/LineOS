// LOSLibC/Source/Time/clock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <time.h>
#include <syscall.h>

_Static_assert(sizeof(struct timespec) == sizeof(TIME_SPEC), "timespec ABI size");
_Static_assert(__builtin_offsetof(struct timespec, tv_sec) == __builtin_offsetof(TIME_SPEC, Seconds), "timespec ABI seconds");
_Static_assert(__builtin_offsetof(struct timespec, tv_nsec) == __builtin_offsetof(TIME_SPEC, Nanoseconds), "timespec ABI nanoseconds");

int clock_gettime(clockid_t ClockID, struct timespec *Time)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_CLOCK_GETTIME, (UINT64) ClockID, (UINT64) Time));
}

int nanosleep(const struct timespec *Duration, struct timespec *Remaining)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_NANOSLEEP, (UINT64) Duration, (UINT64) Remaining));
}
