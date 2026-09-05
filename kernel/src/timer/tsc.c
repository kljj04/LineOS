// kernel/src/timer/tsc.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <timer/tsc.h>
#include <interrupt/apic.h>
#include <arch/x86_64/cpu.h>
#include <timer/hpet.h>
#include <debug/panic.h>
#include <lineos/typeinfo.h>

#define TSC_CALIBRATION_HPET_TICKS 1000000ULL
#define IA32_TSC_DEADLINE          0x6E0

STATIC UINT64 TSCFrequency = 0;

BOOLEAN TSCCalibrate(VOID)
{
    UINT64 HPETFrequency;
    UINT64 HPETStart;
    UINT64 HPETEnd;
    UINT64 TSCStart;
    UINT64 TSCEnd;
    UINT64 HPETDelta;
    UINT64 TSCDelta;

    HPETFrequency = HPETGetFrequency();

    if (HPETFrequency == 0)
    {
        return FALSE;
    }

    HPETStart = HPETReadCounter();
    TSCStart = RDTSC();

    while ((HPETReadCounter() - HPETStart) < TSC_CALIBRATION_HPET_TICKS)
    {
        PAUSE();
    }

    TSCEnd = RDTSC();
    HPETEnd = HPETReadCounter();

    HPETDelta = HPETEnd - HPETStart;
    TSCDelta = TSCEnd - TSCStart;

    if (HPETDelta == 0)
    {
        return FALSE;
    }

    TSCFrequency = (TSCDelta * HPETFrequency) / HPETDelta;

    return TRUE;
}

UINT64 TSCGetFrequency(VOID)
{
    return TSCFrequency;
}

BOOLEAN IsTSCDeadlineSupported(VOID)
{
    UINT32 eax;
    UINT32 ebx;
    UINT32 ecx;
    UINT32 edx;

    CPUID(1, 0, &eax, &ebx, &ecx, &edx);

    return (ecx & (1U << 24)) != 0;
}

BOOLEAN TSCDeadlineInit(UINT8 vector)
{
    if (!IsTSCDeadlineSupported())
    {
        Panic(L"TSC Deadline Timer is not supported");
    }

    LAPICSetTSCDeadlineTimer(vector);

    return TRUE;
}
VOID TSCSetDeadline(UINT64 deadline)
{
    WriteMSR(IA32_TSC_DEADLINE, deadline);
}

VOID SleepMls(UINT64 milliseconds)
{
    UINT64 ticks;
    UINT64 deadline;

    ticks = (UINT64) (((UINT128) TSCFrequency * milliseconds) / 1000ULL);
    deadline = RDTSC() + ticks;
    TSCSetDeadline(deadline);

    while ((INT64) (RDTSC() - deadline) < 0)
    {
        PAUSE();
    }
}

VOID SleepMis(UINT64 microseconds)
{
    UINT64 ticks;
    UINT64 deadline;

    ticks = (UINT64) (((UINT128) TSCFrequency * microseconds) / 1000000ULL);
    deadline = RDTSC() + ticks;
    TSCSetDeadline(deadline);

    while ((INT64) (RDTSC() - deadline) < 0)
    {
        PAUSE();
    }
}

VOID SleepNs(UINT64 nanoseconds)
{
    UINT64 ticks;
    UINT64 deadline;

    ticks = (UINT64) (((UINT128) TSCFrequency * nanoseconds) / 1000000000ULL);
    deadline = RDTSC() + ticks;
    TSCSetDeadline(deadline);

    while ((INT64) (RDTSC() - deadline) < 0)
    {
        PAUSE();
    }
}
