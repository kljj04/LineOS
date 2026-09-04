// kernel/src/scheduler/prr.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/typeinfo.h>
#include <scheduler/prr.h>

#include <render/truetype/print.h>

#include <scheduler/prr_types.h>
#include <memory/memory.h>
#include <timer/tsc.h>
#include <arch/x86_64/cpu.h>
#include <debug/panic.h>
#include <debug/debug.h>

#define PRR_MAX_TASKS        256
#define PRR_TASK_STACK_PAGES 16
#define PRR_PAGE_SIZE        4096
#define PRR_QUANTUM_MS       5
#define PRR_YIELD_VECTOR     0x43
#define PRR_INITIAL_RFLAGS   0x202ULL
#define PRR_KERNEL_RIP_MIN   0x400000ULL
#define PRR_KERNEL_RIP_MAX   0x80000000ULL

STATIC TASK    Tasks[PRR_MAX_TASKS];
STATIC UINT64  TaskCount = 0;
STATIC UINT64  CurrentTask = 0;
STATIC BOOLEAN SchedulerStarted = FALSE;
STATIC BOOLEAN CurrentTaskValid = FALSE;
STATIC UINT64  SwitchStack = 0;

EXTERN VOID PRRRestoreContext(UINT64 stack);

STATIC BOOLEAN PRRTaskOwnsStack(UINT64 index, UINT64 stack)
{
    if (index >= TaskCount)
    {
        return FALSE;
    }

    return stack >= Tasks[index].StackBase && stack < Tasks[index].StackBase + Tasks[index].StackSize;
}

