// LOSLibC/Source/time.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <time.h>
#include <errno.h>
#include <stddef.h>
#include <syscall.h>

STATIC BOOLEAN TimeIsLeapYear(int Year)
{
    if (Year % 400 == 0)
    {
        return TRUE;
    }

    if (Year % 100 == 0)
    {
        return FALSE;
    }

    return Year % 4 == 0;
}

STATIC int TimeYearDays(int Year)
{
    if (TimeIsLeapYear(Year))
    {
        return 366;
    }

    return 365;
}

struct tm *gmtime_r(const time_t *Timer, struct tm *Result)
{
    STATIC CONST int MonthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (Timer == NULL || Result == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    long Days = *Timer / 86400;
    long Seconds = *Timer % 86400;

    if (Seconds < 0)
    {
        Seconds += 86400;
        Days--;
    }

    if (Days < -719528 || Days > 2932896)
    {
        errno = EOVERFLOW;
        return NULL;
    }

    Result->tm_sec = Seconds % 60;
    Result->tm_min = Seconds / 60 % 60;
    Result->tm_hour = Seconds / 3600;
    Result->tm_wday = (Days % 7 + 11) % 7;

    int Year = 1970;

    while (Days < 0)
    {
        Year--;
        Days += TimeYearDays(Year);
    }

    for (;;)
    {
        int Length = TimeYearDays(Year);
        if (Days < Length)
        {
            break;
        }
        Days -= Length;
        Year++;
    }

    Result->tm_year = Year - 1900;
    Result->tm_yday = Days;
    Result->tm_isdst = 0;
    Result->tm_gmtoff = 0;
    Result->tm_zone = "UTC";

    int Month = 0;

    for (; Month < 11; Month++)
    {
        int Length = MonthDays[Month];

        if (Month == 1 && TimeIsLeapYear(Year))
        {
            Length++;
        }

        if (Days < Length)
        {
            break;
        }
        Days -= Length;
    }

    Result->tm_mon = Month;
    Result->tm_mday = Days + 1;

    return Result;
}

_Static_assert(sizeof(struct timespec) == sizeof(TIME_SPEC), "timespec ABI size");
_Static_assert(__builtin_offsetof(struct timespec, tv_sec) == __builtin_offsetof(TIME_SPEC, Seconds), "timespec ABI seconds");
_Static_assert(__builtin_offsetof(struct timespec, tv_nsec) == __builtin_offsetof(TIME_SPEC, Nanoseconds), "timespec ABI nanoseconds");

int clock_gettime(clockid_t ClockID, struct timespec *Time)
{
    return (int) LineOSSyscallResult(LineOSSyscall2(SYSCALL_CLOCK_GETTIME, (UINT64) ClockID, (UINT64) Time));
}
