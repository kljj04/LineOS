// Kernel/Source/Timer/Clock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Timer/Clock.h>
#include <Timer/RTC.h>
#include <Timer/TSC.h>
#include <Arch/AMD64/CPU.h>

#define CLOCK_NS_PER_SECOND   1000000000ULL
#define CLOCK_SECONDS_PER_DAY 86400ULL

STATIC UINT64  ClockBaseUnix;
STATIC UINT64  ClockBaseTSC;
STATIC UINT64  ClockFrequency;
STATIC BOOLEAN ClockReady = FALSE;

STATIC BOOLEAN ClockIsLeapYear(UINT32 Year)
{
    return (Year % 4 == 0) && ((Year % 100 != 0) || (Year % 400 == 0));
}

STATIC UINT32 ClockDaysInMonth(UINT32 Year, UINT32 Month)
{
    STATIC CONST UINT8 Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (Month == 2 && ClockIsLeapYear(Year))
    {
        return 29;
    }

    return Days[Month - 1];
}

STATIC BOOLEAN ClockToUnixTime(CONST RTC_TIME *Time, UINT64 *Seconds)
{
    UINT64 Days = 0;
    UINT32 Year;
    UINT32 Month;

    if (Time->Year < 1970 || Time->Month < 1 || Time->Month > 12 || Time->Hour > 23 || Time->Minute > 59 || Time->Second > 59)
    {
        return FALSE;
    }

    if (Time->Day < 1 || Time->Day > ClockDaysInMonth(Time->Year, Time->Month))
    {
        return FALSE;
    }

    for (Year = 1970; Year < Time->Year; Year++)
    {
        Days += ClockIsLeapYear(Year) ? 366 : 365;
    }

    for (Month = 1; Month < Time->Month; Month++)
    {
        Days += ClockDaysInMonth(Time->Year, Month);
    }

    Days += Time->Day - 1;

    *Seconds = Days * CLOCK_SECONDS_PER_DAY + (UINT64) Time->Hour * 3600 + (UINT64) Time->Minute * 60 + Time->Second;

    return TRUE;
}

BOOLEAN ClockInit(VOID)
{
    RTC_TIME Time;
    UINT64   Before;
    UINT64   After;
    UINT64   UnixTime;
    UINT64   Frequency;

    ClockReady = FALSE;

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return FALSE;
    }

    Before = RDTSC();

    if (!RTCReadTime(&Time))
    {
        return FALSE;
    }

    After = RDTSC();

    if (!ClockToUnixTime(&Time, &UnixTime))
    {
        return FALSE;
    }

    ClockBaseUnix = UnixTime;
    ClockBaseTSC = Before + (After - Before) / 2;
    ClockFrequency = Frequency;
    ClockReady = TRUE;

    return TRUE;
}

BOOLEAN ClockGetTime(CLOCK_TIME *Time)
{
    UINT64 Now;
    UINT64 Delta;
    UINT64 Seconds;
    UINT64 Remainder;
    UINT64 Frequency;

    if (Time == NULL || !ClockReady)
    {
        return FALSE;
    }

    Frequency = ClockFrequency;
    Now = RDTSC();

    if (Now < ClockBaseTSC)
    {
        return FALSE;
    }

    Delta = Now - ClockBaseTSC;
    Seconds = Delta / Frequency;
    Remainder = Delta % Frequency;

    Time->Seconds = ClockBaseUnix + Seconds;
    Time->Nanoseconds = (UINT32) (((UINT128) Remainder * CLOCK_NS_PER_SECOND) / Frequency);

    return TRUE;
}

UINT64 ClockGetUnixTime(VOID)
{
    CLOCK_TIME Time;

    if (!ClockGetTime(&Time))
    {
        return 0;
    }

    return Time.Seconds;
}

BOOLEAN ClockGetDateTime(CLOCK_DATE_TIME *DateTime, INT32 UTCOffsetMinutes)
{
    CLOCK_TIME Time;
    INT64      Seconds;
    INT64      Days;
    INT64      Remaining;
    UINT32     Year = 1970;
    UINT32     Month = 1;
    UINT32     DaysInYear;
    UINT32     DaysInMonth;

    if (DateTime == NULL || !ClockGetTime(&Time))
    {
        return FALSE;
    }

    if (UTCOffsetMinutes < -1440 || UTCOffsetMinutes > 1440)
    {
        return FALSE;
    }

    Seconds = (INT64) Time.Seconds + (INT64) UTCOffsetMinutes * 60;

    if (Seconds < 0)
    {
        return FALSE;
    }

    Days = Seconds / 86400;
    Remaining = Seconds % 86400;

    while (TRUE)
    {
        DaysInYear = ClockIsLeapYear(Year) ? 366 : 365;

        if (Days < DaysInYear)
        {
            break;
        }

        Days -= DaysInYear;
        Year++;

        if (Year > UINT16_MAX)
        {
            return FALSE;
        }
    }

    while (TRUE)
    {
        DaysInMonth = ClockDaysInMonth(Year, Month);

        if (Days < DaysInMonth)
        {
            break;
        }

        Days -= DaysInMonth;
        Month++;
    }

    DateTime->Year = (UINT16) Year;
    DateTime->Month = (UINT8) Month;
    DateTime->Day = (UINT8) (Days + 1);
    DateTime->Hour = (UINT8) (Remaining / 3600);
    DateTime->Minute = (UINT8) ((Remaining % 3600) / 60);
    DateTime->Second = (UINT8) (Remaining % 60);

    return TRUE;
}