// Kernel/Source/Syscall/Process/SyscallProcess.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Process/SyscallProcess.h>
#include <Syscall/IPC/SyscallIPC.h>
#include <Syscall/Process/SyscallThread.h>
#include <Syscall/Process/ProcessInternal.h>
#include <Syscall/SyscallError.h>
#include <Syscall/Memory/SyscallMemory.h>
#include <Syscall/File/SyscallFile.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Scheduler/WaitQueue.h>
#include <Exec/ELF64.h>
#include <Memory/PMM.h>
#include <Debug/Debug.h>
#include <Debug/Panic.h>
#include <Arch/AMD64/CPU.h>

#define IA32_FS_BASE 0xC0000100U

STATIC_ASSERT(sizeof(TASK_CONTEXT) == sizeof(INTERRUPT_FRAME), "fork context size");
STATIC_ASSERT(__builtin_offsetof(TASK_CONTEXT, RIP) == __builtin_offsetof(INTERRUPT_FRAME, RIP) && __builtin_offsetof(TASK_CONTEXT, RAX) == __builtin_offsetof(INTERRUPT_FRAME, RAX) && __builtin_offsetof(TASK_CONTEXT, RSP) == __builtin_offsetof(INTERRUPT_FRAME, RSP), "fork context offsets");

#define PROCESS_LIMIT           128U
#define PROCESS_EXECUTION_PAGES ((sizeof(ELF64_EXECUTION) + 4095U) / 4096U)

typedef struct
{
    ELF64_EXECUTION *Execution;
    UINT16           ParentPID;
    BOOLEAN          Exited;
    BOOLEAN          Waited;
    INT32            Status;
} PROCESS_SLOT;

STATIC PROCESS_SLOT ProcessSlots[PROCESS_LIMIT];
STATIC UINT32       ProcessBusy;
STATIC SPIN_LOCK    ProcessLock;
STATIC WAIT_QUEUE   ProcessWaiters;
STATIC WAIT_QUEUE   ProcessChildWaiters;

BOOLEAN ProcessBegin(VOID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

    while (ProcessBusy != 0)
    {
        if (!WaitQueueSleepUninterruptible(&ProcessWaiters, &ProcessLock))
        {
            SpinLockReleaseIRQRestore(&ProcessLock, Flags);

            return FALSE;
        }
    }

    ProcessBusy = 1;
    SpinLockReleaseIRQRestore(&ProcessLock, Flags);

    return TRUE;
}

INT64 ProcessEnd(INT64 Result)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

    ProcessBusy = 0;
    WaitQueueWakeAll(&ProcessWaiters);
    SpinLockReleaseIRQRestore(&ProcessLock, Flags);

    return Result;
}

VOID ProcessReapThreads(PROCESS *Process)
{
    TASK *Representative = Process->Execution->Task;
    TASK *Task           = Process->ThreadHead;

    while (Task != NULL)
    {
        TASK *Next = Task->ThreadNext;

        if (Task != Representative)
        {
            TaskDestroy(Task);
        }

        Task = Next;
    }
}

STATIC PROCESS_SLOT *ProcessReserve(VOID)
{
    for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
    {
        PROCESS_SLOT    *Slot      = &ProcessSlots[Index];
        ELF64_EXECUTION *Execution = Slot->Execution;
        TASK            *Parent    = Slot->ParentPID != 0 ? TaskGetByPID(Slot->ParentPID) : NULL;
        BOOLEAN          Retained  = !Slot->Waited && Parent != NULL && !Parent->Process->Exited;

        if (Execution != NULL && Execution->Task != NULL)
        {
            ProcessReapThreads(Execution->Task->Process);
        }

        if (!Retained && Execution != NULL && Execution->Task != NULL && ATOMIC_LOAD(&Execution->Task->Process->ThreadCount, ATOMIC_ACQUIRE) == 1 && (!Execution->Task->Scheduled || LBPWRRRemoveTask(Execution->Task)) && ELF64Release(Execution))
        {
            KMemFreePages(Execution, PROCESS_EXECUTION_PAGES);
            KMemSet(Slot, 0, sizeof(*Slot));
        }
    }

    for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
    {
        if (ProcessSlots[Index].Execution == NULL)
        {
            return &ProcessSlots[Index];
        }
    }

    return NULL;
}

