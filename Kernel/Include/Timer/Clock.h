// Kernel/Include/Timer/Clock.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

typedef struct CLOCK_TIME
{
    UINT64 Seconds;
    UINT32 Nanoseconds;
} CLOCK_TIME;
typedef struct CLOCK_DATE_TIME
{
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
} CLOCK_DATE_TIME;

BOOLEAN ClockInit(VOID);
BOOLEAN ClockGetTime(CLOCK_TIME *Time);
UINT64  ClockGetUnixTime(VOID);
BOOLEAN ClockGetDateTime(CLOCK_DATE_TIME *DateTime, INT32 UTCOffsetMinutes);