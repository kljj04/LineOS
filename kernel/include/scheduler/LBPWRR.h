// kernel/include/scheduler/lbpwrr.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <interrupt/IDT.h>
#include <scheduler/TaskTypes.h>
#include <scheduler/LBPWRRTypes.h>

BOOLEAN LBPWRRInit(VOID);
BOOLEAN LBPWRRAddTask(TASK *task);
VOID    LBPWRRStart(VOID);
VOID    LBPWRRJoin(VOID);
VOID    LBPWRRYield(VOID);
UINT64  LBPWRRTick(INTERRUPT_FRAME *frame);
BOOLEAN LBPWRRMoveTask(TASK *task, UINT32 TargetCPUID);
BOOLEAN LBPWRRNotifyTaskChanged(TASK *task);
TASK   *LBPWRRGetTaskFromCPU(UINT32 CPUID);
TASK   *LBPWRRGetCurrentTask(VOID);
UINTN   LBPWRRGetCPUUsage(UINT32 CPUID);
UINT32  LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID);
