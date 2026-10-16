
// kernel/kernel.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/TrueType/print.h>
#include <multicore/SMP.h>
#include <render/TrueType/TrueTypeEngine.h>
#include <scheduler/LBPWRRRunQueue.h>
#include <render/GPU/VirtIOGPU.h>
#include <input/VirtIOInput.h>
#include <scheduler/LBPWRR.h>
#include <scheduler/task.h>
#include <arch/AMD64/CPU.h>
#include <LineOS/BootInfo.h>
#include <interrupt/APIC.h>
#include <interrupt/IDT.h>
#include <memory/memory.h>
#include <timer/HPET.h>
#include <timer/TSC.h>
#include <PCI/PCI.h>

#define TEST_WORKER_COUNT 4
#define TEST_CHURN_COUNT  32
#define TEST_WAIT_MS      2000

#define TEST_FAIL_PREEMPT       (1U << 0)
#define TEST_FAIL_SLEEP         (1U << 1)
#define TEST_FAIL_YIELD         (1U << 2)
#define TEST_FAIL_CHURN_CREATE  (1U << 3)
#define TEST_FAIL_CHURN_RUN     (1U << 4)
#define TEST_FAIL_CHURN_DESTROY (1U << 5)
#define TEST_FAIL_PRIORITY      (1U << 6)
#define TEST_FAIL_KILL          (1U << 7)
#define TEST_FAIL_TERMINATE     (1U << 8)

STATIC TASK *TestWorkers[TEST_WORKER_COUNT];
STATIC TASK *TestSleepTask;
STATIC TASK *TestYieldTask;
STATIC TASK *TestKillTask;
STATIC TASK *TestTerminateTask;
STATIC TASK *TestControllerTask;
STATIC TASK *TestMonitorTask;
STATIC TASK *TestChurnTasks[TEST_CHURN_COUNT];

STATIC VOLATILE UINT64 TestCounters[TEST_WORKER_COUNT];
STATIC VOLATILE UINT64 TestSleepCount;
STATIC VOLATILE UINT64 TestYieldCount;
STATIC VOLATILE UINT64 TestKillCount;
STATIC VOLATILE UINT64 TestTerminateCount;
STATIC VOLATILE UINT64 TestCoreCounters[SMP_MAX_CPUS];
STATIC VOLATILE UINT64 TestChurnEntered;

STATIC VOLATILE UINT32  TestPhase;
STATIC VOLATILE UINT32  TestErrors;
STATIC VOLATILE UINT32  TestMigrationSuccess;
STATIC VOLATILE UINT32  TestMigrationFailure;
STATIC VOLATILE UINT32  TestFailureMask;
STATIC VOLATILE UINT32  TestChurnCreated;
STATIC VOLATILE UINT32  TestChurnCompleted;
STATIC VOLATILE BOOLEAN TestCompleted;

STATIC VOID TestSetPhase(UINT32 Phase, CONST char *Name)
{
    TestPhase = Phase;

    (VOID) Name;
}

STATIC VOID TestFail(UINT32 Failure, UINT8 Marker)
{
    TestErrors++;
    TestFailureMask |= Failure;
    (VOID) Marker;
}

STATIC VOID TestCountWorker(UINT32 WorkerIndex)
{
    UINT32 CPUID;

    CPUID = SMPGetCurrentCPUID();

    TestCounters[WorkerIndex]++;

    if (CPUID < SMP_MAX_CPUS)
    {
        TestCoreCounters[CPUID]++;
    }
}

