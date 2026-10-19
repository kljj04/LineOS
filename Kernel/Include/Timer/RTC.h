// Kernel/Include/Timer/RTC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define RTC_PORT_INDEX 0x70
#define RTC_PORT_DATA  0x71

#define RTC_REG_SECOND 0x00
#define RTC_REG_MINUTE 0x02
#define RTC_REG_HOUR   0x04
#define RTC_REG_DAY    0x07
#define RTC_REG_MONTH  0x08
#define RTC_REG_YEAR   0x09
#define RTC_REG_A      0x0A
#define RTC_REG_B      0x0B

#define RTC_UIP     0x80
#define RTC_BINARY  0x04
#define RTC_24_HOUR 0x02
#define RTC_PM      0x80

#define RTC_MAX_RETRIES 100000

typedef struct RTC_TIME
{
    UINT16 Year;
    UINT8  Month;
    UINT8  Day;
    UINT8  Hour;
    UINT8  Minute;
    UINT8  Second;
} RTC_TIME;

BOOLEAN RTCReadTime(RTC_TIME *Time);
BOOLEAN RTCIsUpdating(VOID);
