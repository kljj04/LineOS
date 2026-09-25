// kernel/include/scheduler/lbpwrr.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <interrupt/idt.h>
#include <scheduler/task_types.h>
#include <scheduler/lbpwrr_types.h>

BOOLEAN LBPWRRInit(VOID);
BOOLEAN LBPWRRAddTask(TASK *task);
VOID    LBPWRRStart(VOID);
VOID    LBPWRRJoin(VOID);
VOID    LBPWRRYield(VOID);
UINT64  LBPWRRTick(INTERRUPT_FRAME *frame);
BOOLEAN LBPWRRMoveTask(TASK *task, UINT32 TargetCPUID);
TASK   *LBPWRRGetTaskFromCPU(UINT32 CPUID);
TASK   *LBPWRRGetCurrentTask(VOID);
UINTN   LBPWRRGetCPUUsage(UINT32 CPUID);
UINT32  LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID);