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

#define CLOCK_NTP_INTERVAL_MLS 600000ULL
#ifndef CLOCK_NTP_SERVER
#define CLOCK_NTP_SERVER "pool.ntp.org"
#endif
#ifndef CLOCK_NTP_PORT
#define CLOCK_NTP_PORT 123
#endif

typedef struct
{
    BOOLEAN Synchronized;
    BOOLEAN Busy;
    UINT32  Failures;
    UINT32  ServerAddress;
    UINT32  RoundTripMls;
    INT64   OffsetNanoseconds;
    UINT64  LastSyncUptimeMls;
    UINT64  NextSyncUptimeMls;
} CLOCK_NTP_STATUS;

UINT64  ClockGetUptimeMls(VOID);
BOOLEAN ClockSyncNTP(CONST CHAR8 *Server, UINT32 TimeoutMls);
VOID    ClockGetNTPStatus(CLOCK_NTP_STATUS *Status);
VOID    ClockNTPTask(VOID);
