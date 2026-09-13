// kernel/src/scheduler/lbpwrr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/lbpwrr.h>
#include <scheduler/task.h>
#include <multicore/smp.h>
#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <memory/memory.h>

BOOLEAN LBPWRRInit(VOID)
{
    
}
BOOLEAN LBPWRRAddTask(TASK *task) {}
VOID    LBPWRRStart(VOID) {}
VOID    LBPWRRJoin(VOID) {}
VOID    LBPWRRYield(VOID) {}
UINT64  LBPWRRTick(INTERRUPT_FRAME *frame) {}
TASK   *LBPWRRGetCurrentTask(VOID) {}
UINTN   LBPWRRGetCPUUsage(UINT32 CPUID) {}
UINT32  LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID) {}