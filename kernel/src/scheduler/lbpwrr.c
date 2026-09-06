// kernel/src/scheduler/lbpwrr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/typeinfo.h>
#include <scheduler/lbpwrr.h>

#include <scheduler/lbpwrr_types.h>
#include <multicore/smp.h>
#include <memory/memory.h>
#include <timer/tsc.h>
#include <arch/x86_64/cpu.h>
#include <debug/panic.h>
#include <debug/debug.h>

#define LBPWRR_MAX_TASKS         4096
#define LBPWRR_TASK_STACK_PAGES 16
#define LBPWRR_PAGE_SIZE         4096
#define LBPWRR_QUANTUM_MS        5
#define LBPWRR_YIELD_VECTOR      0x43
#define LBPWRR_TIMER_VECTOR      0x40
#define LBPWRR_INITIAL_RFLAGS    0x202ULL
#define LBPWRR_KERNEL_RIP_MIN    0x400000ULL
#define LBPWRR_KERNEL_RIP_MAX    0x80000000ULL

STATIC TASK       Tasks[LBPWRR_MAX_TASKS];
STATIC LBPWRR_CPU CPUStates[SMP_MAX_CPUS];
STATIC UINT64     IdleTasks[SMP_MAX_CPUS];

STATIC UINT64           TaskCount = 0;
STATIC UINT32           NextCPU = 0;
STATIC VOLATILE BOOLEAN SchedulerStarted = FALSE;

VOLATILE UINT64 LBPWRRDebugRSPAfterSwitch = 0;
VOLATILE UINT64 LBPWRRDebugRSPBeforeIRET = 0;

EXTERN VOID LBPWRRRestoreContext(UINT64 stack);

STATIC BOOLEAN LBPWRRCreateTaskForCPU(VOID (*Entry)(VOID), UINT32 CPUID, BOOLEAN IsIdle);


STATIC VOID LBPWRRUpdateCPUUsage(LBPWRR_CPU *CPUState)
{
    UINT64 now;
    UINT64 delta;

    now = RDTSC();

    if (CPUState->LastTSC == 0)
    {
        CPUState->LastTSC = now;
        return;
    }

    delta = now - CPUState->LastTSC;
    CPUState->LastTSC = now;

    if (CPUState->CurrentTaskValid &&
        CPUState->CurrentTask < TaskCount &&
        !Tasks[CPUState->CurrentTask].IsIdle)
    {
        CPUState->BusyTSC += delta;
    }
    else
    {
        CPUState->IdleTSC += delta;
    }
}

STATIC BOOLEAN LBPWRRTaskOwnsStack(UINT64 index, UINT64 stack)
{
    if (index >= TaskCount)
    {
        return FALSE;
    }

    return stack >= Tasks[index].StackBase && stack < Tasks[index].StackBase + Tasks[index].StackSize;
}

STATIC LBPWRR_CPU *LBPWRRGetCurrentCPUState(VOID)
{
    UINT32 CPUID;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID == UINT32_MAX || CPUID >= SMP_MAX_CPUS)
    {
        return NULL;
    }

    return &CPUStates[CPUID];
}

STATIC VOID LBPWRRDumpTask(UINT64 index)
{
    INTERRUPT_FRAME *frame;

    if (index >= TaskCount)
    {
        return;
    }

    frame = (INTERRUPT_FRAME *) Tasks[index].RSP;

    DebugWrite(" task=");
    DebugWriteDec(index);
    DebugWrite(" cpu=");
    DebugWriteDec(Tasks[index].CPUID);
    DebugWrite(" state=");
    DebugWriteDec(Tasks[index].State);
    DebugWrite(" stack=");
    DebugWriteHex(Tasks[index].StackBase);
    DebugWrite("-");
    DebugWriteHex(Tasks[index].StackBase + Tasks[index].StackSize);
    DebugWrite(" rsp=");
    DebugWriteHex(Tasks[index].RSP);
    DebugWrite(" rip=");
    DebugWriteHex(frame == NULL ? 0 : frame->RIP);
    DebugWrite("\n");
}

