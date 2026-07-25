// tsc.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <time/hpet.h>
#include <time/tsc.h>

#define CPUID_TSC_CRYSTAL_LEAF 0x00000015U
#define IA32_TSC_DEADLINE_MSR 0x000006E0
#define TSC_CALIBRATION_MS 10ULL

LINEOS_TSC_INFO TSCInfo;

UINT64 TSCRead(VOID)
{
    UINT32 low;
    UINT32 high;

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((UINT64)high << 32) | low;
}

STATIC BOOLEAN TSCDetectFrequencyWithCPUID(VOID)
{
    UINT32 EAX;
    UINT32 EBX;
    UINT32 ECX;
    UINT32 EDX;

    if (CPUInfo.MaxLeaf < CPUID_TSC_CRYSTAL_LEAF)
    {
        return FALSE;
    }

    CPUID(CPUID_TSC_CRYSTAL_LEAF, 0, &EAX, &EBX, &ECX, &EDX);
    (VOID)EDX;

    if (EAX == 0 || EBX == 0 || ECX == 0)
    {
        return FALSE;
    }

    TSCInfo.FrequencyHz = ((UINT64)ECX * EBX) / EAX;
    TSCInfo.TicksPerMillisecond = TSCInfo.FrequencyHz / 1000;
    TSCInfo.Calibrated = TSCInfo.TicksPerMillisecond != 0;
    return TSCInfo.Calibrated;
}

STATIC BOOLEAN TSCCalibrateWithHPET(UINT64 milliseconds)
{
    UINT64 StartCounter;
    UINT64 TargetHPETTicks;
    UINT64 StartTSC;
    UINT64 EndTSC;
    UINT64 ElapsedTSC;

    if (!CPUInfo.TSC || !HPETInfo.Available || milliseconds == 0)
    {
        return FALSE;
    }

    TargetHPETTicks = HPETMillisecondsToTicks(milliseconds);
    if (TargetHPETTicks == 0)
    {
        return FALSE;
    }

    StartCounter = HPETReadCounter();
    StartTSC = TSCRead();

    while (HPETReadCounter() - StartCounter < TargetHPETTicks)
    {
        __asm__ volatile("pause");
    }

    EndTSC = TSCRead();
    ElapsedTSC = EndTSC - StartTSC;

    TSCInfo.TicksPerMillisecond = ElapsedTSC / milliseconds;
    TSCInfo.FrequencyHz = TSCInfo.TicksPerMillisecond * 1000;
    TSCInfo.Calibrated = TSCInfo.TicksPerMillisecond != 0;
    return TSCInfo.Calibrated;
}

VOID TSCInit(VOID)
{
    TSCInfo.FrequencyHz = 0;
    TSCInfo.TicksPerMillisecond = 0;
    TSCInfo.Calibrated = FALSE;

    if (!CPUInfo.TSC)
    {
        return;
    }

    if (TSCDetectFrequencyWithCPUID())
    {
        return;
    }

    (VOID)TSCCalibrateWithHPET(TSC_CALIBRATION_MS);
}

VOID TSCDeadlineSet(UINT64 DeadlineTSC)
{
    WRMSR(IA32_TSC_DEADLINE_MSR, DeadlineTSC);
}

VOID TSCDeadlineClear(VOID)
{
    WRMSR(IA32_TSC_DEADLINE_MSR, 0);
}
