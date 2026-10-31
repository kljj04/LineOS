// Kernel/Source/Timer/TSC.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Timer/TSC.h>
#include <Interrupt/APIC.h>
#include <Arch/AMD64/CPU.h>
#include <Timer/HPET.h>
#include <Debug/Panic.h>
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
    UINT32 EAX;
    UINT32 EBX;
    UINT32 ECX;
    UINT32 EDX;

    CPUID(1, 0, &EAX, &EBX, &ECX, &EDX);

    return (ECX & (1U << 24)) != 0;
}

BOOLEAN TSCDeadlineInit(UINT8 Vector)
{
    if (!IsTSCDeadlineSupported())
    {
        Panic(L"TSC Deadline Timer is not supported");
    }

    LAPICSetTSCDeadlineTimer(Vector);

    if (TSCFrequency == 0)
    {
        return FALSE;
    }

    TSCSetDeadline(RDTSC() + TSCFrequency / 200);

    return TRUE;
}

VOID TSCSetDeadline(UINT64 Deadline)
{
    WriteMSR(IA32_TSC_DEADLINE, Deadline);
}

STATIC VOID TSCSleepTicks(UINT128 Ticks)
{
    UINT64 Flags;
    UINT64 Chunk;
    UINT64 Deadline;

    if (Ticks == 0)
    {
        return;
    }

    ASM("pushfq\n"
        "popq %0\n"
        "cli" : "=r"(Flags) : : "memory");

    while (Ticks != 0)
    {
        Chunk = Ticks > INT64_MAX ? INT64_MAX : (UINT64) Ticks;
        Deadline = RDTSC() + Chunk;

        if (Deadline == 0)
        {
            Deadline = 1;
        }

        TSCSetDeadline(Deadline);

        while ((INT64) (RDTSC() - Deadline) < 0)
        {
            STIHLT();
            CLI();
        }

        TSCSetDeadline(0);
        Ticks -= Chunk;
    }

    if ((Flags & (1ULL << 9)) != 0)
    {
        STI();
    }
}

STATIC VOID TSCSleepDuration(UINT64 Duration, UINT64 Divisor)
{
    UINT128 Ticks;

    if (Duration == 0)
    {
        return;
    }

    if (TSCFrequency == 0)
    {
        Panic(L"TSC is not calibrated");
    }

    Ticks = (((UINT128) TSCFrequency * Duration) + Divisor - 1) / Divisor;
    TSCSleepTicks(Ticks);
}

VOID SleepMls(UINT64 Milliseconds)
{
    TSCSleepDuration(Milliseconds, 1000ULL);
}

VOID SleepMis(UINT64 Microseconds)
{
    TSCSleepDuration(Microseconds, 1000000ULL);
}

VOID SleepNs(UINT64 Nanoseconds)
{
    TSCSleepDuration(Nanoseconds, 1000000000ULL);
}
