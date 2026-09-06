// kernel/include/scheduler/lbpwrr.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>
#include <interrupt/idt.h>

BOOLEAN LBPWRRInit(VOID);
BOOLEAN LBPWRRCreateTask(VOID (*entry)(VOID));
VOID    StartSchedule(VOID);
VOID    APJoinSchedule(VOID);
VOID    Yield(VOID);
VOID    LBPWRRTick(INTERRUPT_FRAME *frame);
VOID    LBPWRRRecordIdleTSC(UINT64 StartTSC, UINT64 EndTSC);
UINT64  LBPWRRGetSwitchStack(INTERRUPT_FRAME *frame);
UINT64  LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID);
UINTN   LBPWRRGetCPUUsage(UINT32 CPUID);
