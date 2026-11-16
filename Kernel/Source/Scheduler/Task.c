// Kernel/Source/Scheduler/Task.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Scheduler/Task.h>
#include <Syscall/IPC/SyscallIPC.h>
#include <Syscall/File/SyscallFile.h>
#include <Syscall/Memory/SyscallMemory.h>
#include <Scheduler/WaitQueue.h>
#include <Scheduler/LBPWRRRunQueue.h>
#include <Scheduler/LBPWRRChunk.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/LBPWRRTypes.h>
#include <Arch/AMD64/CPU.h>
#include <Debug/Debug.h>
#include <Memory/PMM.h>
#include <Timer/TSC.h>

STATIC TASK_SLOT* TaskSlots;
STATIC UINT32 TaskSlotCount;
STATIC UINT32 TaskSlotCapacity;
STATIC UINTN TaskCount;
STATIC UINT16 NextPID;
STATIC SPIN_LOCK TaskLock;

STATIC NORETURN VOID TaskExit(VOID)
{
    TASK* Task;

    Task = LBPWRRGetCurrentTask();

    if (Task != NULL)
    {
        TaskTerminate(Task);
    }

    for (;;)
    {
        LBPWRRYield();
    }
}

STATIC NORETURN VOID TaskIdle(VOID)
{
    while (TRUE)
    {
        ASM("sti; hlt" :: : "memory");
    }
}

TASK* TaskCreateIdle(VOID)
{
    TASK* Task;

    Task = TaskCreate(TaskIdle);

    if (Task != NULL)
    {
        Task->Idle = TRUE;
    }

    return Task;
}

STATIC UINT8 PToW(UINT8 Priority)
{
    if (Priority < 1 || Priority > LBPWRR_MAX_WEIGHT)
    {
        return 0;
    }

    return Priority;
}

