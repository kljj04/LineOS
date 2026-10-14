// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "debug/panic.h"

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

#define SMP_TEST_TASK_COUNT 1
#define TEST_BUSY_WAIT      50000000ULL
#define TASK_BAND_HEIGHT    118
#define TEST_TASK_COUNT     10

STATIC VOLATILE BOOLEAN SchedulerReady = FALSE;
STATIC VOLATILE BOOLEAN DelayedTaskVAdded = FALSE;
STATIC VOLATILE BOOLEAN DelayedTaskWAdded = FALSE;
STATIC TASK            *DelayedTaskV = NULL;
STATIC TASK            *DelayedTaskW = NULL;
STATIC VOLATILE UINT64  TaskCounters[TEST_TASK_COUNT];
STATIC VOLATILE UINT32  TaskCPUs[TEST_TASK_COUNT];
STATIC VOLATILE BOOLEAN TaskEnabled[TEST_TASK_COUNT];
STATIC VOLATILE UINT64  ChaosCounter = 0;
STATIC UINT32           ChaosSeed = 0xC0FFEEU;

STATIC CONST CHAR16 *TaskLabels[TEST_TASK_COUNT] = {
    L"A", L"B", L"C", L"D", L"E", L"F", L"G", L"H", L"V", L"W",
};

STATIC CONST UINT32 TaskColors[TEST_TASK_COUNT] = {
    0x005BFF,
    0xFF1F1F,
    0x00E040,
    0xFFB000,
    0xA020F0,
    0x00C8D7,
    0xFF5FA2,
    0x708090,
    0x191970,
    0x303030,
};

STATIC CONST UINT32 TaskForegrounds[TEST_TASK_COUNT] = {
    0xFFFFFF,
    0xFFFFFF,
    0x000000,
    0x000000,
    0xFFFFFF,
    0x000000,
    0x000000,
    0xFFFFFF,
    0xFFFFFF,
    0xFFFFFF,
};

STATIC CONST UINT8 TaskPriorities[TEST_TASK_COUNT] = {
    0, 1, 2, 3, 4, 5, 2, 4, 0, 5,
};

STATIC VOID BusyWait(UINT64 Count)
{
    VOLATILE UINT64 Index;

    for (Index = 0; Index < Count; Index++)
    {
        ASM("pause");
    }
}

STATIC VOID DrawTaskBand(UINT32 Index, UINT32 Y, BOOLEAN Active)
{
    UINT32 Background;
    UINT32 Foreground;
    UINT32 CPUID;
    CONST CHAR16 *State;

    Active = Active && TaskEnabled[Index];
    Background = Active ? TaskColors[Index] : 0x202020;
    Foreground = Active ? TaskForegrounds[Index] : 0x707070;
    State = Active ? L"ON" : L"OFF";
    CPUID = TaskCPUs[Index];

    FillRect(0, Y, VirtIOGPUGetFrameBufferWidth(), TASK_BAND_HEIGHT - 4, Background);
    KPrint(
        L"%s  count=%llu  cpu=%u  prio=%u  %s",
        48,
        Y + 84,
        Foreground,
        58,
        PRETENDARD,
        TaskLabels[Index],
        TaskCounters[Index],
        CPUID,
        (UINT32) TaskPriorities[Index],
        State
    );
}

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

    VirtIOInputInit();

    LBPWRRInit();
}

STATIC VOID RunCounterTask(UINT32 Index, UINT64 Wait)
{
    while (TRUE)
    {
        TaskCPUs[Index] = SMPGetCurrentCPUID();

        if (TaskEnabled[Index])
        {
            TaskCounters[Index]++;
        }

        BusyWait(Wait);

        LBPWRRYield();
    }
}

VOID TestTaskA(VOID)
{
    RunCounterTask(0, TEST_BUSY_WAIT);
}

VOID TestTaskB(VOID)
{
    RunCounterTask(1, TEST_BUSY_WAIT / 2);
}

VOID TestTaskC(VOID)
{
    RunCounterTask(2, TEST_BUSY_WAIT / 3);
}

VOID TestTaskD(VOID)
{
    RunCounterTask(3, TEST_BUSY_WAIT / 4);
}

VOID TestTaskE(VOID)
{
    RunCounterTask(4, TEST_BUSY_WAIT / 5);
}

VOID TestTaskF(VOID)
{
    RunCounterTask(5, TEST_BUSY_WAIT / 6);
}

VOID TestTaskG(VOID)
{
    RunCounterTask(6, TEST_BUSY_WAIT / 7);
}

VOID TestTaskH(VOID)
{
    RunCounterTask(7, TEST_BUSY_WAIT / 8);
}

VOID TestTaskV(VOID)
{
    RunCounterTask(8, TEST_BUSY_WAIT / 2);
}

VOID TestTaskW(VOID)
{
    RunCounterTask(9, TEST_BUSY_WAIT / 9);
}

