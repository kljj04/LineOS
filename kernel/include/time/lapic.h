// lapic.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct
{
    UINT64 BaseAddress;
    UINT32 TicksPerMillisecond;
    BOOLEAN Available;
    BOOLEAN Calibrated;
} LINEOS_LAPIC_INFO;

VOID LAPICInit(VOID);
UINT32 LAPICRead(UINT32 offset);
VOID LAPICWrite(UINT32 offset, UINT32 value);
VOID LAPICTimerSetDivide(UINT32 value);
VOID LAPICTimerSetInitialCount(UINT32 value);
UINT32 LAPICTimerGetCurrentCount(VOID);
BOOLEAN LAPICTimerCalibrateWithHPET(UINT64 Milliseconds);
VOID LAPICTimerStartOneShot(UINT32 vector, UINT64 Milliseconds);
VOID LAPICTimerStartOneShotUs(UINT32 vector, UINT64 Microseconds);
VOID LAPICEOI(VOID);

extern LINEOS_LAPIC_INFO LAPICInfo;