STATIC VOID TestMonitor(VOID)
{
    for (;;)
    {
        FillRect(20, 20, 1400, 680, 0x101010);

        KPrint(L"LineOS Scheduler Stress Test", 40, 76, 0xFFFFFF, 38, PRETENDARD);

        KPrint(L"Phase: %u   Errors: %u   Complete: %u", 40, 130, 0xB0C8FF, 26, JETBRAINS_MONO, TestPhase, TestErrors, TestCompleted ? 1 : 0);

        KPrint(L"Worker 0: %llu   Worker 1: %llu", 40, 184, 0xD0D0D0, 24, JETBRAINS_MONO, TestCounters[0], TestCounters[1]);

        KPrint(L"Worker 2: %llu   Worker 3: %llu", 40, 230, 0xD0D0D0, 24, JETBRAINS_MONO, TestCounters[2], TestCounters[3]);

        KPrint(L"Sleep: %llu   Yield: %llu", 40, 284, 0xD0D0D0, 24, JETBRAINS_MONO, TestSleepCount, TestYieldCount);

        KPrint(L"Kill: %llu   Terminate: %llu", 40, 330, 0xD0D0D0, 24, JETBRAINS_MONO, TestKillCount, TestTerminateCount);

        KPrint(L"Migration OK: %u   Failed: %u", 40, 384, 0xD0D0D0, 24, JETBRAINS_MONO, TestMigrationSuccess, TestMigrationFailure);

        KPrint(L"Churn: %u / %u   Entered: %llu", 40, 430, 0xD0D0D0, 24, JETBRAINS_MONO, TestChurnCompleted, TestChurnCreated, TestChurnEntered);

        KPrint(L"CPU counters: %llu %llu %llu %llu", 40, 484, 0xD0D0D0, 23, JETBRAINS_MONO, TestCoreCounters[0], TestCoreCounters[1], TestCoreCounters[2], TestCoreCounters[3]);

        KPrint(L"Failure mask: 0x%X", 40, 538, 0xFFB0B0, 25, JETBRAINS_MONO, TestFailureMask);

        KPrint(TestCompleted ? (TestErrors == 0 ? L"RESULT: PASS" : L"RESULT: FAIL") : L"RESULT: RUNNING", 40, 620, TestCompleted && TestErrors == 0 ? 0x80FF80 : TestCompleted ? 0xFF8080 : 0xFFD080, 36, PRETENDARD);

        VirtIOGPUFlush();
        TaskSleepMls(250);
    }
}

STATIC VOID TestWorker0(VOID)
{
    for (;;)
    {
        TestCountWorker(0);
    }
}

STATIC VOID TestWorker1(VOID)
{
    for (;;)
    {
        TestCountWorker(1);
    }
}

STATIC VOID TestWorker2(VOID)
{
    for (;;)
    {
        TestCountWorker(2);
    }
}

STATIC VOID TestWorker3(VOID)
{
    for (;;)
    {
        TestCountWorker(3);
    }
}

STATIC VOID TestChurnWorker(VOID)
{
    TestChurnEntered++;
}

STATIC BOOLEAN TestWaitForChurnCompletion(VOID)
{
    UINT32 Index;
    UINT32 Retry;

    for (Retry = 0; Retry < 100; Retry++)
    {
        TestChurnCompleted = 0;

        for (Index = 0; Index < TEST_CHURN_COUNT; Index++)
        {
            if (TestChurnTasks[Index] == NULL)
            {
                continue;
            }

            if (TestChurnTasks[Index]->State == TASK_TERMINATED || TestChurnTasks[Index]->State == TASK_KILLED)
            {
                TestChurnCompleted++;
            }
        }

        if (TestChurnCompleted == TestChurnCreated)
        {
            return TRUE;
        }

        TaskSleepMls(100);
    }

    return FALSE;
}

STATIC BOOLEAN TestDestroyChurnTasks(VOID)
{
    UINT32 Index;
    UINT32 Retry;
    UINT32 Remaining;

    for (Retry = 0; Retry < 100; Retry++)
    {
        Remaining = 0;

        for (Index = 0; Index < TEST_CHURN_COUNT; Index++)
        {
            if (TestChurnTasks[Index] == NULL)
            {
                continue;
            }

            if (TaskDestroy(TestChurnTasks[Index]))
            {
                TestChurnTasks[Index] = NULL;
                continue;
            }

            Remaining++;
        }

        if (Remaining == 0)
        {
            return TRUE;
        }

        TaskSleepMls(10);
    }

    return FALSE;
}

