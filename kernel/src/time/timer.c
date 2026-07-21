// timer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <acpi/acpi.h>
#include <time/timer.h>

LINEOS_TIMER_INFO TimerInfo;

VOID TimerDetect(VOID)
{
    TimerInfo.TSCDeadlineAvailable = CPUInfo.TSC && CPUInfo.InvariantTSC && CPUInfo.APIC && CPUInfo.TSCDeadline;
    TimerInfo.LAPICTimerAvailable = CPUInfo.APIC;
    TimerInfo.HPETAvailable = HPETIsAvailable();
    TimerInfo.MainSource = TimerSourceNone;

    if (TimerInfo.TSCDeadlineAvailable)
    {
        TimerInfo.MainSource = TimerSourceTSCDeadline;
        return;
    }

    if (TimerInfo.LAPICTimerAvailable)
    {
        TimerInfo.MainSource = TimerSourceLAPICTimer;
        return;
    }

    if (TimerInfo.HPETAvailable)
    {
        TimerInfo.MainSource = TimerSourceHPET;
    }
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