STATIC INT64 ProcessCopyArguments(TASK *Task, UINT64 Vector, ELF64_ARGUMENTS *Arguments)
{
    KMemSet(Arguments, 0, sizeof(*Arguments));

    if (Vector == 0)
    {
        return 0;
    }

    for (UINT32 Index = 0; Index <= ELF64_ARG_MAX_COUNT; Index++)
    {
        UINT64 Pointer;
        UINT64 Offset = (UINT64) Index * sizeof(Pointer);

        if (Vector > UINT64_MAX - Offset || !VMMCopyFromUser(Task->Process->AddressSpace, &Pointer, Vector + Offset, sizeof(Pointer)))
        {
            return -EFAULT;
        }

        if (Pointer == 0)
        {
            return 0;
        }

        if (Index == ELF64_ARG_MAX_COUNT)
        {
            return -E2BIG;
        }

        Arguments->Offsets[Index] = Arguments->Bytes;

        BOOLEAN Terminated = FALSE;

        for (UINT32 Position = 0; Position < ELF64_ARG_MAX_LENGTH; Position++)
        {
            if (Arguments->Bytes == ELF64_ARG_MAX_BYTES)
            {
                return -E2BIG;
            }

            if (Pointer > UINT64_MAX - Position || !VMMCopyFromUser(Task->Process->AddressSpace, &Arguments->Data[Arguments->Bytes], Pointer + Position, 1))
            {
                return -EFAULT;
            }

            if (Arguments->Data[Arguments->Bytes++] == 0)
            {
                Terminated = TRUE;
                break;
            }
        }

        if (!Terminated)
        {
            return -E2BIG;
        }

        Arguments->Count++;
    }

    return -E2BIG;
}

STATIC VOID ForkUnusedEntry(VOID) {}

