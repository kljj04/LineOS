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

#define TEST_SAMPLE_MLS 500ULL
#define TEST_SLEEP_MLS  50ULL

STATIC VOLATILE BOOLEAN SchedulerReady;
STATIC BOOLEAN          SchedulerInitialized;
STATIC BOOLEAN          TestRuntimeChecked;
STATIC TASK            *TestWorkers[SMP_MAX_CPUS];
STATIC TASK            *TestSleeper;
STATIC UINT32           TestCPUCount;
STATIC UINT32           TestWorkerCount;
STATIC UINT32           TestIdleCPUID;
STATIC UINT32           TestFailures;
STATIC UINT32           TestPage;
STATIC UINT64           TestStartTSC;
STATIC UINTN            TestCPUUsages[SMP_MAX_CPUS];
STATIC VOLATILE UINT64  TestWorkerTicks[SMP_MAX_CPUS];
STATIC VOLATILE UINT64  TestCPUWork[SMP_MAX_CPUS];
STATIC VOLATILE UINT64  TestWakeCount;
STATIC VOLATILE UINT32  TestWorkerFaults;

STATIC BOOLEAN TestCheck(BOOLEAN result, CONST char *name)
{
    DebugWrite(result ? "PASS " : "FAIL ");
    DebugWriteLine(name);

    if (!result)
    {
        TestFailures++;
    }

    return result;
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

    SchedulerInitialized = LBPWRRInit();
}

STATIC VOID TestWorkerTask(VOID)
{
    TASK   *task;
    UINT32  WorkerIndex;
    UINT32  index;
    UINT32  CPUID;

    task = LBPWRRGetCurrentTask();
    WorkerIndex = TestWorkerCount;

    for (index = 0; index < TestWorkerCount; index++)
    {
        if (TestWorkers[index] == task)
        {
            WorkerIndex = index;
            break;
        }
    }

    if (WorkerIndex == TestWorkerCount)
    {
        __atomic_fetch_add(&TestWorkerFaults, 1, __ATOMIC_RELAXED);

        while (TRUE)
        {
            LBPWRRYield();
        }
    }

    while (TRUE)
    {
        CPUID = SMPGetCurrentCPUID();

        if (CPUID >= TestCPUCount || LBPWRRGetCurrentTask() != task)
        {
            __atomic_fetch_add(&TestWorkerFaults, 1, __ATOMIC_RELAXED);
        }
        else
        {
            TestCPUWork[CPUID]++;
        }

        TestWorkerTicks[WorkerIndex]++;

        if (task == TestSleeper)
        {
            TaskSleepMls(TEST_SLEEP_MLS);
            TestWakeCount++;
        }
        else
        {
            for (index = 0; index < 100000; index++)
            {
                PAUSE();
            }
        }

        LBPWRRYield();
    }
}

STATIC VOID TestMonitorTask(VOID)
{
    UINT64 SampleTicks;
    UINT64 LastSampleTSC;
    UINT64 now;
    UINT32 CPUID;
    UINT32 rows;
    UINT32 row;
    UINT32 FirstCPUID;
    UINTN  usage;

    SampleTicks = (UINT64) (((UINT128) TSCGetFrequency() * TEST_SAMPLE_MLS) / 1000ULL);
    LastSampleTSC = RDTSC();

    while (TRUE)
    {
        now = RDTSC();

        if (now - LastSampleTSC < SampleTicks)
        {
            LBPWRRYield();
            continue;
        }

        LastSampleTSC = now;

        for (CPUID = 0; CPUID < TestCPUCount; CPUID++)
        {
            TestCPUUsages[CPUID] = LBPWRRGetCPUUsage(CPUID);
        }

        if (!TestRuntimeChecked && (UINT128) (now - TestStartTSC) >= (UINT128) TSCGetFrequency() * 3ULL)
        {
            for (CPUID = 0; CPUID < TestWorkerCount; CPUID++)
            {
                TestCheck(TestWorkerTicks[CPUID] != 0, "worker made progress");
            }

            for (CPUID = 0; CPUID < TestCPUCount; CPUID++)
            {
                usage = TestCPUUsages[CPUID];
                TestCheck(usage <= 100, "CPU usage range");

                if (CPUID == TestIdleCPUID && TestCPUCount > 1)
                {
                    TestCheck(usage == 0, "idle CPU usage");
                }
                else if (LBPWRRGetCPUAssignedTaskCount(CPUID) != 0)
                {
                    TestCheck(usage != 0, "active CPU usage");
                }
            }

            TestCheck(TestWakeCount != 0, "sleeping task woke");
            TestCheck(TestWorkerFaults == 0, "worker stayed on its current CPU");
            TestRuntimeChecked = TRUE;
        }

        rows = VirtIOGPUGetFrameBufferHeight();
        rows = rows > 210 ? (rows - 210) / 32 : 1;

        if (rows > TestCPUCount)
        {
            rows = TestCPUCount;
        }

        FirstCPUID = (TestPage * rows) % TestCPUCount;
        TestPage++;

        FillScreen(0x101010);
        KPrint(L"LBPWRR / CPU USAGE TEST", 48, 56, 0xFFFFFF, 30, JETBRAINS_MONO);
        KPrint(L"CPUs %u  tasks %llu  fails %u  wake %llu", 48, 100, 0xC8CDD4, 22, JETBRAINS_MONO, TestCPUCount, (UINT64) TaskGetCount(), TestFailures, TestWakeCount);
        KPrint(L"CPU   load   tasks   work", 48, 145, 0x86A6CC, 20, JETBRAINS_MONO);

        for (row = 0; row < rows; row++)
        {
            CPUID = (FirstCPUID + row) % TestCPUCount;
            usage = TestCPUUsages[CPUID];
            KPrint(L"CPU %u  %llu%%  tasks %u  work %llu", 48, 178 + row * 32, usage == 0 ? 0x9AA0AA : 0x55E6A5, 21, JETBRAINS_MONO, CPUID, (UINT64) usage, LBPWRRGetCPUAssignedTaskCount(CPUID), TestCPUWork[CPUID]);
        }

        VirtIOGPUFlush();
        LBPWRRYield();
    }
}