STATIC BOOLEAN TaskPIDUsed(UINT16 PID)
{
    for (UINT32 Index = 0; Index < TaskSlotCount; Index++)
    {
        TASK* Task = TaskSlots[Index].Task;

        if (Task != NULL && Task->PID == PID)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC BOOLEAN TaskAllocatePID(UINT16* PID)
{
    UINT32 Retry;
    UINT16 Candidate;

    if (TaskCount == UINT16_MAX)
    {
        return FALSE;
    }

    Candidate = NextPID;

    for (Retry = 0; Retry < UINT16_MAX; Retry++)
    {
        if (Candidate == 0)
        {
            Candidate = 1;
        }

        if (!TaskPIDUsed(Candidate))
        {
            *PID = Candidate;

            Candidate++;

            if (Candidate == 0)
            {
                Candidate = 1;
            }

            NextPID = Candidate;

            return TRUE;
        }

        Candidate++;
    }

    return FALSE;
}

STATIC BOOLEAN TaskRegister(TASK* Task)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    UINT32 Index = 0;

    while (Index < TaskSlotCount && TaskSlots[Index].Task != NULL)
    {
        Index++;
    }

    if (!TaskAllocatePID(&Task->PID))
    {
        SpinLockReleaseIRQRestore(&TaskLock, Flags);

        return FALSE;
    }

    if (Index == TaskSlotCapacity)
    {
        TASK_SLOT* Slots = LBPWRRGrowArray(TaskSlots, &TaskSlotCapacity, Index + 1, sizeof(*Slots));

        if (Slots == NULL)
        {
            SpinLockReleaseIRQRestore(&TaskLock, Flags);

            return FALSE;
        }

        TaskSlots = Slots;
    }

    if (Index == TaskSlotCount)
    {
        TaskSlotCount++;
    }

    TaskSlots[Index].Task = Task;
    TaskSlots[Index].Destroying = FALSE;
    TaskCount++;
    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    return TRUE;
}

STATIC VOID InitTaskContext(TASK* Task, VOID (*Entry)(VOID))
{
    UINT64* ReturnAddress;

    KMemSet(&Task->Context, 0, sizeof(TASK_CONTEXT));

    ReturnAddress = (UINT64*)Task->InitialRSP - 1;
    *ReturnAddress = (UINT64)TaskExit;
    Task->InitialRSP = (UINT64)ReturnAddress;

    Task->Context.RIP = (UINT64)Entry;
    Task->Context.CS = GDT_KERNEL_CODE_SELECTOR;
    Task->Context.RFLAGS = 0x202;
    Task->Context.RSP = Task->InitialRSP;
    Task->Context.SS = GDT_KERNEL_DATA_SELECTOR;

    Task->RSP = Task->InitialRSP - sizeof(TASK_CONTEXT);
    KMemCpy((VOID*)Task->RSP, &Task->Context, sizeof(TASK_CONTEXT));
}

BOOLEAN TaskInit(VOID)
{
    SpinLockInit(&TaskLock);
    NextPID = 1;

    return TRUE;
}

TASK* TaskCreate(VOID (*Entry)(VOID))
{
    TASK* Task;

    if (Entry == NULL)
    {
        return NULL;
    }

    Task = KAlloc(sizeof(*Task));

    if (Task == NULL)
    {
        return NULL;
    }

    KMemSet(Task, 0, sizeof(*Task));

    Task->Priority = 3;
    Task->Weight = PToW(Task->Priority);

    Task->StackBase = (UINT64)KAllocPagesBelow(TASK_STACK_PAGES, TASK_STACK_ADDRESS_LIMIT);

    if (Task->StackBase == 0)
    {
        KFree(Task);

        return NULL;
    }

    Task->KernelStackBase = (UINT64)KAllocPagesBelow(TASK_STACK_PAGES, TASK_STACK_ADDRESS_LIMIT);

    if (Task->KernelStackBase == 0)
    {
        KMemFreePages((VOID*)Task->StackBase, TASK_STACK_PAGES);

        KFree(Task);

        return NULL;
    }

    Task->KernelStackSize = TASK_STACK_PAGES * PAGE_SIZE;

    Task->StackSize = TASK_STACK_PAGES * PAGE_SIZE;
    Task->InitialRSP = Task->StackBase + Task->StackSize;

    Task->State = TASK_READY;

    InitTaskContext(Task, Entry);

    Task->Modified = FALSE;
    Task->Idle = FALSE;
    Task->Scheduled = FALSE;

    if (!TaskRegister(Task))
    {
        KMemFreePages((VOID*)Task->KernelStackBase, TASK_STACK_PAGES);
        KMemFreePages((VOID*)Task->StackBase, TASK_STACK_PAGES);
        KFree(Task);

        return NULL;
    }

    return Task;
}

BOOLEAN TaskSetPriority(TASK* Task, UINT8 Priority)
{
    LBPWRR_RUNQUEUE* RunQueue;
    UINT32 CPUID;
    UINT64 Flags;
    UINT8 Weight;

    if (Task == NULL || Priority < 1 || Priority > LBPWRR_MAX_WEIGHT)
    {
        return FALSE;
    }

    if (!Task->Scheduled)
    {
        Task->Priority = Priority;
        Task->Weight = PToW(Priority);
        Task->Modified = TRUE;

        return TRUE;
    }

    for (;;)
    {
        CPUID = Task->CPUID;
        RunQueue = LBPWRRGetRunQueue(CPUID);
        Flags = SpinLockAcquireIRQSave(&RunQueue->Lock);

        if (Task->CPUID == CPUID)
        {
            break;
        }

        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);
    }

    Weight = 0;

    if (Task->State == TASK_READY || Task->State == TASK_RUNNING || (Task->State == TASK_BLOCKED && Task->WaitQueue !=
        NULL))
    {
        Weight = PToW(Priority);
    }

    if (!LBPWRRModifyChunk(RunQueue, Task, Weight))
    {
        SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

        return FALSE;
    }

    Task->Priority = Priority;
    Task->Weight = PToW(Priority);
    Task->Modified = FALSE;
    SpinLockReleaseIRQRestore(&RunQueue->Lock, Flags);

    return TRUE;
}

