// timer.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef enum
{
    TimerSourceNone,
    TimerSourceTSCDeadline,
    TimerSourceLAPICTimer,
    TimerSourceHPET
} TIMER_SOURCE;

typedef struct
{
    BOOLEAN TSCDeadlineAvailable;
    BOOLEAN LAPICTimerAvailable;
    BOOLEAN LAPICTimerCalibrated;
    BOOLEAN HPETAvailable;
    UINT32 LAPICTicksPerMillisecond;
    UINT64 TSCFrequencyHz;
    UINT64 TSCTicksPerMillisecond;
    TIMER_SOURCE MainSource;
} LINEOS_TIMER_INFO;

VOID TimerInit(VOID);
VOID TimerDetect(VOID);
VOID DelayUs(UINT64 microseconds);
VOID TimerStartOneShot(UINT64 milliseconds);
VOID TimerStartOneShotUs(UINT64 microseconds);
VOID TimerHandleTick(VOID);
UINT64 TimerGetTicks(VOID);
BOOLEAN TimerConsumeTick(VOID);
CONST CHAR16* TimerSourceName(TIMER_SOURCE source);

#define DelayMs(milliseconds) DelayUs((UINT64)((milliseconds) * 1000))

extern LINEOS_TIMER_INFO TimerInfo;