STATIC BOOLEAN TestSetup(VOID)
{
    TASK   *MonitorTask;
    TASK   *task;
    UINT32  index;
    UINT32  TargetCPUID;

    if (!TestCheck(SchedulerInitialized, "LBPWRRInit"))
    {
        return FALSE;
    }

    if (!TestCheck(TSCGetFrequency() != 0, "TSC calibrated"))
    {
        return FALSE;
    }

    TestCPUCount = SMPGetCPUCount();

    if (TestCPUCount == 0)
    {
        TestCPUCount = 1;
    }

    if (TestCPUCount > SMP_MAX_CPUS)
    {
        TestCPUCount = SMP_MAX_CPUS;
    }

    TestWorkerCount = TestCPUCount > 1 ? TestCPUCount - 1 : 1;
    TestCheck(LBPWRRGetCurrentTask() == NULL, "no current task before start");
    TestCheck(LBPWRRTick(NULL) == 0, "null interrupt frame");
    TestCheck(!LBPWRRAddTask(NULL), "null task rejected");
    TestCheck(!LBPWRRMoveTask(NULL, 0), "null move rejected");
    TestCheck(!LBPWRRNotifyTaskChanged(NULL), "null change rejected");
    TestCheck(LBPWRRGetTaskFromCPU(TestCPUCount) == NULL, "invalid CPU task lookup");
    TestCheck(LBPWRRGetCPUAssignedTaskCount(TestCPUCount) == 0, "invalid CPU task count");
    TestCheck(LBPWRRGetCPUUsage(TestCPUCount) == 0, "invalid CPU usage");
    TestCheck(TaskCreate(NULL) == NULL, "null entry rejected");

    MonitorTask = TaskCreate(TestMonitorTask);

    if (!TestCheck(MonitorTask != NULL, "monitor created"))
    {
        return FALSE;
    }

    if (!TestCheck(LBPWRRAddTask(MonitorTask), "monitor added"))
    {
        return FALSE;
    }

    TestCheck(TaskGetByPID(MonitorTask->PID) == MonitorTask, "PID lookup");
    TestCheck(TaskSetPriority(MonitorTask, 5), "monitor priority changed");
    TestCheck(!TaskSetPriority(MonitorTask, 6), "invalid priority rejected");
    TestCheck(LBPWRRNotifyTaskChanged(MonitorTask), "chunk rebuild requested");

    for (index = 0; index < TestWorkerCount; index++)
    {
        task = TaskCreate(TestWorkerTask);

        if (!TestCheck(task != NULL, "worker created"))
        {
            return FALSE;
        }

        TestWorkers[index] = task;

        if (!TestCheck(LBPWRRAddTask(task), "worker added"))
        {
            return FALSE;
        }

        TestCheck(TaskSetPriority(task, (UINT8) (index % 6)), "worker priority changed");
    }

    TestSleeper = TestWorkers[0];

    if (TestCPUCount > 1)
    {
        TestIdleCPUID = TestSleeper->CPUID;
        TargetCPUID = TestCPUCount > 2 ? TestCPUCount - 1 : 0;

        if (!TestCheck(LBPWRRMoveTask(TestSleeper, TargetCPUID), "task migrated"))
        {
            return FALSE;
        }

        TestCheck(LBPWRRGetCPUAssignedTaskCount(TestIdleCPUID) == 0, "source CPU became idle");
        TestCheck(LBPWRRGetTaskFromCPU(TargetCPUID) != NULL, "target CPU has task");
        TestCheck(!LBPWRRMoveTask(TestSleeper, TargetCPUID), "same CPU move rejected");
        TestCheck(!LBPWRRMoveTask(TestSleeper, TestCPUCount), "invalid CPU move rejected");
    }

    TestCheck(TaskGetCount() == TestWorkerCount + 1, "task count");
    TestStartTSC = RDTSC();

    return TRUE;
}

VOID MS_ABI KMain(LINEOS_BOOT_INFO *BootInfo)
{
    InitKernel(BootInfo);

    if (!TestSetup())
    {
        HLT();
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

    while (!SchedulerReady)
    {
        PAUSE();
    }

    CompilerBarrier();

    LBPWRRJoin();

    HLT();
}
