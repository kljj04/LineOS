// kernel/include/scheduler/LBPWRRLoadRate.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

VOID LBPWRRInitCPUUsage(UINT32 CPUID);
VOID LBPWRRAccountCPUTime(UINT32 CPUID, BOOLEAN busy);
