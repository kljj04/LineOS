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
    BOOLEAN HPETAvailable;
    TIMER_SOURCE MainSource;
} LINEOS_TIMER_INFO;

VOID TimerDetect(VOID);
CONST CHAR16* TimerSourceName(TIMER_SOURCE source);

extern LINEOS_TIMER_INFO TimerInfo;
