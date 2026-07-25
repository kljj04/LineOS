// timer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <acpi/acpi.h>
#include <arch/x86_64/interrupt.h>
#include <time/hpet.h>
#include <time/lapic.h>
#include <time/timer.h>
#include <time/tsc.h>

#define TIMER_TSC_FALLBACK_TICKS_PER_MS 1000000ULL

LINEOS_TIMER_INFO TimerInfo;
STATIC volatile UINT64 TimerTicks;
STATIC volatile BOOLEAN TimerTickPending;

STATIC VOID DelayWithHPET(UINT64 microseconds)
{
    UINT64 TargetTicks = (HPETMillisecondsToTicks(1) * microseconds) / 1000;
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

STATIC VOID DelayWithLAPICSleep(UINT64 microseconds)
{
    TimerTickPending = FALSE;
    TimerStartOneShotUs(microseconds);
    STI();

    while (!TimerTickPending)
    {
        __asm__ volatile("hlt");
    }

    TimerConsumeTick();
}

STATIC VOID DelayWithTSC(UINT64 microseconds)
{
    UINT64 StartTSC;
    UINT64 CurrentTSC;
    UINT64 TargetTicks = microseconds * (TIMER_TSC_FALLBACK_TICKS_PER_MS / 1000);

    StartTSC = TSCRead();

    while (1)
    {
        CurrentTSC = TSCRead();

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
    TSCInit();
    LAPICTimerCalibrateWithHPET(10);
    TimerDetect();

    if (TimerInfo.MainSource == TimerSourceTSCDeadline ||
        (TimerInfo.LAPICTimerAvailable && TimerInfo.LAPICTimerCalibrated))
    {
        IDTInit();
    }
}

VOID TimerDetect(VOID)
{
    TimerInfo.TSCDeadlineAvailable = CPUInfo.TSC && CPUInfo.InvariantTSC && CPUInfo.APIC && CPUInfo.TSCDeadline && LAPICInfo.Available;
    TimerInfo.LAPICTimerAvailable = LAPICInfo.Available;
    TimerInfo.LAPICTimerCalibrated = LAPICInfo.Calibrated;
    TimerInfo.HPETAvailable = HPETInfo.Available;
    TimerInfo.LAPICTicksPerMillisecond = LAPICInfo.TicksPerMillisecond;
    TimerInfo.TSCFrequencyHz = TSCInfo.FrequencyHz;
    TimerInfo.TSCTicksPerMillisecond = TSCInfo.TicksPerMillisecond;
    TimerInfo.MainSource = TimerSourceNone;

    TimerInfo.TSCDeadlineAvailable = TimerInfo.TSCDeadlineAvailable && TSCInfo.Calibrated;

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

VOID DelayUs(UINT64 microseconds)
{
    if (microseconds == 0)
    {
        return;
    }

    if (TimerInfo.MainSource == TimerSourceTSCDeadline ||
        (TimerInfo.LAPICTimerAvailable && TimerInfo.LAPICTimerCalibrated))
    {
        DelayWithLAPICSleep(microseconds);
        return;
    }

    if (TimerInfo.HPETAvailable)
    {
        DelayWithHPET(microseconds);
        return;
    }

    DelayWithTSC(microseconds);
}

VOID TimerStartOneShot(UINT64 milliseconds)
{
    TimerStartOneShotUs(milliseconds * 1000);
}

VOID TimerStartOneShotUs(UINT64 microseconds)
{
    UINT64 ticks;

    if (microseconds == 0)
    {
        return;
    }

    if (TimerInfo.MainSource == TimerSourceTSCDeadline)
    {
        ticks = (TimerInfo.TSCTicksPerMillisecond * microseconds) / 1000;
        if (ticks == 0)
        {
            ticks = 1;
        }

        LAPICTimerStartTSCDeadline(INTERRUPT_LAPIC_TIMER_VECTOR, TSCRead() + ticks);
        return;
    }

    LAPICTimerStartOneShotUs(INTERRUPT_LAPIC_TIMER_VECTOR, microseconds);
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