STATIC VOID PRRDumpTask(UINT64 index)
{
    INTERRUPT_FRAME *frame;

    if (index >= TaskCount)
    {
        return;
    }

    frame = (INTERRUPT_FRAME *) Tasks[index].RSP;

    DebugWrite(" task=");
    DebugWriteDec(index);
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

STATIC VOID PRRFatal(CONST CHAR16 *reason, INTERRUPT_FRAME *frame)
{
    CLI();
    TSCSetDeadline(0);

    DebugWrite("PANIC: ");
    DebugWriteWide(reason);
    DebugWrite(" frame=");
    DebugWriteHex((UINT64) frame);
    DebugWrite(" frame-rip=");
    DebugWriteHex(frame == NULL ? 0 : frame->RIP);
    DebugWrite(" current=");
    DebugWriteDec(CurrentTask);
    DebugWrite(" task-count=");
    DebugWriteDec(TaskCount);
    DebugWrite("\n");

    for (UINT64 index = 0; index < TaskCount; index++)
    {
        PRRDumpTask(index);
    }

    HLT();
}

STATIC VOID PRRArmQuantum(VOID)
{
    UINT64 frequency;
    UINT64 ticks;

    frequency = TSCGetFrequency();
    if (frequency == 0)
    {
        return;
    }

    ticks = (UINT64) (((UINT128) frequency * PRR_QUANTUM_MS) / 1000ULL);
    TSCSetDeadline(RDTSC() + ticks);
}

STATIC UINT64 PRRNextReadyTask(UINT64 start)
{
    UINT64 offset;
    UINT64 index;

    for (offset = 1; offset <= TaskCount; offset++)
    {
        index = (start + offset) % TaskCount;
        if (Tasks[index].State == TASK_READY)
        {
            return index;
        }
    }

    return UINT64_MAX;
}

STATIC BOOLEAN PRRStackContains(UINT64 stack)
{
    for (UINT64 index = 0; index < TaskCount; index++)
    {
        if (PRRTaskOwnsStack(index, stack))
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC VOID PRRValidateFrame(UINT64 stack)
{
    INTERRUPT_FRAME *frame;

    if (!PRRStackContains(stack))
    {
        PRRFatal(L"PRR bad stack", (INTERRUPT_FRAME *) stack);
    }

    frame = (INTERRUPT_FRAME *) stack;
    if (frame->RIP < PRR_KERNEL_RIP_MIN || frame->RIP >= PRR_KERNEL_RIP_MAX)
    {
        PRRFatal(L"PRR bad RIP", frame);
    }
}

STATIC VOID PRRTaskExit(VOID)
{
    CLI();

    if (CurrentTaskValid)
    {
        Tasks[CurrentTask].State = TASK_DEAD;
    }

    Yield();
    HLT();
}

BOOLEAN PRRInit(VOID)
{
    KMemSet(Tasks, 0, sizeof(Tasks));

    TaskCount = 1;
    CurrentTask = 0;
    CurrentTaskValid = TRUE;
    SchedulerStarted = FALSE;
    SwitchStack = 0;

    Tasks[0].State = TASK_RUNNING;

    return TRUE;
}

BOOLEAN PRRCreateTask(VOID (*entry)(VOID))
{
    TASK            *task;
    VOID            *stack;
    UINT64           StackTop;
    UINT64           ReturnSlot;
    INTERRUPT_FRAME *frame;
    UINT16           selector;
    UINT16           StackSelector;

    if (entry == NULL || TaskCount >= PRR_MAX_TASKS)
    {
        return FALSE;
    }

    stack = KAllocPages(PRR_TASK_STACK_PAGES);

    if (stack == NULL)
    {
        return FALSE;
    }

    task = &Tasks[TaskCount];
    KMemSet(task, 0, sizeof(TASK));

    task->StackBase = (UINT64) stack;
    task->StackSize = PRR_TASK_STACK_PAGES * PRR_PAGE_SIZE;

    StackTop = task->StackBase + task->StackSize;
    StackTop &= ~0xFULL;
    ReturnSlot = StackTop - sizeof(UINT64);
    *((UINT64 *) ReturnSlot) = (UINT64) PRRTaskExit;

    frame = (INTERRUPT_FRAME *) (ReturnSlot - sizeof(INTERRUPT_FRAME));
    KMemSet(frame, 0, sizeof(INTERRUPT_FRAME));

    ASM("mov %%cs, %0" : "=r"(selector));
    ASM("mov %%ss, %0" : "=r"(StackSelector));

    frame->RDI = 0;
    frame->Vector = 0;
    frame->ErrorCode = 0;
    frame->RIP = (UINT64) entry;
    frame->CS = selector;
    frame->RFLAGS = PRR_INITIAL_RFLAGS;
    frame->RSP = ReturnSlot;
    frame->SS = StackSelector;

    task->RSP = (UINT64) frame;
    task->InitialRSP = task->RSP;
    task->State = TASK_READY;

    TaskCount++;

    DebugWrite("CREATE TASK=");
    DebugWriteHex(TaskCount);
    DebugWrite(" RSP=");
    DebugWriteHex(task->RSP);
    DebugWrite(" STACK=");
    DebugWriteHex(task->StackBase);
    DebugWrite("\n");

    return TRUE;
}

VOID StartSchedule(VOID)
{
    DebugWrite("PRESTART TASK1 RSP=");
    DebugWriteHex(Tasks[1].RSP);
    DebugWrite("\n");

    Tasks[0].State = TASK_BLOCKED;
    SchedulerStarted = TRUE;
    PRRArmQuantum();
    STI();
}

VOLATILE UINT64 PRRDebugRSPAfterSwitch = 0;
VOLATILE UINT64 PRRDebugRSPBeforeIRET = 0;
VOLATILE UINT64 PRRDebugTaskEntryRSP = 0;
VOID Yield(VOID)
{
    ASM("int %0" : : "i"(PRR_YIELD_VECTOR));
}

VOID PRRTick(INTERRUPT_FRAME *frame)
{
    DebugWrite("TICK ENTRY TASK1 RSP=");
    DebugWriteHex(Tasks[1].RSP);
    DebugWrite("\n");

    UINT64 next;

    SwitchStack = (UINT64) frame;

    if (frame == NULL || TaskCount == 0 || !SchedulerStarted)
    {
        PRRArmQuantum();
        return;
    }

    if (CurrentTaskValid && Tasks[CurrentTask].StackBase != 0 && !PRRTaskOwnsStack(CurrentTask, (UINT64) frame))
    {
        PRRFatal(L"PRR tick bad stack", frame);
    }

    if (CurrentTaskValid)
    {
        DebugWrite("SAVE TASK=");
        DebugWriteHex(CurrentTask);
        DebugWrite(" FRAME=");
        DebugWriteHex((UINT64) frame);
        DebugWrite(" OLD RSP=");
        DebugWriteHex(Tasks[CurrentTask].RSP);
        DebugWrite("\n");

        Tasks[CurrentTask].RSP = (UINT64) frame;

        DebugWrite(" NEW RSP=");
        DebugWriteHex(Tasks[CurrentTask].RSP);
        DebugWrite("\n");
        if (Tasks[CurrentTask].State == TASK_RUNNING)
        {
            Tasks[CurrentTask].State = TASK_READY;
        }
    }

    next = CurrentTaskValid ? PRRNextReadyTask(CurrentTask) : PRRNextReadyTask(TaskCount - 1);
    if (next == UINT64_MAX)
    {
        if (CurrentTaskValid && Tasks[CurrentTask].State != TASK_DEAD)
        {
            Tasks[CurrentTask].State = TASK_RUNNING;
            SwitchStack = Tasks[CurrentTask].RSP;
        }
        else
        {
            PRRFatal(L"PRR no runnable task", frame);
        }

        PRRArmQuantum();
        return;
    }

    CurrentTask = next;
    CurrentTaskValid = TRUE;
    Tasks[CurrentTask].State = TASK_RUNNING;
    SwitchStack = Tasks[CurrentTask].RSP;
    PRRValidateFrame(SwitchStack);

    PRRArmQuantum();
}

UINT64 PRRGetSwitchStack(INTERRUPT_FRAME *frame)
{
    if (SwitchStack == 0)
    {
        return (UINT64) frame;
    }

    return SwitchStack;
}