STATIC VOID TestSleeper(VOID)
{
    for (;;)
    {
        TestSleepCount++;
        TaskSleepMls(100);
    }
}

STATIC VOID TestYielder(VOID)
{
    for (;;)
    {
        TestYieldCount++;
        LBPWRRYield();
    }
}

STATIC VOID TestKillWorker(VOID)
{
    for (;;)
    {
        TestKillCount++;
    }
}

STATIC VOID TestTerminateWorker(VOID)
{
    for (;;)
    {
        TestTerminateCount++;
        TaskSleepMls(10);
    }
}

STATIC VOID TestController(VOID)
{
    UINT32  Index;
    UINT32  CPUCount;
    UINT32  TargetCPU;
    UINT64  Previous[TEST_WORKER_COUNT];
    UINT64  PreviousSleep;
    UINT64  PreviousYield;
    UINT64  PreviousKill;
    UINT64  PreviousTerminate;
    TASK   *Task;
    BOOLEAN Success;

    CPUCount = LBPWRRGetActiveCPUCount();

    TestSetPhase(1, "TIMER PREEMPTION");

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        Previous[Index] = TestCounters[Index];
    }

    TaskSleepMls(TEST_WAIT_MS);

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        Success = TestCounters[Index] != Previous[Index];

        if (!Success)
        {
            TestFail(TEST_FAIL_PREEMPT, 'P');
        }
    }

    TestSetPhase(2, "SLEEP AND WAKE");

    PreviousSleep = TestSleepCount;

    TaskSleepMls(TEST_WAIT_MS);

    Success = TestSleepCount > PreviousSleep;

    if (!Success)
    {
        TestFail(TEST_FAIL_SLEEP, 'S');
    }

    TestSetPhase(3, "VOLUNTARY YIELD");

    PreviousYield = TestYieldCount;

    TaskSleepMls(TEST_WAIT_MS);

    Success = TestYieldCount > PreviousYield;

    if (!Success)
    {
        TestFail(TEST_FAIL_YIELD, 'Y');
    }

    TestSetPhase(4, "TASK CHURN");

    for (Index = 0; Index < TEST_CHURN_COUNT; Index++)
    {
        Task = TaskCreate(TestChurnWorker);
        TestChurnTasks[Index] = Task;

        if (Task == NULL)
        {
            TestFail(TEST_FAIL_CHURN_CREATE, 'C');

            continue;
        }

        if (!LBPWRRAddTask(Task))
        {
            TestFail(TEST_FAIL_CHURN_CREATE, 'C');

            TaskTerminate(Task);
            TaskDestroy(Task);
            TestChurnTasks[Index] = NULL;

            continue;
        }

        TestChurnCreated++;
    }

    Success = TestWaitForChurnCompletion();

    if (!Success)
    {
        TestFail(TEST_FAIL_CHURN_RUN, 'R');
    }

    Success = TestDestroyChurnTasks();

    if (!Success)
    {
        TestFail(TEST_FAIL_CHURN_DESTROY, 'D');
    }

    TestSetPhase(5, "PRIORITY CHANGES");

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        Success = TaskSetPriority(TestWorkers[Index], (UINT8) (Index + 1));

        if (!Success)
        {
            TestFail(TEST_FAIL_PRIORITY, 'Q');
        }
    }

    TaskSleepMls(TEST_WAIT_MS);

    TestSetPhase(6, "REVERSE PRIORITIES");

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        Success = TaskSetPriority(TestWorkers[Index], (UINT8) (5 - Index));

        if (!Success)
        {
            TestFail(TEST_FAIL_PRIORITY, 'q');
        }
    }

    TaskSleepMls(TEST_WAIT_MS);

    TestSetPhase(7, "CPU MIGRATION");

    if (CPUCount > 1)
    {
        for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
        {
            Task = TestWorkers[Index];

            TargetCPU = (Task->CPUID + 1) % CPUCount;

            if (LBPWRRMoveTask(Task, TargetCPU))
            {
                TestMigrationSuccess++;
            }
            else
            {
                TestMigrationFailure++;
            }
        }
    }

    TaskSleepMls(TEST_WAIT_MS);

    TestSetPhase(8, "TASK KILL");

    TaskKill(TestKillTask);

    TaskSleepMls(500);

    PreviousKill = TestKillCount;

    TaskSleepMls(500);

    Success = TestKillCount == PreviousKill;

    if (!Success)
    {
        TestFail(TEST_FAIL_KILL, 'K');
    }

    TestSetPhase(9, "TASK TERMINATION");

    TaskTerminate(TestTerminateTask);

    TaskSleepMls(500);

    PreviousTerminate = TestTerminateCount;

    TaskSleepMls(500);

    Success = TestTerminateCount == PreviousTerminate;

    if (!Success)
    {
        TestFail(TEST_FAIL_TERMINATE, 'T');
    }

    TestSetPhase(10, "STOP WORKERS");

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        Task = TestWorkers[Index];

        TaskTerminate(Task);
    }

    TaskSleepMls(500);

    TestSetPhase(11, "STOP AUXILIARY TASKS");

    TaskKill(TestSleepTask);

    TaskKill(TestYieldTask);

    TaskSleepMls(500);

    TestSetPhase(12, "IDLE TRANSITION");

    TestCompleted = TRUE;

    TaskTerminate(LBPWRRGetCurrentTask());

    LBPWRRYield();

    for (;;)
    {
        TestFail(TEST_FAIL_TERMINATE, 'X');

        LBPWRRYield();
    }
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

    if (!TSCCalibrate() || !TSCDeadlineInit(0x40))
    {
        HLT();
    }

    if (!SMPInit(BootInfo))
    {
        HLT();
    }

    if (!LBPWRRInit())
    {
        HLT();
    }

    VirtIOInputInit();
}

