// Kernel/Source/Syscall/Process/SyscallProcess.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Process/SyscallProcess.h>
#include <Syscall/IPC/SyscallIPC.h>
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

STATIC BOOLEAN ProcessBegin(VOID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

    while (ProcessBusy != 0)
    {
        if (!WaitQueueSleep(&ProcessWaiters, &ProcessLock))
        {
            SpinLockReleaseIRQRestore(&ProcessLock, Flags);

            return FALSE;
        }
    }

    ProcessBusy = 1;
    SpinLockReleaseIRQRestore(&ProcessLock, Flags);

    return TRUE;
}

STATIC INT64 ProcessEnd(INT64 Result)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

    ProcessBusy = 0;
    WaitQueueWakeAll(&ProcessWaiters);
    SpinLockReleaseIRQRestore(&ProcessLock, Flags);

    return Result;
}

STATIC PROCESS_SLOT *ProcessReserve(VOID)
{
    for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
    {
        PROCESS_SLOT    *Slot = &ProcessSlots[Index];
        ELF64_EXECUTION *Execution = Slot->Execution;
        TASK            *Parent = Slot->ParentPID != 0 ? TaskGetByPID(Slot->ParentPID) : NULL;
        BOOLEAN          Retained = !Slot->Waited && Parent != NULL && Parent->State != TASK_TERMINATED && Parent->State != TASK_KILLED;

        if (!Retained && Execution != NULL && Execution->Task != NULL && (!Execution->Task->Scheduled || LBPWRRRemoveTask(Execution->Task)) && ELF64Release(Execution))
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

        if (Vector > UINT64_MAX - Offset || !VMMCopyFromUser(Task->AddressSpace, &Pointer, Vector + Offset, sizeof(Pointer)))
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

            if (Pointer > UINT64_MAX - Position || !VMMCopyFromUser(Task->AddressSpace, &Arguments->Data[Arguments->Bytes], Pointer + Position, 1))
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
    UINT64 Count = 0;
    INT64  Result = -ENOMEM;

    if (!ProcessBegin())
    {
        return -EAGAIN;
    }

    if (Parent == NULL || Parent->AddressSpace == NULL || Parent->FileContext == NULL || Parent->Execution == NULL)
    {
        return ProcessEnd(-ESRCH);
    }

    PROCESS_SLOT *Slot = ProcessReserve();

    if (Slot == NULL)
    {
        return ProcessEnd(-EAGAIN);
    }

    if (!VMMListUserPages(Parent->AddressSpace, NULL, 0, &Count) || Count > ELF64_EXEC_MAX_PAGES)
    {
        return ProcessEnd(-EAGAIN);
    }

    VMM_USER_PAGE   *Pages = KAlloc(Count * sizeof(*Pages));
    ELF64_EXECUTION *Execution = KAllocPages(PROCESS_EXECUTION_PAGES);

    if (Pages == NULL || Execution == NULL)
    {
        if (Pages != NULL)
            KFree(Pages);

        if (Execution != NULL)
            KMemFreePages(Execution, PROCESS_EXECUTION_PAGES);

        return ProcessEnd(-ENOMEM);
    }

    if (!VMMListUserPages(Parent->AddressSpace, Pages, Count, &Count) || !VMMCreateAddressSpace(&Execution->AddressSpace))
    {
        goto Failed;
    }

    TASK *Child = Execution->Task = TaskCreate(ForkUnusedEntry);

    if (Child == NULL)
    {
        goto Failed;
    }

    Child->AddressSpace = Execution->AddressSpace;
    Child->Execution = Execution;

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
            Physical = (UINT64) Copy;
            Execution->Pages[Execution->PageCount++] = Copy;
        }

        if (!VMMMapUserPages(Child->AddressSpace, Pages[Index].Address, &Physical, 1, Pages[Index].Flags))
        {
            goto Failed;
        }
    }

    Child->FileContext = KAlloc(sizeof(VFS_CONTEXT));

    if (Child->FileContext == NULL)
    {
        goto Failed;
    }

    *Child->FileContext = *Parent->FileContext;
    Child->RealUID = Parent->RealUID;
    Child->RealGID = Parent->RealGID;
    Child->AuthSession = Parent->AuthSession;
    KMemCpy(Child->Name, Parent->Name, sizeof(Child->Name));
    Child->UserStackBase = Parent->UserStackBase;
    Child->UserStackSize = Parent->UserStackSize;
    KMemCpy(Child->Assets, Parent->Assets, sizeof(Child->Assets));
    KMemCpy(&Child->Context, Frame, sizeof(Child->Context));
    Child->Context.RAX = 0;
    KMemCpy((VOID *) Child->RSP, &Child->Context, sizeof(Child->Context));

    if (!SyscallMemoryClone(Child, Parent) || !SyscallFilesInherit(Child, Parent))
    {
        goto Failed;
    }

    Slot->Execution = Execution;
    Slot->ParentPID = Parent->PID;
    Slot->Exited = FALSE;
    Slot->Waited = FALSE;
    Slot->Status = 0;
    Result = Child->PID;

    if (!LBPWRRAddTask(Child))
    {
        Slot->Execution = NULL;
        Result = -EAGAIN;

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

    if (Task == NULL || Task->Execution == NULL || Task->FileContext == NULL)
    {
        return ProcessEnd(-ESRCH);
    }

    INT64 Result = SyscallCopyPath(Task, Frame->RDI, &Name);

    if (Result < 0)
    {
        return ProcessEnd(Result);
    }

    ELF64_ARGUMENTS *Arguments = KAlloc(sizeof(*Arguments) * 2);
    ELF64_EXECUTION *New = KAllocPages(PROCESS_EXECUTION_PAGES);

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

    if (Result == 0 && !ELF64PrepareArguments(Task->FileContext, Name, Arguments, New))
    {
        Result = New->Error != 0 ? New->Error : -ENOEXEC;
    }

    if (Result == 0 && !ELF64SetEnvironment(New, Arguments + 1))
    {
        Result = -E2BIG;
    }

    if (Result == 0)
    {
        ELF64_EXECUTION   *Owner = Task->Execution;
        VMM_ADDRESS_SPACE *OldSpace = Task->AddressSpace;

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
            Owner->PageCount = New->PageCount;
            KMemCpy(Owner->Pages, New->Pages, New->PageCount * sizeof(VOID *));
            Task->FileContext->Credentials = New->Task->FileContext->Credentials;
            Task->AddressSpace = New->AddressSpace;
            Task->UserStackBase = New->Task->UserStackBase;
            Task->UserStackSize = New->Task->UserStackSize;
            KMemCpy(Task->Name, New->Task->Name, sizeof(Task->Name));
            Task->Context = New->Task->Context;
            KMemCpy(Frame, &Task->Context, sizeof(*Frame));
            KMemSet(Task->Assets, 0, sizeof(Task->Assets));
            New->AddressSpace = NULL;
            New->PageCount = 0;
            New->Task->AddressSpace = NULL;
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

VOID SyscallProcessExit(TASK *Task, UINT64 Code)
{
    if (!ProcessBegin())
    {
        return;
    }

    for (UINT32 Index = 0; Index < PROCESS_LIMIT; Index++)
    {
        PROCESS_SLOT *Slot = &ProcessSlots[Index];

        if (Slot->Execution != NULL && Slot->Execution->Task == Task)
        {
            Slot->Status = (INT32) ((Code & 255) << 8);
            Slot->Exited = TRUE;
            break;
        }
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&ProcessLock);

    WaitQueueWakeAll(&ProcessChildWaiters);
    SpinLockReleaseIRQRestore(&ProcessLock, Flags);
    ProcessEnd(0);
}

INT64 SyscallWait4(INT64 PID, UINT64 Status, UINT64 Options, UINT64 Usage)
{
    TASK *Parent = LBPWRRGetCurrentTask();

    if (Parent == NULL || Parent->AddressSpace == NULL)
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

            if (Slot->Execution == NULL || Slot->Execution->Task == NULL || Slot->ParentPID != Parent->PID || Slot->Waited)
            {
                continue;
            }

            TASK *Child = Slot->Execution->Task;

            if (PID != -1 && PID != Child->PID)
            {
                continue;
            }

            Found = TRUE;

            if (Slot->Exited || Child->State == TASK_TERMINATED || Child->State == TASK_KILLED)
            {
                INT32 Code = Slot->Exited ? Slot->Status : Child->State == TASK_KILLED ? 9 : 0;

                if (Status != 0 && !VMMCopyToUser(Parent->AddressSpace, Status, &Code, sizeof(Code)))
                {
                    return ProcessEnd(-EFAULT);
                }

                Slot->Waited = TRUE;

                return ProcessEnd(Child->PID);
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

        if (Parent == NULL || Parent->FileContext == NULL || Parent->RealUID != Task->RealUID)
        {
            break;
        }

        VFS_CONTEXT Context = *Parent->FileContext;

        Context.Credentials = Task->FileContext->Credentials;
        Result = SyscallVFSError(VFSChangeDirectory(&Context, Name));

        if (Result == 0)
        {
            Parent->FileContext->Partition = Context.Partition;
            KMemCpy(Parent->FileContext->Cwd, Context.Cwd, sizeof(Context.Cwd));
        }

        break;
    }

    return ProcessEnd(Result);
}
