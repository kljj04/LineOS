// LOSLibC/Source/time.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <time.h>
#include <errno.h>
#include <stddef.h>

struct tm *gmtime_r(const time_t *Timer, struct tm *Result)
{
    static const int MonthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
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
        Days += 365 + (Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0));
    }
    for (;;)
    {
        int Length = 365 + (Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0));
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
    int Month = 0;
    for (; Month < 11; Month++)
    {
        int Length = MonthDays[Month] + (Month == 1 && Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0));
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
