// Kernel/Include/Scheduler/LBPWRRRunQueue.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Scheduler/LBPWRRTypes.h>

VOID             LBPWRRInitRunQueues(UINT32 CPUCount);
UINT32           LBPWRRGetActiveCPUCount(VOID);
LBPWRR_RUNQUEUE *LBPWRRGetRunQueue(UINT32 CPUID);
TASK            *LBPWRRGetCurrentTaskFromCPU(UINT32 CPUID);
VOID             LBPWRRSetCurrentTask(UINT32 CPUID, TASK *Task);

VOID *LBPWRRGrowArray(VOID *Array, UINT32 *Capacity, UINT32 Required, UINTN ElementSize);
