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
#include <debug/debug.h>
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

STATIC VOLATILE UINT32 TestPhase;
STATIC VOLATILE UINT32 TestErrors;
STATIC VOLATILE UINT32 TestMigrationSuccess;
STATIC VOLATILE UINT32 TestMigrationFailure;
STATIC VOLATILE UINT32 TestFailureMask;
STATIC VOLATILE UINT32 TestChurnCreated;
STATIC VOLATILE UINT32 TestChurnCompleted;
STATIC VOLATILE BOOLEAN TestCompleted;

STATIC VOID TestSetPhase(UINT32 Phase)
{
    TestPhase = Phase;
    OUTB(0xE9, '0' + (UINT8) Phase);
}

STATIC VOID TestFail(UINT32 Failure, UINT8 Marker)
{
    TestErrors++;
    TestFailureMask |= Failure;
    OUTB(0xE9, '!');
    OUTB(0xE9, Marker);
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
        FillRect(24, 24, 760, 330, 0x101010);
        KPrint(L"LineOS scheduler stress test", 40, 58, 0xFFFFFF, 24, PRETENDARD);
        KPrint(L"phase=%u  errors=%u  complete=%u", 40, 94, 0xB0C8FF, 16, JETBRAINS_MONO,
               TestPhase, TestErrors, TestCompleted ? 1 : 0);
        KPrint(L"workers=%llu %llu %llu %llu", 40, 124, 0xD0D0D0, 15, JETBRAINS_MONO,
               TestCounters[0], TestCounters[1], TestCounters[2], TestCounters[3]);
        KPrint(L"sleep=%llu  yield=%llu", 40, 154, 0xD0D0D0, 15, JETBRAINS_MONO,
               TestSleepCount, TestYieldCount);
        KPrint(L"kill=%llu  terminate=%llu", 40, 184, 0xD0D0D0, 15, JETBRAINS_MONO,
               TestKillCount, TestTerminateCount);
        KPrint(L"migration ok=%u  failed=%u", 40, 214, 0xD0D0D0, 15, JETBRAINS_MONO,
               TestMigrationSuccess, TestMigrationFailure);
        KPrint(L"churn=%u/%u  core=%llu %llu %llu %llu", 40, 244, 0xD0D0D0, 15, JETBRAINS_MONO,
               TestChurnCompleted, TestChurnCreated, TestCoreCounters[0], TestCoreCounters[1],
               TestCoreCounters[2], TestCoreCounters[3]);
        KPrint(L"failure-mask=%u", 40, 274, 0xFFB0B0, 15, JETBRAINS_MONO, TestFailureMask);
        KPrint(TestCompleted ? (TestErrors == 0 ? L"RESULT: PASS" : L"RESULT: FAIL") : L"RESULT: RUNNING",
               40, 310, TestCompleted && TestErrors == 0 ? 0x80FF80 : TestCompleted ? 0xFF8080 : 0xFFD080,
               20, PRETENDARD);
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

            if (TestChurnTasks[Index]->State == TASK_TERMINATED ||
                TestChurnTasks[Index]->State == TASK_KILLED)
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
    UINT32 Index;
    UINT32 CPUCount;
    UINT32 TargetCPU;
    UINT64 Previous[TEST_WORKER_COUNT];
    UINT64 PreviousSleep;
    UINT64 PreviousYield;
    UINT64 PreviousKill;
    UINT64 PreviousTerminate;

    CPUCount = LBPWRRGetActiveCPUCount();

    // Phase 1: Timer preemption.
    TestSetPhase(1);

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        Previous[Index] = TestCounters[Index];
    }

    TaskSleepMls(TEST_WAIT_MS);

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        if (TestCounters[Index] == Previous[Index])
        {
            TestFail(TEST_FAIL_PREEMPT, 'P');
        }
    }

    // Phase 2: Sleep/Wake.
    TestSetPhase(2);
    PreviousSleep = TestSleepCount;

    TaskSleepMls(TEST_WAIT_MS);

    if (TestSleepCount <= PreviousSleep)
    {
        TestFail(TEST_FAIL_SLEEP, 'S');
    }

    // Phase 3: Yield.
    TestSetPhase(3);
    PreviousYield = TestYieldCount;

    TaskSleepMls(TEST_WAIT_MS);

    if (TestYieldCount <= PreviousYield)
    {
        TestFail(TEST_FAIL_YIELD, 'Y');
    }

    // Phase 4: Repeated create, exit, remove and destroy.
    TestSetPhase(4);

    for (Index = 0; Index < TEST_CHURN_COUNT; Index++)
    {
        TestChurnTasks[Index] = TaskCreate(TestChurnWorker);

        if (TestChurnTasks[Index] == NULL || !LBPWRRAddTask(TestChurnTasks[Index]))
        {
            TestFail(TEST_FAIL_CHURN_CREATE, 'C');

            if (TestChurnTasks[Index] != NULL)
            {
                TaskTerminate(TestChurnTasks[Index]);
                TaskDestroy(TestChurnTasks[Index]);
                TestChurnTasks[Index] = NULL;
            }
        }
        else
        {
            TestChurnCreated++;
        }
    }

    if (!TestWaitForChurnCompletion())
    {
        TestFail(TEST_FAIL_CHURN_RUN, 'R');
    }

    if (!TestDestroyChurnTasks())
    {
        TestFail(TEST_FAIL_CHURN_DESTROY, 'D');
    }

    // Phase 5: Priority changes.
    TestSetPhase(5);

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        if (!TaskSetPriority(
            TestWorkers[Index],
            (UINT8) Index))
        {
            TestFail(TEST_FAIL_PRIORITY, 'Q');
        }
    }

    TaskSleepMls(TEST_WAIT_MS);

    // Phase 6: Reverse priorities.
    TestSetPhase(6);

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        if (!TaskSetPriority(
            TestWorkers[Index],
            (UINT8) (5 - Index)))
        {
            TestFail(TEST_FAIL_PRIORITY, 'q');
        }
    }

    TaskSleepMls(TEST_WAIT_MS);

    // Phase 7: CPU migration.
    TestSetPhase(7);

    if (CPUCount > 1)
    {
        for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
        {
            TargetCPU =
                (TestWorkers[Index]->CPUID + 1)
                % CPUCount;

            if (LBPWRRMoveTask(
                TestWorkers[Index],
                TargetCPU))
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

    // Phase 8: Kill.
    TestSetPhase(8);
    TaskKill(TestKillTask);
    TaskSleepMls(500);

    PreviousKill = TestKillCount;
    TaskSleepMls(500);

    if (TestKillCount != PreviousKill)
    {
        TestFail(TEST_FAIL_KILL, 'K');
    }

    // Phase 9: Terminate.
    TestSetPhase(9);
    TaskTerminate(TestTerminateTask);
    TaskSleepMls(500);

    PreviousTerminate = TestTerminateCount;
    TaskSleepMls(500);

    if (TestTerminateCount != PreviousTerminate)
    {
        TestFail(TEST_FAIL_TERMINATE, 'T');
    }

    // Phase 10: Terminate workers.
    TestSetPhase(10);

    for (Index = 0; Index < TEST_WORKER_COUNT; Index++)
    {
        TaskTerminate(TestWorkers[Index]);
    }

    TaskSleepMls(500);

    // Phase 11: Stop remaining tasks.
    TestSetPhase(11);
    TaskKill(TestSleepTask);
    TaskKill(TestYieldTask);
    TaskSleepMls(500);

    // Phase 12: Idle-only transition.
    TestSetPhase(12);
    TestCompleted = TRUE;

    TaskTerminate(LBPWRRGetCurrentTask());
    LBPWRRYield();

    // A terminated task must never resume.
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

    VirtIOGPUCreateFrameBuffer(
        GPU->DisplayInfo.Displays[0].Rect.Width,
        GPU->DisplayInfo.Displays[0].Rect.Height);

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

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    TASK *Task;

    InitKernel(BootInfo);

    KPrint(L"Initializing scheduler stress test...", 40, 330,
           0xFFFFFF, 18, PRETENDARD);
    VirtIOGPUFlush();

    Task = TaskCreate(TestWorker0);
    TestWorkers[0] = Task;
    if (Task == NULL || !TaskSetPriority(Task, 0) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestWorker1);
    TestWorkers[1] = Task;
    if (Task == NULL || !TaskSetPriority(Task, 1) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestWorker2);
    TestWorkers[2] = Task;
    if (Task == NULL || !TaskSetPriority(Task, 3) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestWorker3);
    TestWorkers[3] = Task;
    if (Task == NULL || !TaskSetPriority(Task, 5) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestSleeper);
    TestSleepTask = Task;
    if (Task == NULL || !TaskSetPriority(Task, 3) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestYielder);
    TestYieldTask = Task;
    if (Task == NULL || !TaskSetPriority(Task, 3) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestKillWorker);
    TestKillTask = Task;
    if (Task == NULL || !TaskSetPriority(Task, 3) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestTerminateWorker);
    TestTerminateTask = Task;
    if (Task == NULL || !TaskSetPriority(Task, 3) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestController);
    TestControllerTask = Task;
    if (Task == NULL || !TaskSetPriority(Task, 3) || !LBPWRRAddTask(Task)) HLT();
    Task = TaskCreate(TestMonitor);
    TestMonitorTask = Task;
    if (Task == NULL || !TaskSetPriority(Task, 0) || !LBPWRRAddTask(Task)) HLT();

    KPrint(L"BEFORE START", 40, 370,
       0x80FF80, 18, PRETENDARD);
    VirtIOGPUFlush();

    LBPWRRStart();

    KPrint(L"AFTER START", 40, 410,
           0xFF8080, 18, PRETENDARD);
    VirtIOGPUFlush();

    HLT();

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
