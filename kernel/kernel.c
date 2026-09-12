// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <multicore/smp.h>
#include <render/truetype/truetype_engine.h>
#include <render/gpu/virtio_gpu.h>
#include <input/virtio_input.h>
#include <scheduler/lbpwrr.h>
#include <scheduler/task.h>
#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <lineos/bootinfo.h>
#include <interrupt/apic.h>
#include <interrupt/idt.h>
#include <memory/memory.h>
#include <timer/hpet.h>
#include <timer/tsc.h>
#include <pci/pci.h>

#define SMP_TEST_TASK_COUNT 1

STATIC VOLATILE BOOLEAN SchedulerReady = FALSE;

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
}

VOID TestTask(VOID)
{
    DebugWrite("TESTTASK ENTRY\n");

    FillScreen(0x0000FF);
    VirtIOGPUFlush();

    while (TRUE)
    {
        for (UINT32 i = 0; i < 100000; i++)
        {
            PAUSE();
        }

        LBPWRRYield();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK *task;

    InitKernel(BootInfo);

    for (UINT32 i = 0; i < SMP_TEST_TASK_COUNT; i++)
    {
        task = TaskCreate(TestTask);

        if (task == NULL)
        {
            continue;
        }

        LBPWRRAddTask(task);
    }

    CompilerBarrier();

    SchedulerReady = TRUE;

    CompilerBarrier();

    LBPWRRStart();

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

    HLT();
}