VOID TestTaskRender(VOID)
{
    UINT32 Index;

    while (TRUE)
    {
        for (Index = 0; Index < 8; Index++)
        {
            DrawTaskBand(Index, Index * TASK_BAND_HEIGHT, TRUE);
        }

        DrawTaskBand(8, 8 * TASK_BAND_HEIGHT, DelayedTaskVAdded);
        DrawTaskBand(9, 9 * TASK_BAND_HEIGHT, DelayedTaskWAdded);

        VirtIOGPUFlush();
        BusyWait(TEST_BUSY_WAIT / 2);
        LBPWRRYield();
    }
}

STATIC UINT32 ChaosRandom(VOID)
{
    ChaosSeed = ChaosSeed * 1664525U + 1013904223U;

    return ChaosSeed;
}

VOID TestTaskAddDelayed(VOID)
{
    TaskSleepMls(1000);

    if (!DelayedTaskVAdded && DelayedTaskV != NULL)
    {
        DelayedTaskVAdded = TRUE;
        TaskEnabled[8] = TRUE;
        LBPWRRAddTask(DelayedTaskV);
    }

    TaskSleepMls(800);

    if (!DelayedTaskWAdded && DelayedTaskW != NULL)
    {
        DelayedTaskWAdded = TRUE;
        TaskEnabled[9] = TRUE;
        LBPWRRAddTask(DelayedTaskW);
    }

    while (TRUE)
    {
        UINT32 Index;

        TaskSleepMls(350);

        Index = ChaosRandom() % TEST_TASK_COUNT;

        if ((Index == 8 && !DelayedTaskVAdded) || (Index == 9 && !DelayedTaskWAdded))
        {
            continue;
        }

        TaskEnabled[Index] = !TaskEnabled[Index];
        ChaosCounter++;

        LBPWRRYield();
    }
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK *TaskA;
    TASK *TaskB;
    TASK *TaskC;
    TASK *TaskD;
    TASK *TaskE;
    TASK *TaskF;
    TASK *TaskG;
    TASK *TaskH;
    TASK *TaskV;
    TASK *TaskW;
    TASK *TaskAddDelayed;
    TASK *TaskRender;

    InitKernel(BootInfo);

    TaskA = TaskCreate(TestTaskA);
    TaskB = TaskCreate(TestTaskB);
    TaskC = TaskCreate(TestTaskC);
    TaskD = TaskCreate(TestTaskD);
    TaskE = TaskCreate(TestTaskE);
    TaskF = TaskCreate(TestTaskF);
    TaskG = TaskCreate(TestTaskG);
    TaskH = TaskCreate(TestTaskH);
    TaskV = TaskCreate(TestTaskV);
    TaskW = TaskCreate(TestTaskW);
    TaskAddDelayed = TaskCreate(TestTaskAddDelayed);
    TaskRender = TaskCreate(TestTaskRender);

    if (TaskA == NULL || TaskB == NULL || TaskC == NULL || TaskD == NULL || TaskE == NULL || TaskF == NULL || TaskG == NULL || TaskH == NULL || TaskV == NULL || TaskW == NULL || TaskAddDelayed == NULL || TaskRender == NULL)
    {
        while (TRUE)
        {
            HLTONCE();
        }
    }

    TaskSetPriority(TaskA, TaskPriorities[0]);
    TaskSetPriority(TaskB, TaskPriorities[1]);
    TaskSetPriority(TaskC, TaskPriorities[2]);
    TaskSetPriority(TaskD, TaskPriorities[3]);
    TaskSetPriority(TaskE, TaskPriorities[4]);
    TaskSetPriority(TaskF, TaskPriorities[5]);
    TaskSetPriority(TaskG, TaskPriorities[6]);
    TaskSetPriority(TaskH, TaskPriorities[7]);
    TaskSetPriority(TaskV, TaskPriorities[8]);
    TaskSetPriority(TaskW, TaskPriorities[9]);
    TaskSetPriority(TaskAddDelayed, 0);
    TaskSetPriority(TaskRender, 0);

    DelayedTaskV = TaskV;
    DelayedTaskW = TaskW;

    for (UINT32 Index = 0; Index < TEST_TASK_COUNT; Index++)
    {
        TaskCPUs[Index] = UINT32_MAX;
        TaskEnabled[Index] = Index < 8;
    }

    LBPWRRAddTask(TaskA);
    LBPWRRAddTask(TaskE);
    LBPWRRAddTask(TaskB);
    LBPWRRAddTask(TaskF);
    LBPWRRAddTask(TaskC);
    LBPWRRAddTask(TaskG);
    LBPWRRAddTask(TaskD);
    LBPWRRAddTask(TaskH);
    LBPWRRAddTask(TaskAddDelayed);
    LBPWRRAddTask(TaskRender);
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
    CPUEnableSSE();
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

    while (!SchedulerReady)
    {
        PAUSE();
    }

    CompilerBarrier();

    while (TRUE)
    {
        LBPWRRJoin();
        PAUSE();
    }
}
