// tsc.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct
{
    UINT64 FrequencyHz;
    UINT64 TicksPerMillisecond;
    BOOLEAN Calibrated;
} LINEOS_TSC_INFO;

UINT64 TSCRead(VOID);
VOID TSCInit(VOID);
VOID TSCDeadlineSet(UINT64 DeadlineTSC);
VOID TSCDeadlineClear(VOID);

extern LINEOS_TSC_INFO TSCInfo;
