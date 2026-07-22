// hpet.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <acpi/acpi.h>

typedef struct
{
    UINT64 BaseAddress;
    UINT64 CounterClockPeriod;
    BOOLEAN Available;
} LINEOS_HPET_INFO;

VOID HPETInit(VOID);
UINT64 HPETReadCounter(VOID);
VOID HPETEnable(VOID);
UINT64 HPETMillisecondsToTicks(UINT64 Milliseconds);

extern LINEOS_HPET_INFO HPETInfo;
