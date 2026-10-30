// Kernel/Include/Scheduler/Usage.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Scheduler/Task.h>

VOID   UsageInit(UINT32 CPUCount);
VOID   UsageStart(UINT32 CPUID);
UINT64 UsageAccount(UINT32 CPUID, BOOLEAN Idle);
VOID   UsageSample(UINT32 CPUID);
VOID   UsageSampleTasks(VOID);
UINTN  UsageGetCPU(UINT32 CPUID);
UINTN  UsageGetTotal(VOID);

VOID UsageGetCounters(UINT32 CPUID, UINT64 *Busy, UINT64 *Idle);
