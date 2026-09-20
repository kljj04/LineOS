// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/truetype/print.h>

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
#define TEST_BUSY_WAIT      50000000ULL

STATIC VOLATILE BOOLEAN SchedulerReady = FALSE;

STATIC VOID BusyWait(UINT64 Count)
{
    VOLATILE UINT64 Index;

    for (Index = 0; Index < Count; Index++)
    {
        ASM("pause");
    }
}

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

    VirtIOGPUCreateFrameBuffer(
        GPU->DisplayInfo.Displays[0].Rect.Width,
        GPU->DisplayInfo.Displays[0].Rect.Height
    );

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
    while (TRUE)
    {
        FillScreen(0x0000FF);
        KPrint(L"A", 100, 200, 0xFFFFFF, 300, PRETENDARD);
        VirtIOGPUFlush();

        BusyWait(TEST_BUSY_WAIT);

        LBPWRRYield();
    }
}

VOID TestTask2(VOID)
{
    while (TRUE)
    {
        FillScreen(0xFF0000);
        KPrint(L"B", 100, 200, 0xFFFFFF, 300, PRETENDARD);
        VirtIOGPUFlush();

        BusyWait(TEST_BUSY_WAIT);

        LBPWRRYield();
    }
}

VOID TestTask3(VOID)
{
    while (TRUE)
    {
        FillScreen(0x00FF00);
        KPrint(L"C", 100, 200, 0xFFFFFF, 300, PRETENDARD);
        VirtIOGPUFlush();

        BusyWait(TEST_BUSY_WAIT);

        LBPWRRYield();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK *Task1;
    TASK *Task2;
    TASK *Task3;

    InitKernel(BootInfo);

    Task1 = TaskCreate(TestTask);
    Task2 = TaskCreate(TestTask2);
    Task3 = TaskCreate(TestTask3);

    if (Task1 == NULL || Task2 == NULL || Task3 == NULL)
    {
        while (TRUE)
        {
            HLTONCE();
        }
    }

    TaskSetPriority(Task1, 0);
    TaskSetPriority(Task2, 2);
    TaskSetPriority(Task3, 4);

    LBPWRRAddTask(Task1);
    LBPWRRAddTask(Task2);
    LBPWRRAddTask(Task3);

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