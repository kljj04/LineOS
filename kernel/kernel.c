// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <multicore/smp.h>
#include <render/truetype/truetype_engine.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <input/virtio_input.h>
#include <scheduler/lbpwrr.h>
#include <arch/x86_64/cpu.h>
#include <lineos/bootinfo.h>
#include <interrupt/apic.h>
#include <interrupt/idt.h>
#include <memory/memory.h>
#include <debug/panic.h>
#include <timer/hpet.h>
#include <timer/tsc.h>
#include <acpi/acpi.h>
#include <pci/pci.h>

#include <input/input.h>

#define CPU_USAGE_REFRESH_MS 500

STATIC UINTN CPUUsageSamples[SMP_MAX_CPUS];

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();

    KMemoryInit(BootInfo);
    KHeapInit();

    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);

    GDTInitCurrentCPU();

    IDTInit();
    IDTLoad();

    LAPICInit();

    HPETInit(BootInfo);

    TSCCalibrate();
    TSCDeadlineInit(0x40);

    SMPInit(BootInfo);

    VirtIOInputInit();

    LBPWRRInit();

    STI();
}

VOID TestTask(VOID)
{
    HLTONCE();
}

VOID FlushScreen(VOID)
{
    if (!VirtIOGPUFlush())
    {
        VirtIOGPUFlush();
    }
}

STATIC VOID WaitCPUUsageRefresh(VOID)
{
    UINT64 frequency;
    UINT64 ticks;
    UINT64 deadline;

    frequency = TSCGetFrequency();

    if (frequency == 0)
    {
        for (UINT32 count = 0; count < 1000; count++)
        {
            Yield();
        }

        return;
    }

    ticks = (UINT64) (((UINT128) frequency * CPU_USAGE_REFRESH_MS) / 1000ULL);
    deadline = RDTSC() + ticks;

    while ((INT64) (RDTSC() - deadline) < 0)
    {
        Yield();
        PAUSE();
    }
}

STATIC VOID DrawCPUUsageMonitor(VOID)
{
    UINT32 CPUCount;
    UINT64 TotalUsage;
    UINTN AverageUsage;
    UINT32 PanelHeight;

    CPUCount = SMPGetCPUCount();
    TotalUsage = 0;

    for (UINT32 CPUID = 0; CPUID < CPUCount && CPUID < SMP_MAX_CPUS; CPUID++)
    {
        CPUUsageSamples[CPUID] = LBPWRRGetCPUUsage(CPUID);
        TotalUsage += CPUUsageSamples[CPUID];
    }

    AverageUsage = 0;

    if (CPUCount != 0)
    {
        AverageUsage = (UINTN) (TotalUsage / CPUCount);
    }

    PanelHeight = 130 + CPUCount * 40;

    FillRect(80, 60, 1320, PanelHeight, 0x000000);
    KPrint(L"CPU Count: %u   Total Usage: %u%%", 100, 100, 0xFFFFFF, 48, JETBRAINS_MONO, CPUCount, (UINT32) AverageUsage);

    for (UINT32 CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        CPU_INFO *CPU;
        UINT64 AssignedTaskCount;

        CPU = SMPGetCPU(CPUID);

        if (CPU == NULL)
        {
            continue;
        }

        AssignedTaskCount = LBPWRRGetCPUAssignedTaskCount(CPUID);

        KPrint(L"CPU %u APIC %u BSP %u Online %u Tasks %llu Usage %u%%", 100, 160 + CPUID * 40, 0xFFFFFF, 32, JETBRAINS_MONO, CPU->CPUID, CPU->APICID, CPU->BSP, CPU->Online, AssignedTaskCount, (UINT32) CPUUsageSamples[CPUID]);
    }

    VirtIOGPUFlush();
}

VOID CPUUsageMonitorTask(VOID)
{
    WaitCPUUsageRefresh();

    while (TRUE)
    {
        DrawCPUUsageMonitor();
        WaitCPUUsageRefresh();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    LBPWRRCreateTask(CPUUsageMonitorTask);

    for (UINT32 i = 0; i < 1024; i++)
    {
        LBPWRRCreateTask(TestTask);
    }

    StartSchedule();

    while (TRUE)
    {
        HLTONCE();
    }
}

VOID APMain(UINT32 CPUID)
{
    CPU_INFO *CPU;

    CLI();

    CPU = SMPGetCPU(CPUID);

    GDTInitCurrentCPU();
    IDTLoad();

    if (!LAPICInitCurrentCPU())
    {
        HLT();
    }

    if (CPU != NULL)
    {
        CPU->Online = TRUE;
        CompilerBarrier();
    }

    APJoinSchedule();
}
