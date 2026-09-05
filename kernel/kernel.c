// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <multicore/smp.h>
#include <render/truetype/truetype_engine.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <input/virtio_input.h>
#include <scheduler/prr.h>
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

    IDTInit();
    IDTLoad();

    LAPICInit();

    HPETInit(BootInfo);

    TSCCalibrate();
    TSCDeadlineInit(0x40);

    SMPInit(BootInfo);

    VirtIOInputInit();

    PRRInit();

    STI();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    KPrint(L"CPU Count: %u", 100, 100, 0xFFFFFF, 48, JETBRAINS_MONO, SMPGetCPUCount());

    for (UINT32 CPUID = 0; CPUID < SMPGetCPUCount(); CPUID++)
    {
        CPU_INFO *CPU;

        CPU = SMPGetCPU(CPUID);

        KPrint(L"CPU %u APIC %u BSP %u Online %u", 100, 160 + CPUID * 40, 0xFFFFFF, 32, JETBRAINS_MONO, CPU->CPUID, CPU->APICID, CPU->BSP, CPU->Online);
    }

    VirtIOGPUFlush();
    HLT();
}

VOID APMain(UINT32 CPUID)
{
    CPU_INFO *CPU;

    CLI();

    CPU = SMPGetCPU(CPUID);

    if (CPU != NULL)
    {
        CPU->Online = TRUE;
    }

    while (TRUE)
    {
        HLT();
    }
}