STATIC TASK *TestCreateTask(CONST char *Name, VOID (*Entry)(VOID), UINT8 Priority)
{
    TASK *Task;

    (VOID) Name;

    Task = TaskCreate(Entry);

    if (Task == NULL)
    {
        HLT();
    }

    if (!TaskSetPriority(Task, Priority))
    {
        HLT();
    }

    if (!LBPWRRAddTask(Task))
    {
        HLT();
    }

    return Task;
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    KPrint(L"Initializing scheduler stress test...", 40, 740, 0xFFFFFF, 26, PRETENDARD);

    VirtIOGPUFlush();

    TestWorkers[0] = TestCreateTask("WORKER 0", TestWorker0, 6);

    TestWorkers[1] = TestCreateTask("WORKER 1", TestWorker1, 5);

    TestWorkers[2] = TestCreateTask("WORKER 2", TestWorker2, 3);

    TestWorkers[3] = TestCreateTask("WORKER 3", TestWorker3, 1);

    TestSleepTask = TestCreateTask("SLEEPER", TestSleeper, 3);

    TestYieldTask = TestCreateTask("YIELDER", TestYielder, 3);

    TestKillTask = TestCreateTask("KILL WORKER", TestKillWorker, 3);

    TestTerminateTask = TestCreateTask("TERMINATE WORKER", TestTerminateWorker, 3);

    TestControllerTask = TestCreateTask("CONTROLLER", TestController, 3);

    TestMonitorTask = TestCreateTask("MONITOR", TestMonitor, 6);

    KPrint(L"BEFORE START", 40, 790, 0x80FF80, 28, PRETENDARD);

    VirtIOGPUFlush();

    LBPWRRStart();

    KPrint(L"AFTER START", 40, 840, 0xFF8080, 28, PRETENDARD);

    VirtIOGPUFlush();

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

    if (!TSCDeadlineInit(0x40))
    {
        HLT();
    }

    if (CPU != NULL)
    {
        CPU->Online = TRUE;
        CompilerBarrier();
    }

    CompilerBarrier();

    for (;;)
    {
        LBPWRRJoin();
        STIHLT();
    }
}