INT64 SyscallFork(INTERRUPT_FRAME *Frame)
{
    TASK  *Parent = LBPWRRGetCurrentTask();
    UINT64 Count  = 0;
    INT64  Result = -ENOMEM;

    if (!ProcessBegin())
    {
        return -EAGAIN;
    }

    if (Parent == NULL || Parent->Process->AddressSpace == NULL || Parent->Process->FileContext == NULL || Parent->Process->Execution == NULL)
    {
        return ProcessEnd(-ESRCH);
    }

    if (ATOMIC_LOAD(&Parent->Process->GroupExiting, ATOMIC_ACQUIRE))
    {
        return ProcessEnd(-EINTR);
    }

    PROCESS_SLOT *Slot = ProcessReserve();

    if (Slot == NULL)
    {
        return ProcessEnd(-EAGAIN);
    }

    if (!VMMListUserPages(Parent->Process->AddressSpace, NULL, 0, &Count) || Count > ELF64_EXEC_MAX_PAGES)
    {
        return ProcessEnd(-EAGAIN);
    }

    VMM_USER_PAGE   *Pages     = KAlloc(Count * sizeof(*Pages));
    ELF64_EXECUTION *Execution = KAllocPages(PROCESS_EXECUTION_PAGES);

    if (Pages == NULL || Execution == NULL)
    {
        if (Pages != NULL)
            KFree(Pages);

        if (Execution != NULL)
            KMemFreePages(Execution, PROCESS_EXECUTION_PAGES);

        return ProcessEnd(-ENOMEM);
    }

    if (!VMMListUserPages(Parent->Process->AddressSpace, Pages, Count, &Count) || !VMMCreateAddressSpace(&Execution->AddressSpace))
    {
        goto Failed;
    }

    TASK *Child = Execution->Task = TaskCreate(ForkUnusedEntry);

    if (Child == NULL)
    {
        goto Failed;
    }

    Child->Process->AddressSpace = Execution->AddressSpace;
    Child->Process->Execution    = Execution;

    for (UINT64 Index = 0; Index < Count; Index++)
    {
        UINT64 Physical = Pages[Index].Physical;

        if (!SyscallMemoryShared(Parent, Pages[Index].Address))
        {
            VOID *Copy = KAllocPages(1);

            if (Copy == NULL)
            {
                goto Failed;
            }

            KMemCpy(Copy, (CONST VOID *) Physical, 4096);
            Physical                                 = (UINT64) Copy;
            Execution->Pages[Execution->PageCount++] = Copy;
        }

        if (!VMMMapUserPages(Child->Process->AddressSpace, Pages[Index].Address, &Physical, 1, Pages[Index].Flags))
        {
            goto Failed;
        }
    }

    Child->Process->FileContext = KAlloc(sizeof(VFS_CONTEXT));

    if (Child->Process->FileContext == NULL)
    {
        goto Failed;
    }

    *Child->Process->FileContext = *Parent->Process->FileContext;
    Child->Process->RealUID      = Parent->Process->RealUID;
    Child->Process->RealGID      = Parent->Process->RealGID;
    Child->Process->AuthSession  = Parent->Process->AuthSession;
    KMemCpy(Child->Process->Name, Parent->Process->Name, sizeof(Child->Process->Name));
    ASM("fxsave64 %0" : "=m"(Child->FPState)::"memory");
    Child->FSBase        = ReadMSR(IA32_FS_BASE);
    Child->UserStackBase = Parent->UserStackBase;
    Child->UserStackSize = Parent->UserStackSize;
    KMemCpy(Child->Process->Assets, Parent->Process->Assets, sizeof(Child->Process->Assets));
    KMemCpy(&Child->Context, Frame, sizeof(Child->Context));
    Child->Context.RAX = 0;
    KMemCpy((VOID *) Child->RSP, &Child->Context, sizeof(Child->Context));

    if (!SyscallMemoryClone(Child, Parent) || !SyscallFilesInherit(Child, Parent))
    {
        goto Failed;
    }

    Slot->Execution = Execution;
    Slot->ParentPID = Parent->Process->PID;
    Slot->Exited    = FALSE;
    Slot->Waited    = FALSE;
    Slot->Status    = 0;
    Result          = Child->Process->PID;

    if (!LBPWRRAddTask(Child))
    {
        Slot->Execution = NULL;
        Result          = -EAGAIN;

        goto Failed;
    }

    KFree(Pages);

    return ProcessEnd(Result);
Failed:
    if (Execution->Task != NULL)
    {
        TaskKill(Execution->Task);
    }

    if (!ELF64Release(Execution))
    {
        Panic(L"fork cleanup failed");
    }

    KMemFreePages(Execution, PROCESS_EXECUTION_PAGES);
    KFree(Pages);

    return ProcessEnd(Result);
}

