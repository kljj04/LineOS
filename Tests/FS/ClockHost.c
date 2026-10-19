// Tests/FS/ClockHost.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <stdlib.h>
#include <Timer/Clock.h>
#include <Timer/RTC.h>

STATIC RTC_TIME Current;
STATIC BOOLEAN  Available = TRUE;
STATIC UINT32   Checks;

UINT64 RDTSC(VOID)
{
    STATIC UINT64 Counter = 1000;
    return Counter++;
}

UINT64 TSCGetFrequency(VOID)
{
    return 1000000000000ULL;
}

BOOLEAN RTCReadTime(RTC_TIME *Time)
{
    *Time = Current;
    return Available;
}

STATIC VOID Check(BOOLEAN Condition)
{
    Checks++;

    if (!Condition)
    {
        fprintf(stderr, "Clock assertion %u failed\n", Checks);
        exit(1);
    }
}

int main(VOID)
{
    CLOCK_TIME      Time;
    CLOCK_DATE_TIME Date;

    Check(!ClockGetTime(&Time));
    Current = (RTC_TIME){.Year = 2024, .Month = 2, .Day = 29, .Hour = 12, .Minute = 34, .Second = 56};
    Check(ClockInit());
    Check(ClockGetUnixTime() == 1709210096ULL);
    Check(ClockGetTime(&Time) && Time.Nanoseconds < 1000000000);
    Check(ClockGetDateTime(&Date, 540) && Date.Year == 2024 && Date.Month == 2 && Date.Day == 29 && Date.Hour == 21 && Date.Minute == 34 && Date.Second == 56);
    Check(ClockGetDateTime(&Date, -720) && Date.Hour == 0);
    Check(!ClockGetDateTime(&Date, 1441));
    Current = (RTC_TIME){.Year = 2100, .Month = 2, .Day = 29};
    Check(!ClockInit());
    Check(ClockGetUnixTime() == 0);
    Current = (RTC_TIME){.Year = 2000, .Month = 2, .Day = 29};
    Check(ClockInit());
    Check(ClockGetUnixTime() == 951782400ULL);
    Current = (RTC_TIME){.Year = 1969, .Month = 12, .Day = 31};
    Check(!ClockInit());
    Available = FALSE;
    Check(!ClockInit());
    printf("PASS %u clock assertions\n", Checks);
    return 0;
}
