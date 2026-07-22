// lapic.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <memory/memory.h>
#include <time/hpet.h>
#include <time/lapic.h>

#define IA32_APIC_BASE_MSR 0x0000001B
#define IA32_APIC_BASE_ENABLE 0x800
#define IA32_APIC_BASE_MASK 0xFFFFFFFFFFFFF000ULL

#define LAPIC_SPURIOUS_INTERRUPT_VECTOR 0x0F0
#define LAPIC_TIMER_LVT 0x320
#define LAPIC_TIMER_INITIAL_COUNT 0x380
#define LAPIC_TIMER_CURRENT_COUNT 0x390
#define LAPIC_TIMER_DIVIDE_CONFIGURATION 0x3E0
#define LAPIC_EOI 0x0B0

#define LAPIC_SPURIOUS_VECTOR 0xFF
#define LAPIC_TIMER_MASKED 0x00010000
#define LAPIC_TIMER_MODE_ONE_SHOT 0x00000000
#define LAPIC_TIMER_DIVIDE_16 0x3

LINEOS_LAPIC_INFO LAPICInfo;

VOID LAPICInit(VOID)
{
    UINT64 ApicBase;

    LAPICInfo.BaseAddress = 0;
    LAPICInfo.TicksPerMillisecond = 0;
    LAPICInfo.Available = FALSE;
    LAPICInfo.Calibrated = FALSE;

    if (!CPUInfo.APIC)
    {
        return;
    }

    ApicBase = RDMSR(IA32_APIC_BASE_MSR);
    LAPICInfo.BaseAddress = ApicBase & IA32_APIC_BASE_MASK;

    WRMSR(IA32_APIC_BASE_MSR, ApicBase | IA32_APIC_BASE_ENABLE);
    LAPICWrite(LAPIC_SPURIOUS_INTERRUPT_VECTOR, 0x100 | LAPIC_SPURIOUS_VECTOR);
    LAPICWrite(LAPIC_TIMER_LVT, LAPIC_TIMER_MASKED);
    LAPICTimerSetDivide(LAPIC_TIMER_DIVIDE_16);

    LAPICInfo.Available = TRUE;
}

UINT32 LAPICRead(UINT32 offset)
{
    return MMIORead32(LAPICInfo.BaseAddress + offset);
}

VOID LAPICWrite(UINT32 offset, UINT32 value)
{
    MMIOWrite32(LAPICInfo.BaseAddress + offset, value);
}

VOID LAPICTimerSetDivide(UINT32 value)
{
    LAPICWrite(LAPIC_TIMER_DIVIDE_CONFIGURATION, value);
}

VOID LAPICTimerSetInitialCount(UINT32 value)
{
    LAPICWrite(LAPIC_TIMER_INITIAL_COUNT, value);
}

UINT32 LAPICTimerGetCurrentCount(VOID)
{
    return LAPICRead(LAPIC_TIMER_CURRENT_COUNT);
}

BOOLEAN LAPICTimerCalibrateWithHPET(UINT64 Milliseconds)
{
    UINT64 StartCounter;
    UINT64 TargetTicks;
    UINT32 StartCount = 0xFFFFFFFFU;
    UINT32 CurrentCount;
    UINT32 ElapsedCount;

    if (!LAPICInfo.Available || !HPETInfo.Available || Milliseconds == 0)
    {
        return FALSE;
    }

    TargetTicks = HPETMillisecondsToTicks(Milliseconds);

    if (TargetTicks == 0)
    {
        return FALSE;
    }

    LAPICTimerSetDivide(LAPIC_TIMER_DIVIDE_16);
    LAPICTimerSetInitialCount(StartCount);

    StartCounter = HPETReadCounter();

    while (HPETReadCounter() - StartCounter < TargetTicks)
    {
        __asm__ volatile("pause");
    }

    CurrentCount = LAPICTimerGetCurrentCount();
    ElapsedCount = StartCount - CurrentCount;
    LAPICInfo.TicksPerMillisecond = ElapsedCount / (UINT32) Milliseconds;
    LAPICInfo.Calibrated = LAPICInfo.TicksPerMillisecond != 0;
    LAPICTimerSetInitialCount(0);
    return LAPICInfo.Calibrated;
}

VOID LAPICTimerStartOneShot(UINT32 vector, UINT64 Milliseconds)
{
    LAPICTimerStartOneShotUs(vector, Milliseconds * 1000);
}

VOID LAPICTimerStartOneShotUs(UINT32 vector, UINT64 Microseconds)
{
    UINT64 ticks;

    if (!LAPICInfo.Available || !LAPICInfo.Calibrated || Microseconds == 0)
    {
        return;
    }

    ticks = ((UINT64)LAPICInfo.TicksPerMillisecond * Microseconds) / 1000;

    if (ticks == 0)
    {
        ticks = 1;
    }

    if (ticks > 0xFFFFFFFFULL)
    {
        ticks = 0xFFFFFFFFULL;
    }

    LAPICTimerSetDivide(LAPIC_TIMER_DIVIDE_16);
    LAPICWrite(LAPIC_TIMER_LVT, LAPIC_TIMER_MODE_ONE_SHOT | vector);
    LAPICTimerSetInitialCount((UINT32)ticks);
}

VOID LAPICEOI(VOID)
{
    if (!LAPICInfo.Available)
    {
        return;
    }

    LAPICWrite(LAPIC_EOI, 0);
}
