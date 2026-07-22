// timer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <acpi/acpi.h>
#include <arch/x86_64/interrupt.h>
#include <time/hpet.h>
#include <time/lapic.h>
#include <time/timer.h>

#define TIMER_TSC_FALLBACK_TICKS_PER_MS 1000000ULL

LINEOS_TIMER_INFO TimerInfo;
STATIC volatile UINT64 TimerTicks;
STATIC volatile BOOLEAN TimerTickPending;

STATIC VOID DelayWithHPET(UINT64 Microseconds)
{
    UINT64 TargetTicks = (HPETMillisecondsToTicks(1) * Microseconds) / 1000;
    UINT64 StartCounter;

    if (TargetTicks == 0)
    {
        TargetTicks = 1;
    }

    StartCounter = HPETReadCounter();

    while (HPETReadCounter() - StartCounter < TargetTicks)
    {
        __asm__ volatile("pause");
    }
}

STATIC VOID DelayWithLAPICSleep(UINT64 Microseconds)
{
    TimerTickPending = FALSE;
    TimerStartOneShotUs(Microseconds);
    STI();

    while (!TimerTickPending)
    {
        __asm__ volatile("hlt");
    }

    TimerConsumeTick();
}

STATIC VOID DelayWithTSC(UINT64 Microseconds)
{
    UINT32 low;
    UINT32 high;
    UINT64 StartTSC;
    UINT64 TargetTicks = Microseconds * (TIMER_TSC_FALLBACK_TICKS_PER_MS / 1000);

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    StartTSC = ((UINT64)high << 32) | low;

    while (1)
    {
        UINT64 CurrentTSC;

        __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
        CurrentTSC = ((UINT64)high << 32) | low;

        if (CurrentTSC - StartTSC >= TargetTicks)
        {
            return;
        }

        __asm__ volatile("pause");
    }
}

VOID TimerInit(VOID)
{
    HPETInit();
    LAPICInit();
    LAPICTimerCalibrateWithHPET(10);
    TimerDetect();

    if (TimerInfo.LAPICTimerAvailable && TimerInfo.LAPICTimerCalibrated)
    {
        IDTInit();
    }
}

VOID TimerDetect(VOID)
{
    TimerInfo.TSCDeadlineAvailable = CPUInfo.TSC && CPUInfo.InvariantTSC && CPUInfo.APIC && CPUInfo.TSCDeadline;
    TimerInfo.LAPICTimerAvailable = LAPICInfo.Available;
    TimerInfo.LAPICTimerCalibrated = LAPICInfo.Calibrated;
    TimerInfo.HPETAvailable = HPETInfo.Available;
    TimerInfo.LAPICTicksPerMillisecond = LAPICInfo.TicksPerMillisecond;
    TimerInfo.MainSource = TimerSourceNone;

    if (TimerInfo.TSCDeadlineAvailable)
    {
        TimerInfo.MainSource = TimerSourceTSCDeadline;
        return;
    }

    if (TimerInfo.LAPICTimerAvailable && TimerInfo.LAPICTimerCalibrated)
    {
        TimerInfo.MainSource = TimerSourceLAPICTimer;
        return;
    }

    if (TimerInfo.HPETAvailable)
    {
        TimerInfo.MainSource = TimerSourceHPET;
    }
}

VOID DelayUs(UINT64 Microseconds)
{
    if (Microseconds == 0)
    {
        return;
    }

    if (TimerInfo.LAPICTimerAvailable && TimerInfo.LAPICTimerCalibrated)
    {
        DelayWithLAPICSleep(Microseconds);
        return;
    }

    if (TimerInfo.HPETAvailable)
    {
        DelayWithHPET(Microseconds);
        return;
    }

    DelayWithTSC(Microseconds);
}

VOID TimerStartOneShot(UINT64 Milliseconds)
{
    TimerStartOneShotUs(Milliseconds * 1000);
}

VOID TimerStartOneShotUs(UINT64 Microseconds)
{
    LAPICTimerStartOneShotUs(INTERRUPT_LAPIC_TIMER_VECTOR, Microseconds);
}

VOID TimerHandleTick(VOID)
{
    TimerTicks++;
    TimerTickPending = TRUE;
}

UINT64 TimerGetTicks(VOID)
{
    return TimerTicks;
}

BOOLEAN TimerConsumeTick(VOID)
{
    if (!TimerTickPending)
    {
        return FALSE;
    }

    TimerTickPending = FALSE;
    return TRUE;
}

CONST CHAR16* TimerSourceName(TIMER_SOURCE source)
{
    switch (source)
    {
    case TimerSourceTSCDeadline:
        return L"TSC Deadline";

    case TimerSourceLAPICTimer:
        return L"LAPIC Timer";

    case TimerSourceHPET:
        return L"HPET";

    default:
        return L"None";
    }
}
