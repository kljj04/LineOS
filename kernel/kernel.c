// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

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

    VirtIOInputInit();

    PRRInit();

    STI();
}

VOID FlushScreen(VOID)
{
    while (TRUE)
    {
        VirtIOGPUFlush();
        HLTONCE();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);
    PRRCreateTask(FlushScreen);
    PRRCreateTask(InputMouseHandler);
    PRRCreateTask(InputKeyboardHandler);
    StartSchedule();
    HLT();
}