INT64 SyscallExecVE(INTERRUPT_FRAME *Frame)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;

    if (!ProcessBegin())
    {
        return -EAGAIN;
    }

    if (Task == NULL || Task->Process->Execution == NULL || Task->Process->FileContext == NULL)
    {
        return ProcessEnd(-ESRCH);
    }

    ProcessReapThreads(Task->Process);

    if (ATOMIC_LOAD(&Task->Process->ThreadCount, ATOMIC_ACQUIRE) != 1 || ATOMIC_LOAD(&Task->Process->GroupExiting, ATOMIC_ACQUIRE))
    {
        return ProcessEnd(-EBUSY);
    }

    INT64 Result = SyscallCopyPath(Task, Frame->RDI, &Name);

    if (Result < 0)
    {
        return ProcessEnd(Result);
    }

    ELF64_ARGUMENTS *Arguments = KAlloc(sizeof(*Arguments) * 2);
    ELF64_EXECUTION *New       = KAllocPages(PROCESS_EXECUTION_PAGES);

    if (Arguments == NULL || New == NULL)
    {
        KFree(Name);

        if (Arguments != NULL)
            KFree(Arguments);

        if (New != NULL)
            KMemFreePages(New, PROCESS_EXECUTION_PAGES);

        return ProcessEnd(-ENOMEM);
    }

    Result = ProcessCopyArguments(Task, Frame->RSI, Arguments);

    if (Result == 0)
    {
        Result = ProcessCopyArguments(Task, Frame->RDX, Arguments + 1);
    }

    if (Result == 0 && Arguments[0].Bytes + Arguments[1].Bytes > ELF64_ARG_MAX_BYTES)
    {
        Result = -E2BIG;
    }

    if (Result == 0 && !ELF64PrepareArguments(Task->Process->FileContext, Name, Arguments, New))
    {
        Result = New->Error != 0 ? New->Error : -ENOEXEC;
    }

    if (Result == 0 && !ELF64SetEnvironment(New, Arguments + 1))
    {
        Result = -E2BIG;
    }

    if (Result == 0)
    {
        ELF64_EXECUTION   *Owner    = Task->Process->Execution;
        VMM_ADDRESS_SPACE *OldSpace = Task->Process->AddressSpace;

        if (!VMMSwitchAddressSpace(New->AddressSpace))
        {
            Result = -EBUSY;
        }
        else if (!VMMDestroyAddressSpace(OldSpace))
        {
            if (!VMMSwitchAddressSpace(OldSpace))
                Panic(L"execve rollback failed");
            Result = -EBUSY;
        }
        else
        {
            SyscallIPCRelease(Task);
            SyscallMemoryRelease(Task);

            while (Owner->PageCount != 0)
            {
                KMemFreePages(Owner->Pages[--Owner->PageCount], 1);
            }

            Owner->AddressSpace = New->AddressSpace;
            Owner->PageCount    = New->PageCount;
            KMemCpy(Owner->Pages, New->Pages, New->PageCount * sizeof(VOID *));
            Task->Process->FileContext->Credentials = New->Task->Process->FileContext->Credentials;
            Task->Process->AddressSpace             = New->AddressSpace;
            Task->UserStackBase                     = New->Task->UserStackBase;
            Task->UserStackSize                     = New->Task->UserStackSize;
            KMemCpy(Task->Process->Name, New->Task->Process->Name, sizeof(Task->Process->Name));
            WriteMSR(IA32_FS_BASE, 0);
            ASM("fxrstor64 %0" ::"m"(New->Task->FPState) : "memory");
            Task->ClearTIDAddress = 0;
            Task->RobustListHead  = 0;
            Task->Context         = New->Task->Context;
            KMemCpy(Frame, &Task->Context, sizeof(*Frame));
            KMemSet(Task->Process->Assets, 0, sizeof(Task->Process->Assets));
            New->AddressSpace                = NULL;
            New->PageCount                   = 0;
            New->Task->Process->AddressSpace = NULL;
        }
    }

    if (New->Task != NULL)
    {
        TaskKill(New->Task);
    }

    if (!ELF64Release(New))
    {
        Panic(L"execve cleanup failed");
    }

    KMemFreePages(New, PROCESS_EXECUTION_PAGES);
    KFree(Arguments);
    KFree(Name);

    return ProcessEnd(Result);
}

INT64 SyscallProcessExit(TASK *Task, UINT64 Code, BOOLEAN Group)
{
    if (Task == NULL || Task->Idle || !ProcessBegin())
    {
        return -ESRCH;
    }

    PROCESS *Process   = Task->Process;
    BOOLEAN  OtherLive = FALSE;

    for (TASK *Thread = Process->ThreadHead; Thread != NULL; Thread = Thread->ThreadNext)
    {
        if (Thread != Task && Thread->State != TASK_TERMINATED && Thread->State != TASK_KILLED)
        {
            OtherLive = TRUE;
            break;
        }
    }

    if (Group && !ATOMIC_LOAD(&Process->GroupExiting, ATOMIC_ACQUIRE))
    {
        Process->GroupExitCode = Code;
        ATOMIC_STORE(&Process->GroupExiting, TRUE, ATOMIC_RELEASE);

        for (TASK *Thread = Process->ThreadHead; Thread != NULL; Thread = Thread->ThreadNext)
        {
            if (Thread != Task)
            {
                WaitQueueInterruptTask(Thread);
            }
        }
    }

    if (ATOMIC_LOAD(&Process->GroupExiting, ATOMIC_ACQUIRE))
    {
        Code = Process->GroupExitCode;
    }

    SyscallReleaseRobustList(Task);
    SyscallClearTIDAddress(Task);
    SyscallIPCRelease(Task);

    if (!OtherLive)
    {
        SyscallFilesRelease(Task);
        Process->Exited = TRUE;

        for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
        {
            PROCESS_SLOT *Slot = &ProcessSlots[Index];

            if (Slot->Execution != NULL && Slot->Execution == Process->Execution)
            {
                Slot->Status = (INT32) ((Code & 255) << 8);
                Slot->Exited = TRUE;
                break;
            }
        }

        UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

        WaitQueueWakeAll(&ProcessChildWaiters);
        SpinLockReleaseIRQRestore(&ProcessLock, Flags);
    }

    TaskTerminate(Task);
    ProcessEnd(0);
    LBPWRRYield();

    return -EIO;
}

