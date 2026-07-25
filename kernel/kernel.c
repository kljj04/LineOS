// kernel.c
// LineOS Project
// Timer runtime probe kernel

#include <lineos/bootinfo.h>
#include <acpi/acpi.h>
#include <arch/x86_64/cpu.h>
#include <memory/memory.h>
#include <render/framebuffer.h>
#include <render/print.h>
#include <time/timer.h>
#include <time/tsc.h>

#define TIMER_BG     0x101018
#define TIMER_PANEL  0x1E1E2E
#define TIMER_BORDER 0x89B4FA

STATIC CONST CHAR16* YesNo(BOOLEAN value)
{
    return value ? L"YES" : L"NO";
}

STATIC UINT32 BoolColor(BOOLEAN value)
{
    return value ? LIGHT_GREEN : LIGHT_RED;
}

STATIC VOID PrintBool(CONST CHAR16* label, BOOLEAN value, UINT32 y)
{
    KPrint(label, 96, y, LIGHT_GRAY);
    KPrint(YesNo(value), 520, y, BoolColor(value));
}

STATIC VOID DrawStaticTimerInfo(VOID)
{
    FillScreen(TIMER_BG);
    FillRect(56, 48, 980, 700, TIMER_PANEL);
    DrawRect(56, 48, 980, 700, TIMER_BORDER);

    KPrint(L"LineOS Timer Test", 96, 104, YELLOW);
    KPrint(L"Main timer should prefer TSC Deadline when available.", 96, 144, LIGHT_CYAN);

    KPrint(L"Main source", 96, 220, LIGHT_GRAY);
    KPrint(TimerSourceName(TimerInfo.MainSource), 520, 220, LIGHT_GREEN);

    PrintBool(L"CPU TSC", CPUInfo.TSC, 280);
    PrintBool(L"CPU Invariant TSC", CPUInfo.InvariantTSC, 320);
    PrintBool(L"CPU APIC", CPUInfo.APIC, 360);
    PrintBool(L"CPU TSC Deadline", CPUInfo.TSCDeadline, 400);
    PrintBool(L"Timer TSC Deadline available", TimerInfo.TSCDeadlineAvailable, 460);
    PrintBool(L"LAPIC available", TimerInfo.LAPICTimerAvailable, 500);
    PrintBool(L"LAPIC calibrated", TimerInfo.LAPICTimerCalibrated, 540);
    PrintBool(L"HPET available", TimerInfo.HPETAvailable, 580);

    KPrint(L"TSC Hz", 96, 640, LIGHT_GRAY);
    KPrint(L"%llu", 520, 640, WHITE, TimerInfo.TSCFrequencyHz);

    KPrint(L"TSC ticks/ms", 96, 680, LIGHT_GRAY);
    KPrint(L"%llu", 520, 680, WHITE, TimerInfo.TSCTicksPerMillisecond);
}

STATIC VOID DrawRuntimeLabels(VOID)
{
    KPrint(L"Current TSC", 112, 820, LIGHT_GRAY);
    KPrint(L"Delta TSC", 112, 870, LIGHT_GRAY);
    KPrint(L"Frames", 112, 920, LIGHT_GRAY);
    KPrint(L"Seconds", 112, 970, LIGHT_GRAY);
}

STATIC VOID DrawRuntimeStatus(UINT64 CurrentTSC, UINT64 DeltaTSC, UINT64 frames, UINT64 seconds)
{
    FillRect(500, 782, 520, 46, TIMER_BG);
    KPrint(L"%llu", 520, 820, LIGHT_GREEN, CurrentTSC);

    FillRect(500, 832, 520, 46, TIMER_BG);
    KPrint(L"%llu", 520, 870, LIGHT_CYAN, DeltaTSC);

    FillRect(500, 882, 520, 46, TIMER_BG);
    KPrint(L"%llu", 520, 920, WHITE, frames);

    FillRect(500, 932, 520, 46, TIMER_BG);
    KPrint(L"%llu", 520, 970, YELLOW, seconds);
}

VOID __attribute__((ms_abi)) KMain(LINEOS_BOOT_INFO* BootInfo)
{
    UINT64 StartTSC;
    UINT64 CurrentTSC;
    UINT64 frames = 0;
    UINT64 seconds = 0;

    FrameBufferInit(BootInfo);
    CPUDetect();
    PMMInit(BootInfo);
    ACPIInit();
    TimerInit();

    StartTSC = TSCRead();

    DrawStaticTimerInfo();
    DrawRuntimeLabels();
    DrawRuntimeStatus(StartTSC, 0, frames, seconds);

    while (1)
    {
        CurrentTSC = TSCRead();
        frames++;

        if (TimerInfo.TSCFrequencyHz != 0)
        {
            seconds = (CurrentTSC - StartTSC) / TimerInfo.TSCFrequencyHz;
        }

        DrawRuntimeStatus(CurrentTSC, CurrentTSC - StartTSC, frames, seconds);
        __asm__ volatile("pause");
    }
}