STATIC VOID LBPWRRFatal(CONST CHAR16 *reason, INTERRUPT_FRAME *frame)
{
    LBPWRR_CPU *CPUState;
    UINT32 CPUID;

    CLI();
    TSCSetDeadline(0);

    CPUID = SMPGetCurrentCPUID();
    CPUState = LBPWRRGetCurrentCPUState();

    DebugWrite("PANIC: ");
    DebugWriteWide(reason);
    DebugWrite(" cpu=");
    DebugWriteDec(CPUID);
    DebugWrite(" frame=");
    DebugWriteHex((UINT64) frame);
    DebugWrite(" frame-rip=");
    DebugWriteHex(frame == NULL ? 0 : frame->RIP);
    DebugWrite(" current=");

    if (CPUState != NULL && CPUState->CurrentTaskValid)
    {
        DebugWriteDec(CPUState->CurrentTask);
    }
    else
    {
        DebugWrite("none");
    }

    DebugWrite(" task-count=");
    DebugWriteDec(TaskCount);
    DebugWrite("\n");

    for (UINT64 index = 0; index < TaskCount; index++)
    {
        LBPWRRDumpTask(index);
    }

    HLT();
}

STATIC VOID LBPWRRArmQuantum(VOID)
{
    UINT64 frequency;
    UINT64 ticks;

    frequency = TSCGetFrequency();

    if (frequency == 0)
    {
        return;
    }

    ticks = (UINT64) (((UINT128) frequency * LBPWRR_QUANTUM_MS) / 1000ULL);

    TSCSetDeadline(RDTSC() + ticks);
}

STATIC VOID LBPWRRIdleTask(VOID)
{
    while (TRUE)
    {
        HLTONCE();
    }
}

STATIC UINT64 LBPWRRNextReadyTask(UINT32 CPUID, UINT64 start)
{
    UINT64 offset;
    UINT64 index;

    for (offset = 1; offset <= TaskCount; offset++)
    {
        index = (start + offset) % TaskCount;

        if (Tasks[index].CPUID == CPUID &&
            Tasks[index].State == TASK_READY &&
            !Tasks[index].IsIdle)
        {
            return index;
        }
    }

    return UINT64_MAX;
}

STATIC UINT64 LBPWRRGetIdleTask(UINT32 CPUID)
{
    if (CPUID >= SMP_MAX_CPUS)
    {
        return UINT64_MAX;
    }

    if (IdleTasks[CPUID] == 0)
    {
        return UINT64_MAX;
    }

    return IdleTasks[CPUID];
}

