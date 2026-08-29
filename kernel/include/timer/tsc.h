// kernel/include/timer/tsc.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

BOOLEAN TSCCalibrate(VOID);
UINT64  TSCGetFrequency(VOID);
BOOLEAN IsTSCDeadlineSupported(VOID);
BOOLEAN TSCDeadlineInit(UINT8 vector);
VOID    TSCSetDeadline(UINT64 deadline);
VOID    SleepMls(UINT64 milliseconds);
VOID    SleepMis(UINT64 microseconds);
VOID    SleepNs(UINT64 nanoseconds);