// kernel/src/scheduler/lbpwrr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <scheduler/lbpwrr.h>

#include "render/gpu/virtio_gpu.h"

#include <scheduler/task.h>
#include <multicore/smp.h>
#include <arch/x86_64/cpu.h>
#include <debug/debug.h>
#include <memory/memory.h>

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP);

STATIC LBPWRR_CPU CPUState[SMP_MAX_CPUS];
STATIC CPU_USAGE CPUUsage[SMP_MAX_CPUS];

STATIC TASK *LBPWRRGetNextTask(UINT32 CPUID)
{
    LBPWRR_CPU *CPU;
    TASK *task;
    UINT32 i;
    UINT32 Index;

    if (CPUID >= SMPGetCPUCount())
    {
        return NULL;
    }

    CPU = &CPUState[CPUID];

    if (CPU->RunQueueCount == 0)
    {
        return NULL;
    }

    for (i = 0; i < CPU->RunQueueCount; i++)
    {
        CPU->RunQueueIndex++;

        if (CPU->RunQueueIndex >= CPU->RunQueueCount)
        {
            CPU->RunQueueIndex = 0;
        }

        Index = CPU->RunQueueIndex;
        task = CPU->RunQueue[Index];

        if (task == NULL)
        {
            continue;
        }

        if (task->State == TASK_READY || task->State == TASK_RUNNING)
        {
            return task;
        }
    }

    return NULL;
}

STATIC UINT32 LBPWRRFindLeastLoadedCPU(VOID)
{
    UINT32 CPUCount;
    UINT32 BestCPU;
    UINT32 BestCount;
    UINT32 i;

    CPUCount = SMPGetCPUCount();

    if (CPUCount == 0)
    {
        return 0;
    }

    BestCPU = 0;
    BestCount = CPUState[0].RunQueueCount;

    for (i = 1; i < CPUCount; i++)
    {
        if (CPUState[i].RunQueueCount < BestCount)
        {
            BestCPU = i;
            BestCount = CPUState[i].RunQueueCount;
        }
    }

    return BestCPU;
}

BOOLEAN LBPWRRInit(VOID)
{
    KMemSet(CPUState, 0, sizeof(CPUState));
    KMemSet(CPUUsage, 0, sizeof(CPUUsage));

    if (!TaskInit())
    {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN LBPWRRAddTask(TASK *task)
{
    LBPWRR_CPU *CPU;
    UINT32 CPUID;

    if (task == NULL)
    {
        return FALSE;
    }

    if (task->State == TASK_DEAD)
    {
        return FALSE;
    }

    CPUID = LBPWRRFindLeastLoadedCPU();

    if (CPUID >= SMPGetCPUCount())
    {
        return FALSE;
    }

    CPU = &CPUState[CPUID];

    if (CPU->RunQueueCount >= LBPWRR_MAX_TASKS)
    {
        return FALSE;
    }

    task->CPUID = CPUID;
    task->State = TASK_READY;

    CPU->RunQueue[CPU->RunQueueCount] = task;
    CPU->RunQueueCount++;

    return TRUE;
}

VOID LBPWRRStart(VOID)
{
    LBPWRR_CPU *CPU;
    TASK_CONTEXT *Context;
    TASK *task;
    UINT32 CPUID;

    // Start 진입: 노랑
    FillScreen(0xFFFF00);
    VirtIOGPUFlush();

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= SMPGetCPUCount())
    {
        // CPU ID 오류: 빨강
        FillScreen(0xFF0000);
        VirtIOGPUFlush();
        HLT();
    }

    CPU = &CPUState[CPUID];
    task = LBPWRRGetNextTask(CPUID);

    if (task == NULL)
    {
        // Task 없음: 마젠타
        FillScreen(0xFF00FF);
        VirtIOGPUFlush();
        HLT();
    }

    // Task 찾음: 시안
    FillScreen(0x00FFFF);
    VirtIOGPUFlush();

    task->State = TASK_RUNNING;

    CPU->CurrentTask = (UINT64) task;
    CPU->CurrentTaskValid = TRUE;
    CPU->SwitchStack = task->RSP;

    Context = (TASK_CONTEXT *) task->RSP;

    DebugWrite("LBPWRR RESTORE cpu=");
    DebugWriteDec(CPUID);
    DebugWrite(" task=");
    DebugWriteHex((UINT64) task);
    DebugWrite(" rsp=");
    DebugWriteHex(task->RSP);
    DebugWrite(" rip=");
    DebugWriteHex(Context->RIP);
    DebugWrite(" cs=");
    DebugWriteHex(Context->CS);
    DebugWrite(" rflags=");
    DebugWriteHex(Context->RFLAGS);
    DebugWrite(" vector=");
    DebugWriteHex(Context->Vector);
    DebugWrite(" error=");
    DebugWriteHex(Context->ErrorCode);
    DebugWrite("\n");

    // Context restore 직전: 초록
    FillScreen(0x00FF00);
    VirtIOGPUFlush();

    LBPWRRRestoreContext(task->RSP);

    // 여기 보이면 오히려 이상함: 파랑
    FillScreen(0x0000FF);
    VirtIOGPUFlush();

    HLT();
}

VOID LBPWRRJoin(VOID)
{
    LBPWRRStart();
}

VOID LBPWRRYield(VOID)
{
    ASM("int $0x43" ::: "memory");
}

UINT64 LBPWRRTick(INTERRUPT_FRAME *frame)
{
    LBPWRR_CPU *CPU;
    TASK *CurrentTask;
    TASK *NextTask;
    UINT32 CPUID;

    if (frame == NULL)
    {
        return 0;
    }

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= SMPGetCPUCount())
    {
        return 0;
    }

    CPU = &CPUState[CPUID];

    CurrentTask = NULL;

    if (CPU->CurrentTaskValid)
    {
        CurrentTask = (TASK *) CPU->CurrentTask;

        if (CurrentTask != NULL && CurrentTask->State == TASK_RUNNING)
        {
            CurrentTask->RSP = (UINT64) frame;
            CurrentTask->State = TASK_READY;
        }
    }

    NextTask = LBPWRRGetNextTask(CPUID);

    if (NextTask == NULL)
    {
        if (CurrentTask != NULL && CurrentTask->State != TASK_DEAD)
        {
            CurrentTask->State = TASK_RUNNING;

            CPU->CurrentTask = (UINT64) CurrentTask;
            CPU->CurrentTaskValid = TRUE;
            CPU->SwitchStack = CurrentTask->RSP;

            return CurrentTask->RSP;
        }

        CPU->CurrentTask = 0;
        CPU->CurrentTaskValid = FALSE;
        CPU->SwitchStack = 0;

        return 0;
    }

    NextTask->State = TASK_RUNNING;

    CPU->CurrentTask = (UINT64) NextTask;
    CPU->CurrentTaskValid = TRUE;
    CPU->SwitchStack = NextTask->RSP;

    return NextTask->RSP;
}

TASK *LBPWRRGetCurrentTask(VOID)
{
    UINT32 CPUID;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= SMPGetCPUCount())
    {
        return NULL;
    }

    if (!CPUState[CPUID].CurrentTaskValid)
    {
        return NULL;
    }

    return (TASK *) CPUState[CPUID].CurrentTask;
}

UINTN LBPWRRGetCPUUsage(UINT32 CPUID)
{
    if (CPUID >= SMPGetCPUCount())
    {
        return 0;
    }

    return CPUUsage[CPUID].CPUUsage;
}

UINT32 LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID)
{
    if (CPUID >= SMPGetCPUCount())
    {
        return 0;
    }

    return CPUState[CPUID].RunQueueCount;
}
