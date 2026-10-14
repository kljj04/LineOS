// kernel/src/scheduler/LBPWRRLoadRate.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/LBPWRR.h>
#include <scheduler/LBPWRRLoadRate.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <multicore/SMP.h>
#include <arch/AMD64/CPU.h>
#include <memory/memory.h>
#include <timer/TSC.h>

#define CPU_USAGE_SAMPLE_INTERVAL_DIVISOR 10ULL

STATIC CPU_USAGE CPUUsages[SMP_MAX_CPUS];

VOID LBPWRRInitCPUUsage(UINT32 CPUID)
{
    KMemSet(&CPUUsages[CPUID], 0, sizeof(CPU_USAGE));
    CPUUsages[CPUID].LastTSC = RDTSC();
}

VOID LBPWRRAccountCPUTime(UINT32 CPUID, BOOLEAN busy)
{
    UINT64 now;
    UINT64 delta;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    now = RDTSC();

    if (CPUUsages[CPUID].LastTSC == 0)
    {
        CPUUsages[CPUID].LastTSC = now;
        return;
    }

    if (now < CPUUsages[CPUID].LastTSC)
    {
        CPUUsages[CPUID].LastTSC = now;
        return;
    }

    delta = now - CPUUsages[CPUID].LastTSC;
    CPUUsages[CPUID].LastTSC = now;

    if (busy)
    {
        CPUUsages[CPUID].BusyTSC += delta;
    }
    else
    {
        CPUUsages[CPUID].IdleTSC += delta;
    }
}

STATIC VOID LBPWRRSampleCPUUsage(UINT32 CPUID, BOOLEAN busy)
{
    UINT64 BusyTSC;
    UINT64 IdleTSC;
    UINT64 BusyDelta;
    UINT64 IdleDelta;
    UINT128 TotalDelta;
    UINT64 now;
    UINT64 SampleInterval;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    BusyTSC = CPUUsages[CPUID].BusyTSC;
    IdleTSC = CPUUsages[CPUID].IdleTSC;
    now = RDTSC();

    if (CPUUsages[CPUID].LastTSC != 0 && now >= CPUUsages[CPUID].LastTSC)
    {
        if (busy)
        {
            BusyTSC += now - CPUUsages[CPUID].LastTSC;
        }
        else
        {
            IdleTSC += now - CPUUsages[CPUID].LastTSC;
        }
    }

    BusyDelta = BusyTSC - CPUUsages[CPUID].LastSampleBusyTSC;
    IdleDelta = IdleTSC - CPUUsages[CPUID].LastSampleIdleTSC;
    TotalDelta = (UINT128) BusyDelta + IdleDelta;
    SampleInterval = TSCGetFrequency() / CPU_USAGE_SAMPLE_INTERVAL_DIVISOR;

    if (TotalDelta == 0 || TotalDelta < SampleInterval)
    {
        return;
    }

    CPUUsages[CPUID].CPUUsage = (UINTN) (((UINT128) BusyDelta * 100ULL) / TotalDelta);
    CPUUsages[CPUID].LastSampleBusyTSC = BusyTSC;
    CPUUsages[CPUID].LastSampleIdleTSC = IdleTSC;
}

UINTN LBPWRRGetCPUUsage(UINT32 CPUID)
{
    LBPWRR_RUNQUEUE *RunQueue;
    UINT64           flags;
    UINTN            usage;

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return 0;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);
    flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    LBPWRRSampleCPUUsage(CPUID, LBPWRRGetCurrentTaskFromCPU(CPUID) != NULL);
    usage = CPUUsages[CPUID].CPUUsage;

    SpinLockReleaseIRQRestore(&RunQueue->Lock, flags);

    return usage;
}
