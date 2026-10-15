// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/TrueType/print.h>
#include <multicore/SMP.h>
#include <render/TrueType/TrueTypeEngine.h>
#include <render/GPU/VirtIOGPU.h>
#include <input/VirtIOInput.h>
#include <scheduler/LBPWRR.h>
#include <scheduler/task.h>
#include <arch/AMD64/CPU.h>
#include <debug/debug.h>
#include <LineOS/BootInfo.h>
#include <interrupt/APIC.h>
#include <interrupt/IDT.h>
#include <memory/memory.h>
#include <timer/HPET.h>
#include <timer/TSC.h>
#include <PCI/PCI.h>

VOID InitKernel(LINEOS_BOOT_INFO *BootInfo)
{
    VIRTIO_GPU_INFO *GPU;

    CLI();
    CPUEnableSSE();

    KMemoryInit(BootInfo);
    KHeapInit();

    TrueTypeInit();
    PCIInit(BootInfo);

    VirtIOGPUInit();

    GPU = VirtIOGPUGetInfo();

    VirtIOGPUCreateFrameBuffer(GPU->DisplayInfo.Displays[0].Rect.Width, GPU->DisplayInfo.Displays[0].Rect.Height);
    FillScreen(0x101010);
    VirtIOGPUFlush();

    GDTInitCurrentCPU();

    IDTInit();
    IDTLoad();

    LAPICInit();

    HPETInit(BootInfo);

    TSCCalibrate();
    TSCDeadlineInit(0x40);

    SMPInit(BootInfo);

    LBPWRRInit();

    VirtIOInputInit();
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    HLT();
}

VOID APMain(UINT32 CPUID)
{
    CPU_INFO *CPU;

    CLI();

    CPU = SMPGetCPU(CPUID);

    GDTInitCurrentCPU();
    CPUEnableSSE();
    IDTLoad();

    if (!LAPICInitCurrentCPU())
    {
        HLT();
    }

    TSCDeadlineInit(0x40);

    if (CPU != NULL)
    {
        CPU->Online = TRUE;
        CompilerBarrier();
    }

    CompilerBarrier();
    HLT();
}
