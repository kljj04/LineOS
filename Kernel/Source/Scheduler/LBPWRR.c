// Kernel/Source/Scheduler/LBPWRR.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/LBPWRR.h>
#include <Scheduler/Usage.h>
#include <Debug/Panic.h>
#include <Scheduler/LBPWRRChunk.h>
#include <Scheduler/LBPWRRRunQueue.h>
#include <Scheduler/LBPWRRHelper.h>
#include <Scheduler/Task.h>
#include <Multicore/SMP.h>
#include <Memory/PMM.h>
#include <Timer/TSC.h>
#include <Arch/AMD64/CPU.h>
#include <Memory/VMM.h>

EXTERN VOID LBPWRRRestoreContext(UINT64 RSP) NORETURN;

STATIC TASK *IdleTasks[SMP_MAX_CPUS];
STATIC TASK *SwitchingTasks[SMP_MAX_CPUS];

BOOLEAN LBPWRRInit(VOID)
{
    UINT32 CPUCount;
    UINT32 CPUID;

    if (!TaskInit())
    {
        return FALSE;
    }

    CPUCount = SMPGetCPUCount();

    if (CPUCount == 0)
    {
        CPUCount = 1;
    }

    if (CPUCount > SMP_MAX_CPUS)
    {
        CPUCount = SMP_MAX_CPUS;
    }

    LBPWRRInitRunQueues(CPUCount);
    UsageInit(CPUCount);

    for (CPUID = 0; CPUID < CPUCount; CPUID++)
    {
        IdleTasks[CPUID] = TaskCreateIdle();

        if (IdleTasks[CPUID] == NULL || !LBPWRRAddTask(IdleTasks[CPUID]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

VOID LBPWRRStart(VOID)
{
    UINT32           CPUID;
    UINT32           Index;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *Task;
    UINT64           Flags;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);
    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    Task = NULL;

    for (Index = 0; Index < RunQueue->Chunk.Count; Index++)
    {
        TASK *Candidate;

        Candidate = RunQueue->Chunk.Tasks[Index];

        if (Candidate == NULL || Candidate->State != TASK_READY || Candidate->Idle)
        {
            continue;
        }

        Task = Candidate;
        RunQueue->CurrentIndex = Index;
        break;
    }

    if (Task == NULL)
    {
        for (Index = 0; Index < RunQueue->Chunk.Count; Index++)
        {
            TASK *Candidate;

            Candidate = RunQueue->Chunk.Tasks[Index];

            if (Candidate == NULL || Candidate->State != TASK_READY || !Candidate->Idle)
            {
                continue;
            }

            Task = Candidate;
            RunQueue->CurrentIndex = Index;
            break;
        }
    }

    if (Task == NULL)
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

        return;
    }

    LBPWRRSetCurrentTask(CPUID, Task);
    Task->State = TASK_RUNNING;
    Task->OnCPU = TRUE;

    if (!GDTSetKernelStack(Task->KernelStackBase + Task->KernelStackSize))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
        Panic(L"LBPWRR: Failed to set RSP0");
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

    if (Task->AddressSpace != NULL && !VMMSwitchAddressSpace(Task->AddressSpace))
    {
        Panic(L"LBPWRR: Initial CR3 switch failed");
    }

    UsageStart(CPUID);

    TSCSetDeadline(RDTSC() + TSCGetFrequency() / 200);

    LBPWRRRestoreContext(Task->RSP);
}

VOID LBPWRRJoin(VOID)
{
    UINT32 CPUID;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    if (LBPWRRGetCurrentTaskFromCPU(CPUID) == NULL)
    {
        LBPWRRStart();
    }
}

VOID LBPWRRYield(VOID)
{
    ASM("int $0x43" :: : "memory");
}

UINT64 LBPWRRTick(INTERRUPT_FRAME *Frame)
{
    UINT32           CPUID;
    UINT32           Index;
    UINT32           StartIndex;
    UINT32           CandidateIndex;
    UINT32           ChunkCount;
    LBPWRR_RUNQUEUE *RunQueue;
    TASK            *CurrentTask;
    TASK            *NextTask;
    TASK            *Candidate;
    UINT64           Flags;
    UINT64           NextRSP;
    UINT64           DeltaTSC;

    if (Frame == NULL)
    {
        return 0;
    }

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount() || CPUID >= SMP_MAX_CPUS)
    {
        return (UINT64) Frame;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);

    if (RunQueue == NULL)
    {
        return (UINT64) Frame;
    }

    Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

    CurrentTask = LBPWRRGetCurrentTaskFromCPU(CPUID);

    if (CurrentTask != NULL)
    {
        DeltaTSC = UsageAccount(CPUID, CurrentTask->Idle);

        ATOMIC_FETCH_ADD(&CurrentTask->CPUTimeTSC, DeltaTSC, ATOMIC_RELAXED);

        UsageSample(CPUID);
    }

    LBPWRRWakeSleepingTasks(RunQueue);

    if (CurrentTask != NULL)
    {
        CurrentTask->RSP = (UINT64) Frame;

        if (CurrentTask->State == TASK_RUNNING)
        {
            CurrentTask->State = TASK_READY;
        }
    }

    NextTask = NULL;
    ChunkCount = RunQueue->Chunk.Count;
    StartIndex = RunQueue->CurrentIndex;

    if (StartIndex >= ChunkCount)
    {
        StartIndex = 0;
    }

    if (ChunkCount > 0)
    {
        for (Index = 0; Index < ChunkCount; Index++)
        {
            if (CurrentTask == NULL)
            {
                CandidateIndex = (StartIndex + Index) % ChunkCount;
            }
            else
            {
                CandidateIndex = (StartIndex + Index + 1) % ChunkCount;
            }

            Candidate = RunQueue->Chunk.Tasks[CandidateIndex];

            if (Candidate == NULL || Candidate->State != TASK_READY || Candidate->Idle)
            {
                continue;
            }

            NextTask = Candidate;

            RunQueue->CurrentIndex = CandidateIndex;

            break;
        }
    }

    if (NextTask == NULL && ChunkCount > 0)
    {
        for (Index = 0; Index < ChunkCount; Index++)
        {
            CandidateIndex = (StartIndex + Index) % ChunkCount;

            Candidate = RunQueue->Chunk.Tasks[CandidateIndex];

            if (Candidate == NULL || Candidate->State != TASK_READY || !Candidate->Idle)
            {
                continue;
            }

            NextTask = Candidate;

            RunQueue->CurrentIndex = CandidateIndex;

            break;
        }
    }

    if (NextTask == NULL)
    {
        if (CurrentTask != NULL && CurrentTask->State == TASK_READY)
        {
            CurrentTask->State = TASK_RUNNING;

            NextTask = CurrentTask;
            NextRSP = (UINT64) Frame;
        }
        else
        {
            if (CurrentTask != NULL && CurrentTask->State != TASK_RUNNING && CurrentTask->State != TASK_READY)
            {
                Panic(L"LBPWRR: No runnable task");
            }

            NextRSP = (UINT64) Frame;
        }
    }
    else
    {
        LBPWRRSetCurrentTask(CPUID, NextTask);

        NextTask->State = TASK_RUNNING;
        NextTask->OnCPU = TRUE;
        NextRSP = NextTask->RSP;
    }

    if (CurrentTask != NULL && NextTask != CurrentTask)
    {
        SwitchingTasks[CPUID] = CurrentTask;
    }

    if (NextTask != NULL)
    {
        if (NextTask->KernelStackBase == 0 || NextTask->KernelStackSize == 0 || !GDTSetKernelStack(NextTask->KernelStackBase + NextTask->KernelStackSize))
        {
            SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

            Panic(L"LBPWRR: Failed to set RSP0");
        }
    }

    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

    UsageSampleTasks();

    if (NextTask != NULL && (CurrentTask == NULL ? NextTask->AddressSpace != NULL : NextTask->AddressSpace != CurrentTask->AddressSpace))
    {
        if (!VMMSwitchAddressSpace(NextTask->AddressSpace))
        {
            Panic(L"LBPWRR: CR3 switch failed");
        }
    }

    return NextRSP;
}

VOID LBPWRRCompleteSwitch(VOID)
{
    UINT32           CPUID;
    TASK            *Task;
    LBPWRR_RUNQUEUE *RunQueue;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID >= LBPWRRGetActiveCPUCount())
    {
        return;
    }

    Task = SwitchingTasks[CPUID];

    if (Task == NULL)
    {
        return;
    }

    RunQueue = LBPWRRGetRunQueue(CPUID);
    SpinLockAcquire(&RunQueue->Lock);
    Task->OnCPU = FALSE;
    SwitchingTasks[CPUID] = NULL;
    SpinLockRelease(&RunQueue->Lock);
}