VOID TaskSleepMls(UINT64 Milliseconds)
{
    TASK* Task;
    UINT64 Ticks;
    UINT64 Frequency;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        SleepMls(Milliseconds);

        return;
    }

    if (Milliseconds == 0)
    {
        LBPWRRYield();

        return;
    }

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return;
    }

    Ticks = (UINT64)(((UINT128)Frequency * Milliseconds + 999ULL) / 1000ULL);

    Task->WakeDeadlineTSC = RDTSC() + Ticks;
    Task->State = TASK_SLEEPING;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
    LBPWRRYield();
}

VOID TaskSleepMis(UINT64 Microseconds)
{
    TASK* Task;
    UINT64 Ticks;
    UINT64 Frequency;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        SleepMis(Microseconds);

        return;
    }

    if (Microseconds == 0)
    {
        LBPWRRYield();

        return;
    }

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return;
    }

    Ticks = (UINT64)(((UINT128)Frequency * Microseconds + 999999ULL) / 1000000ULL);

    Task->WakeDeadlineTSC = RDTSC() + Ticks;
    Task->State = TASK_SLEEPING;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
    LBPWRRYield();
}

VOID TaskSleepNs(UINT64 Nanoseconds)
{
    TASK* Task;
    UINT64 Ticks;
    UINT64 Frequency;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL)
    {
        SleepNs(Nanoseconds);

        return;
    }

    if (Nanoseconds == 0)
    {
        LBPWRRYield();

        return;
    }

    Frequency = TSCGetFrequency();

    if (Frequency == 0)
    {
        return;
    }

    Ticks = (UINT64)(((UINT128)Frequency * Nanoseconds + 999999999ULL) / 1000000000ULL);

    Task->WakeDeadlineTSC = RDTSC() + Ticks;
    Task->State = TASK_SLEEPING;
    Task->Modified = TRUE;

    LBPWRRNotifyTaskChanged(Task);
    LBPWRRYield();
}