INT64 SyscallWait4(INT64 PID, UINT64 Status, UINT64 Options, UINT64 Usage)
{
    TASK *Parent = LBPWRRGetCurrentTask();

    if (Parent == NULL || Parent->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if ((Options & ~1ULL) != 0)
    {
        return -EINVAL;
    }

    if (Usage != 0 || PID == 0 || PID < -1)
    {
        return -EOPNOTSUPP;
    }

    for (;;)
    {
        if (!ProcessBegin())
        {
            return -EINTR;
        }

        BOOLEAN Found = FALSE;

        for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
        {
            PROCESS_SLOT *Slot = &ProcessSlots[Index];

            if (Slot->Execution == NULL || Slot->Execution->Task == NULL || Slot->ParentPID != Parent->Process->PID || Slot->Waited)
            {
                continue;
            }

            TASK *Child = Slot->Execution->Task;

            if (PID != -1 && PID != Child->Process->PID)
            {
                continue;
            }

            Found = TRUE;

            if (Slot->Exited || (Child->State == TASK_KILLED && ATOMIC_LOAD(&Child->Process->ThreadCount, ATOMIC_ACQUIRE) == 1))
            {
                INT32 Code = Slot->Exited ? Slot->Status : Child->State == TASK_KILLED ? 9 : 0;

                if (Status != 0 && !VMMCopyToUser(Parent->Process->AddressSpace, Status, &Code, sizeof(Code)))
                {
                    return ProcessEnd(-EFAULT);
                }

                Slot->Waited = TRUE;

                return ProcessEnd(Child->Process->PID);
            }
        }

        if (!Found)
        {
            return ProcessEnd(-ECHILD);
        }

        if ((Options & 1) != 0)
        {
            return ProcessEnd(0);
        }

        UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

        ProcessBusy = 0;
        WaitQueueWakeAll(&ProcessWaiters);

        BOOLEAN Slept = WaitQueueSleep(&ProcessChildWaiters, &ProcessLock);

        SpinLockReleaseIRQRestore(&ProcessLock, Flags);

        if (!Slept)
        {
            return -EINTR;
        }
    }
}

INT64 ProcessChangeParentDirectory(TASK *Task, CONST CHAR8 *Name)
{
    if (!ProcessBegin())
    {
        return -EAGAIN;
    }

    INT64 Result = -ESRCH;

    for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
    {
        PROCESS_SLOT *Slot = &ProcessSlots[Index];

        if (Slot->Execution == NULL || Slot->Execution->Task != Task)
        {
            continue;
        }

        TASK *Parent = TaskGetByPID(Slot->ParentPID);

        if (Parent == NULL || Parent->Process->FileContext == NULL || Parent->Process->RealUID != Task->Process->RealUID)
        {
            break;
        }

        VFS_CONTEXT Context = *Parent->Process->FileContext;

        Context.Credentials = Task->Process->FileContext->Credentials;
        Result              = SyscallVFSError(VFSChangeDirectory(&Context, Name));

        if (Result == 0)
        {
            Parent->Process->FileContext->Partition = Context.Partition;
            KMemCpy(Parent->Process->FileContext->Cwd, Context.Cwd, sizeof(Context.Cwd));
        }

        break;
    }

    return ProcessEnd(Result);
}
