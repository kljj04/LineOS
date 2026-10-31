
// Kernel/Source/Scheduler/Usage.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/Usage.h>
#include <Multicore/SMP.h>
#include <Scheduler/LBPWRRTypes.h>
#include <Arch/AMD64/CPU.h>
#include <Timer/TSC.h>

STATIC CPU_USAGE CPUUsages[SMP_MAX_CPUS];
STATIC UINT64    SampleIntervalTSC;
STATIC UINT32    ActiveCPUCount;
STATIC UINT64    LastTaskSampleTSC;
STATIC UINT32    TaskSampleLock;

VOID UsageInit(UINT32 CPUCount)
{
    UINT32 Index;
    UINT64 Frequency;

    if (CPUCount == 0)
    {
        CPUCount = 1;
    }

    if (CPUCount > SMP_MAX_CPUS)
    {
        CPUCount = SMP_MAX_CPUS;
    }

    ActiveCPUCount = CPUCount;

    Frequency = TSCGetFrequency();
    SampleIntervalTSC = Frequency / 10;

    if (SampleIntervalTSC == 0)
    {
        SampleIntervalTSC = 1;
    }

    for (Index = 0; Index < SMP_MAX_CPUS; Index++)
    {
        CPUUsages[Index] = (CPU_USAGE) {0};
    }

    LastTaskSampleTSC = RDTSC();
    TaskSampleLock = 0;
}

VOID UsageStart(UINT32 CPUID)
{
    if (CPUID >= ActiveCPUCount)
    {
        return;
    }

    CPUUsages[CPUID].LastTSC = RDTSC();
}

UINT64 UsageAccount(UINT32 CPUID, BOOLEAN Idle)
{
    CPU_USAGE *Usage;
    UINT64     Now;
    UINT64     Delta;

    if (CPUID >= ActiveCPUCount)
    {
        return 0;
    }

    Usage = &CPUUsages[CPUID];
    Now = RDTSC();

    if (Usage->LastTSC == 0)
    {
        Usage->LastTSC = Now;

        return 0;
    }

    Delta = Now - Usage->LastTSC;
    Usage->LastTSC = Now;

    if (Idle)
    {
        ATOMIC_FETCH_ADD(&Usage->IdleTSC, Delta, ATOMIC_RELAXED);
    }
    else
    {
        ATOMIC_FETCH_ADD(&Usage->BusyTSC, Delta, ATOMIC_RELAXED);
    }

    return Delta;
}

VOID UsageSample(UINT32 CPUID)
{
    CPU_USAGE *Usage;
    UINT64     Busy;
    UINT64     Idle;
    UINT64     Total;
    UINTN      Percent;

    if (CPUID >= ActiveCPUCount)
    {
        return;
    }

    Usage = &CPUUsages[CPUID];

    if (Usage->LastTSC == 0)
    {
        return;
    }

    Busy = Usage->BusyTSC - Usage->LastSampleBusyTSC;
    Idle = Usage->IdleTSC - Usage->LastSampleIdleTSC;
    Total = Busy + Idle;

    if (Total < SampleIntervalTSC)
    {
        return;
    }

    Percent = (UINTN) (((UINT128) Busy * 100) / Total);

    if (Percent > 100)
    {
        Percent = 100;
    }

    ATOMIC_STORE(&Usage->CPUUsage, Percent, ATOMIC_RELEASE);

    Usage->LastSampleBusyTSC = Usage->BusyTSC;
    Usage->LastSampleIdleTSC = Usage->IdleTSC;
}

VOID UsageSampleTasks(VOID)
{
    UINT64 Now;
    UINT64 Interval;

    if (ATOMIC_EXCHANGE(&TaskSampleLock, 1, ATOMIC_ACQUIRE) != 0)
    {
        return;
    }

    Now = RDTSC();
    Interval = Now - LastTaskSampleTSC;

    if (Interval >= SampleIntervalTSC)
    {
        LastTaskSampleTSC = Now;
        TaskSampleCPUUsage(Interval);
    }

    ATOMIC_STORE(&TaskSampleLock, 0, ATOMIC_RELEASE);
}

UINTN UsageGetCPU(UINT32 CPUID)
{
    if (CPUID >= ActiveCPUCount)
    {
        return 0;
    }

    return ATOMIC_LOAD(&CPUUsages[CPUID].CPUUsage, ATOMIC_ACQUIRE);
}

UINTN UsageGetTotal(VOID)
{
    UINT32 Index;
    UINTN  Total;

    if (ActiveCPUCount == 0)
    {
        return 0;
    }

    Total = 0;

    for (Index = 0; Index < ActiveCPUCount; Index++)
    {
        Total += UsageGetCPU(Index);
    }

    return Total / ActiveCPUCount;
}

VOID UsageGetCounters(UINT32 CPUID, UINT64 *Busy, UINT64 *Idle)
{
    *Busy = 0;
    *Idle = 0;

    if (CPUID < ActiveCPUCount)
    {
        *Busy = ATOMIC_LOAD(&CPUUsages[CPUID].BusyTSC, ATOMIC_ACQUIRE);
        *Idle = ATOMIC_LOAD(&CPUUsages[CPUID].IdleTSC, ATOMIC_ACQUIRE);
    }
}