STATIC BOOLEAN LBPWRRStackContains(UINT64 stack)
{
    for (UINT64 index = 0; index < TaskCount; index++)
    {
        if (LBPWRRTaskOwnsStack(index, stack))
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC VOID LBPWRRValidateFrame(UINT64 stack)
{
    INTERRUPT_FRAME *frame;

    if (!LBPWRRStackContains(stack))
    {
        LBPWRRFatal(L"LBPWRR bad stack", (INTERRUPT_FRAME *) stack);
    }

    frame = (INTERRUPT_FRAME *) stack;

    if (frame->RIP < LBPWRR_KERNEL_RIP_MIN || frame->RIP >= LBPWRR_KERNEL_RIP_MAX)
    {
        LBPWRRFatal(L"LBPWRR bad RIP", frame);
    }
}

STATIC UINT32 LBPWRRSelectNextCPU(VOID)
{
    UINT32 CPUCount;

    CPUCount = SMPGetCPUCount();

    if (CPUCount == 0)
    {
        return 0;
    }

    for (UINT32 offset = 0; offset < CPUCount; offset++)
    {
        UINT32 CPUID;
        CPU_INFO *CPU;

        CPUID = (NextCPU + offset) % CPUCount;
        CPU = SMPGetCPU(CPUID);

        if (CPU != NULL && CPU->Online)
        {
            NextCPU = (CPUID + 1) % CPUCount;
            return CPUID;
        }
    }

    return 0;
}

STATIC VOID LBPWRRTaskExit(VOID)
{
    LBPWRR_CPU *CPUState;

    CLI();

    CPUState = LBPWRRGetCurrentCPUState();

    if (CPUState != NULL && CPUState->CurrentTaskValid)
    {
        Tasks[CPUState->CurrentTask].State = TASK_DEAD;
    }

    Yield();
    HLT();
}

BOOLEAN LBPWRRInit(VOID)
{
    UINT32 BSPCPUID;

    KMemSet(Tasks, 0, sizeof(Tasks));
    KMemSet(CPUStates, 0, sizeof(CPUStates));
    KMemSet(IdleTasks, 0, sizeof(IdleTasks));

    TaskCount = 1;
    NextCPU = 0;
    SchedulerStarted = FALSE;

    BSPCPUID = SMPGetCurrentCPUID();

    if (BSPCPUID == UINT32_MAX || BSPCPUID >= SMP_MAX_CPUS)
    {
        return FALSE;
    }

    Tasks[0].State = TASK_RUNNING;
    Tasks[0].CPUID = BSPCPUID;

    CPUStates[BSPCPUID].CurrentTask = 0;
    CPUStates[BSPCPUID].CurrentTaskValid = TRUE;
    CPUStates[BSPCPUID].SwitchStack = 0;
    CPUStates[BSPCPUID].LastTSC = RDTSC();

    for (UINT32 CPUID = 0; CPUID < SMPGetCPUCount(); CPUID++)
    {
        if (!LBPWRRCreateTaskForCPU(LBPWRRIdleTask, CPUID, TRUE))
        {
            return FALSE;
        }

        IdleTasks[CPUID] = TaskCount - 1;
    }

    return TRUE;
}

STATIC BOOLEAN LBPWRRCreateTaskForCPU(VOID (*Entry)(VOID), UINT32 CPUID, BOOLEAN IsIdle)
{
    TASK *task;
    VOID *stack;
    UINT64 StackTop;
    UINT64 ReturnSlot;
    INTERRUPT_FRAME *frame;
    UINT16 selector;
    UINT16 StackSelector;

    if (Entry == NULL || CPUID >= SMPGetCPUCount() || TaskCount >= LBPWRR_MAX_TASKS || SchedulerStarted)
    {
        return FALSE;
    }

    stack = KAllocPages(LBPWRR_TASK_STACK_PAGES);

    if (stack == NULL)
    {
        return FALSE;
    }

    task = &Tasks[TaskCount];

    KMemSet(task, 0, sizeof(TASK));

    task->StackBase = (UINT64) stack;
    task->StackSize = LBPWRR_TASK_STACK_PAGES * LBPWRR_PAGE_SIZE;

    StackTop = task->StackBase + task->StackSize;
    StackTop &= ~0xFULL;

    ReturnSlot = StackTop - sizeof(UINT64);

    *((UINT64 *) ReturnSlot) = (UINT64) LBPWRRTaskExit;

    frame = (INTERRUPT_FRAME *) (ReturnSlot - sizeof(INTERRUPT_FRAME));

    KMemSet(frame, 0, sizeof(INTERRUPT_FRAME));

    ASM("mov %%cs, %0" : "=r"(selector));
    ASM("mov %%ss, %0" : "=r"(StackSelector));

    frame->RDI = 0;
    frame->Vector = 0;
    frame->ErrorCode = 0;
    frame->RIP = (UINT64) Entry;
    frame->CS = selector;
    frame->RFLAGS = LBPWRR_INITIAL_RFLAGS;
    frame->RSP = ReturnSlot;
    frame->SS = StackSelector;

    task->RSP = (UINT64) frame;
    task->InitialRSP = task->RSP;
    task->State = TASK_READY;
    task->CPUID = CPUID;
    task->IsIdle = IsIdle;

    DebugWrite("CREATE TASK=");
    DebugWriteDec(TaskCount);
    DebugWrite(" CPU=");
    DebugWriteDec(task->CPUID);
    DebugWrite(" RSP=");
    DebugWriteHex(task->RSP);
    DebugWrite(" STACK=");
    DebugWriteHex(task->StackBase);
    DebugWrite("\n");

    TaskCount++;

    return TRUE;
}

BOOLEAN LBPWRRCreateTask(VOID (*entry)(VOID))
{
    return LBPWRRCreateTaskForCPU(entry, LBPWRRSelectNextCPU(), FALSE);
}

VOID StartSchedule(VOID)
{
    LBPWRR_CPU *CPUState;

    CPUState = LBPWRRGetCurrentCPUState();

    if (CPUState == NULL)
    {
        return;
    }

    /*
     * BSP bootstrap context is not a schedulable task after startup.
     */
    Tasks[0].State = TASK_BLOCKED;

    CompilerBarrier();
    SchedulerStarted = TRUE;
    CompilerBarrier();

    LBPWRRArmQuantum();

    STI();
}

VOID APJoinSchedule(VOID)
{
    LBPWRR_CPU *CPUState;
    UINT32 CPUID;

    CLI();

    CPUID = SMPGetCurrentCPUID();

    if (CPUID == UINT32_MAX || CPUID >= SMP_MAX_CPUS)
    {
        HLT();
    }

    /*
     * TSC deadline LVT is per logical CPU.
     */
    TSCDeadlineInit(LBPWRR_TIMER_VECTOR);

    while (!SchedulerStarted)
    {
        PAUSE();
    }

    CompilerBarrier();

    CPUState = &CPUStates[CPUID];

    CPUState->CurrentTask = 0;
    CPUState->CurrentTaskValid = FALSE;
    CPUState->SwitchStack = 0;
    CPUState->LastTSC = RDTSC();

    LBPWRRArmQuantum();

    STI();

    /*
     * This AP has no bootstrap TASK object.
     * Until it receives a real task it simply idles here.
     */
    while (TRUE)
    {
        HLTONCE();
    }
}

VOID Yield(VOID)
{
    ASM("int %0" : : "i"(LBPWRR_YIELD_VECTOR));
}

VOID LBPWRRRecordIdleTSC(UINT64 StartTSC, UINT64 EndTSC)
{
    LBPWRR_CPU *CPUState;
    UINT32 CPUID;
    UINT64 BusyDelta;
    UINT64 IdleDelta;

    if (!SchedulerStarted || EndTSC <= StartTSC)
    {
        return;
    }

    CPUID = SMPGetCurrentCPUID();

    if (CPUID == UINT32_MAX || CPUID >= SMP_MAX_CPUS)
    {
        return;
    }

    CPUState = &CPUStates[CPUID];

    if (CPUState->LastTSC == 0 || StartTSC < CPUState->LastTSC)
    {
        CPUState->LastTSC = EndTSC;
        return;
    }

    BusyDelta = StartTSC - CPUState->LastTSC;
    IdleDelta = EndTSC - StartTSC;

    if (CPUState->CurrentTaskValid &&
        CPUState->CurrentTask < TaskCount &&
        !Tasks[CPUState->CurrentTask].IsIdle)
    {
        CPUState->BusyTSC += BusyDelta;
    }
    else
    {
        CPUState->IdleTSC += BusyDelta;
    }

    CPUState->IdleTSC += IdleDelta;
    CPUState->LastTSC = EndTSC;
}

VOID LBPWRRTick(INTERRUPT_FRAME *frame)
{
    LBPWRR_CPU *CPUState;
    UINT32 CPUID;
    UINT64 next;

    CPUID = SMPGetCurrentCPUID();

    if (CPUID == UINT32_MAX || CPUID >= SMP_MAX_CPUS)
    {
        LBPWRRFatal(L"LBPWRR invalid CPU", frame);
    }

    CPUState = &CPUStates[CPUID];

    LBPWRRUpdateCPUUsage(CPUState);

    CPUState->SwitchStack = (UINT64) frame;

    if (frame == NULL || TaskCount == 0 || !SchedulerStarted)
    {
        LBPWRRArmQuantum();
        return;
    }

    if (CPUState->CurrentTaskValid &&
        Tasks[CPUState->CurrentTask].StackBase != 0 &&
        !LBPWRRTaskOwnsStack(CPUState->CurrentTask, (UINT64) frame))
    {
        LBPWRRFatal(L"LBPWRR tick bad stack", frame);
    }

    if (CPUState->CurrentTaskValid)
    {
        Tasks[CPUState->CurrentTask].RSP = (UINT64) frame;

        if (Tasks[CPUState->CurrentTask].State == TASK_RUNNING)
        {
            Tasks[CPUState->CurrentTask].State = TASK_READY;
        }
    }

    if (CPUState->CurrentTaskValid)
    {
        next = LBPWRRNextReadyTask(CPUID, CPUState->CurrentTask);
    }
    else
    {
        next = LBPWRRNextReadyTask(CPUID, TaskCount - 1);
    }

    if (next == UINT64_MAX)
    {
        if (CPUState->CurrentTaskValid &&
            !Tasks[CPUState->CurrentTask].IsIdle &&
            Tasks[CPUState->CurrentTask].State != TASK_DEAD &&
            Tasks[CPUState->CurrentTask].State != TASK_BLOCKED)
        {
            Tasks[CPUState->CurrentTask].State = TASK_RUNNING;
            CPUState->SwitchStack = Tasks[CPUState->CurrentTask].RSP;
            LBPWRRArmQuantum();

            return;
        }

        next = LBPWRRGetIdleTask(CPUID);

        if (next != UINT64_MAX)
        {
            CPUState->CurrentTask = next;
            CPUState->CurrentTaskValid = TRUE;
            Tasks[next].State = TASK_RUNNING;
            CPUState->SwitchStack = Tasks[next].RSP;
            LBPWRRValidateFrame(CPUState->SwitchStack);
        }
        else
        {
            /*
             * No task assigned to this CPU.
             * Return to the APJoinSchedule/BSP interrupted context.
             */
            CPUState->CurrentTaskValid = FALSE;
            CPUState->SwitchStack = (UINT64) frame;
        }

        LBPWRRArmQuantum();

        return;
    }

    CPUState->CurrentTask = next;
    CPUState->CurrentTaskValid = TRUE;

    Tasks[next].State = TASK_RUNNING;

    CPUState->SwitchStack = Tasks[next].RSP;

    LBPWRRValidateFrame(CPUState->SwitchStack);

    LBPWRRArmQuantum();
}

UINT64 LBPWRRGetSwitchStack(INTERRUPT_FRAME *frame)
{
    LBPWRR_CPU *CPUState;

    CPUState = LBPWRRGetCurrentCPUState();

    if (CPUState == NULL || CPUState->SwitchStack == 0)
    {
        return (UINT64) frame;
    }

    return CPUState->SwitchStack;
}

UINT64 LBPWRRGetCPUAssignedTaskCount(UINT32 CPUID)
{
    UINT64 count;

    count = 0;

    for (UINT64 index = 0; index < TaskCount; index++)
    {
        if (Tasks[index].CPUID == CPUID &&
            Tasks[index].State != TASK_DEAD &&
            !Tasks[index].IsIdle)
        {
            count++;
        }
    }

    return count;
}

UINTN LBPWRRGetCPUUsage(UINT32 CPUID)
{
    LBPWRR_CPU *CPUState;
    UINT64 BusyTSC;
    UINT64 IdleTSC;
    UINT64 RunningDelta;
    UINT64 BusyDelta;
    UINT64 IdleDelta;
    UINT64 TotalTSC;
    UINT64 now;

    if (CPUID >= SMPGetCPUCount())
    {
        return 0;
    }

    CPUState = &CPUStates[CPUID];

    now = RDTSC();
    BusyTSC = CPUState->BusyTSC;
    IdleTSC = CPUState->IdleTSC;

    if (CPUState->LastTSC != 0)
    {
        RunningDelta = now - CPUState->LastTSC;

        if (CPUState->CurrentTaskValid)
        {
            if (CPUState->CurrentTask < TaskCount && !Tasks[CPUState->CurrentTask].IsIdle)
            {
                BusyTSC += RunningDelta;
            }
            else
            {
                IdleTSC += RunningDelta;
            }
        }
        else
        {
            IdleTSC += RunningDelta;
        }
    }

    BusyDelta = BusyTSC - CPUState->LastSampleBusyTSC;
    IdleDelta = IdleTSC - CPUState->LastSampleIdleTSC;

    CPUState->LastSampleBusyTSC = BusyTSC;
    CPUState->LastSampleIdleTSC = IdleTSC;

    TotalTSC = BusyDelta + IdleDelta;

    if (TotalTSC == 0)
    {
        return CPUState->CPUUsage;
    }

    CPUState->CPUUsage = (UINTN) (((UINT128) BusyDelta * 100ULL) / TotalTSC);

    return CPUState->CPUUsage;
}
