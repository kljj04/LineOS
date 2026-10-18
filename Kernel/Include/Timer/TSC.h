// Kernel/Include/Timer/TSC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

BOOLEAN TSCCalibrate(VOID);
UINT64  TSCGetFrequency(VOID);
BOOLEAN IsTSCDeadlineSupported(VOID);
BOOLEAN TSCDeadlineInit(UINT8 Vector);
VOID    TSCSetDeadline(UINT64 Deadline);
VOID    SleepMls(UINT64 Milliseconds);
VOID    SleepMis(UINT64 Microseconds);
VOID    SleepNs(UINT64 Nanoseconds);