// kernel/src/timer/TSC.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <timer/TSC.h>
#include <interrupt/APIC.h>
#include <arch/AMD64/CPU.h>
#include <timer/HPET.h>
#include <debug/panic.h>
#include <LineOS/TypeInfo.h>

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

STATIC VOID TSCSleepTicks(UINT128 ticks)
{
    UINT64 flags;
    UINT64 chunk;
    UINT64 deadline;

    if (ticks == 0)
    {
        return;
    }

    ASM("pushfq\n"
        "popq %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory");

    while (ticks != 0)
    {
        chunk = ticks > INT64_MAX ? INT64_MAX : (UINT64) ticks;
        deadline = RDTSC() + chunk;

        if (deadline == 0)
        {
            deadline = 1;
        }

        TSCSetDeadline(deadline);

        while ((INT64) (RDTSC() - deadline) < 0)
        {
            STIHLT();
            CLI();
        }

        TSCSetDeadline(0);
        ticks -= chunk;
    }

    if ((flags & (1ULL << 9)) != 0)
    {
        STI();
    }
}

STATIC VOID TSCSleepDuration(UINT64 duration, UINT64 divisor)
{
    UINT128 ticks;

    if (duration == 0)
    {
        return;
    }

    if (TSCFrequency == 0)
    {
        Panic(L"TSC is not calibrated");
    }

    ticks = (((UINT128) TSCFrequency * duration) + divisor - 1) / divisor;
    TSCSleepTicks(ticks);
}

VOID SleepMls(UINT64 milliseconds)
{
    TSCSleepDuration(milliseconds, 1000ULL);
}

VOID SleepMis(UINT64 microseconds)
{
    TSCSleepDuration(microseconds, 1000000ULL);
}

VOID SleepNs(UINT64 nanoseconds)
{
    TSCSleepDuration(nanoseconds, 1000000000ULL);
}