BOOLEAN TaskDestroy(TASK* Task)
{
    if (Task == NULL)
    {
        return FALSE;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    UINT32 Index = 0;

    while (Index < TaskSlotCount && TaskSlots[Index].Task != Task)
    {
        Index++;
    }

    if (Index == TaskSlotCount || TaskSlots[Index].Destroying || (Task->State != TASK_TERMINATED && Task->State !=
        TASK_KILLED) || (Task->Scheduled && !LBPWRRRemoveTask(Task)))
    {
        SpinLockReleaseIRQRestore(&TaskLock, Flags);

        return FALSE;
    }

    TaskSlots[Index].Destroying = TRUE;
    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    if (Task->StackBase != 0)
    {
        KMemFreePages((VOID*)Task->StackBase, TASK_STACK_PAGES);
    }

    if (Task->KernelStackBase != 0)
    {
        KMemFreePages((VOID*)Task->KernelStackBase, TASK_STACK_PAGES);
    }

    SyscallMemoryRelease(Task);
    SyscallIPCRelease(Task);
    SyscallFilesRelease(Task);

    if (Task->FileContext != NULL)
    {
        KFree(Task->FileContext);
    }

    Flags = SpinLockAcquireIRQSave(&TaskLock);
    TaskSlots[Index].Task = NULL;
    TaskCount--;
    SpinLockReleaseIRQRestore(&TaskLock, Flags);
    KFree(Task);

    return TRUE;
}

VOID TaskKill(TASK* Task)
{
    if (Task == NULL)
    {
        return;
    }

    WaitQueueFinishTask(Task, TASK_KILLED);
}

VOID TaskTerminate(TASK* Task)
{
    if (Task == NULL)
    {
        return;
    }

    WaitQueueFinishTask(Task, TASK_TERMINATED);
}

TASK* TaskGetByPID(UINT16 PID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    TASK* Result = NULL;

    for (UINT32 Index = 0; Index < TaskSlotCount; Index++)
    {
        TASK* Task = TaskSlots[Index].Task;

        if (Task != NULL && !TaskSlots[Index].Destroying && Task->PID == PID)
        {
            Result = Task;
            break;
        }
    }

    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    return Result;
}

UINTN TaskGetCount(VOID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    UINTN Count = TaskCount;

    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    return Count;
}

VOID TaskSampleCPUUsage(UINT64 IntervalTSC)
{
    UINT64 Current;
    UINT64 Previous;
    UINT64 Delta;
    UINT64 Percent;

    if (IntervalTSC == 0)
    {
        return;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);

    for (UINT32 Index = 0; Index < TaskSlotCount; Index++)
    {
        TASK* Task = TaskSlots[Index].Task;

        if (Task == NULL || TaskSlots[Index].Destroying)
        {
            continue;
        }

        Current = ATOMIC_LOAD(&Task->CPUTimeTSC, ATOMIC_RELAXED);
        Previous = Task->LastCPUTimeTSC;
        Delta = Current - Previous;

        Task->LastCPUTimeTSC = Current;

        Percent = (UINT64)(((UINT128)Delta * 100) / IntervalTSC);

        if (Percent > 100)
        {
            Percent = 100;
        }

        ATOMIC_STORE(&Task->CPUUsage, (UINT32) Percent, ATOMIC_RELEASE);
    }

    SpinLockReleaseIRQRestore(&TaskLock, Flags);
}

UINT32 TaskGetSnapshot(SYSTEM_TASK_INFO* Entries, UINT32 Capacity)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    UINT32 Count = 0;

    for (UINT32 Pass = 0; Pass < 2; Pass++)
    {
        for (UINT32 Index = 0; Index < TaskSlotCount && Count < Capacity; Index++)
        {
            TASK* Task = TaskSlots[Index].Task;

            if (Task == NULL || TaskSlots[Index].Destroying)
            {
                continue;
            }

            TASK_STATE State = ATOMIC_LOAD(&Task->State, ATOMIC_ACQUIRE);

            if (State == TASK_TERMINATED || State == TASK_KILLED)
            {
                continue;
            }

            if ((Task->Idle ? 1U : 0U) != Pass)
            {
                continue;
            }

            UINT16 PID = Task->PID;
            SYSTEM_TASK_INFO Entry = {0};

            Entry.PID = PID;
            Entry.UID = Task->RealUID;
            Entry.CPU = Task->CPUID;
            Entry.Usage = ATOMIC_LOAD(&Task->CPUUsage, ATOMIC_ACQUIRE);
            Entry.State = State;
            Entry.Kernel = Task->AddressSpace == NULL;
            Entry.Idle = Task->Idle;
            KMemCpy(Entry.Name, Task->Name, sizeof(Entry.Name) - 1);

            State = ATOMIC_LOAD(&Task->State, ATOMIC_ACQUIRE);

            if (State == TASK_TERMINATED || State == TASK_KILLED)
            {
                continue;
            }

            Entries[Count++] = Entry;
        }
    }

    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    return Count;
}

UINT32 TaskGetIdleCount(VOID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    UINT32 Count = 0;

    for (UINT32 Index = 0; Index < TaskSlotCount; Index++)
    {
        TASK* Task = TaskSlots[Index].Task;

        if (Task != NULL && !TaskSlots[Index].Destroying && Task->Idle)
        {
            Count++;
        }
    }

    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    return Count;
}

UINT32 TaskGetLiveCount(VOID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TaskLock);
    UINT32 Count = 0;

    for (UINT32 Index = 0; Index < TaskSlotCount; Index++)
    {
        TASK* Task = TaskSlots[Index].Task;

        if (Task == NULL || TaskSlots[Index].Destroying)
        {
            continue;
        }

        TASK_STATE State = ATOMIC_LOAD(&Task->State, ATOMIC_ACQUIRE);

        if (State != TASK_TERMINATED && State != TASK_KILLED)
        {
            Count++;
        }
    }

    SpinLockReleaseIRQRestore(&TaskLock, Flags);

    return Count;
}
