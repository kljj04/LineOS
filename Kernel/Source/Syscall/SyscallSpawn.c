// Kernel/Source/Syscall/SyscallSpawn.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallSpawn.h>
#include <Syscall/SyscallError.h>
#include <Syscall/SyscallMemory.h>
#include <Syscall/SyscallFile.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Exec/ELF64.h>
#include <Memory/PMM.h>
#include <Debug/Debug.h>
#include <Debug/Panic.h>

STATIC_ASSERT(sizeof(TASK_CONTEXT) == sizeof(INTERRUPT_FRAME), "fork context size");
STATIC_ASSERT(__builtin_offsetof(TASK_CONTEXT, RIP) == __builtin_offsetof(INTERRUPT_FRAME, RIP) && __builtin_offsetof(TASK_CONTEXT, RAX) == __builtin_offsetof(INTERRUPT_FRAME, RAX) && __builtin_offsetof(TASK_CONTEXT, RSP) == __builtin_offsetof(INTERRUPT_FRAME, RSP), "fork context offsets");

#define SPAWN_LIMIT           16U
#define SPAWN_EXECUTION_PAGES ((sizeof(ELF64_EXECUTION) + 4095U) / 4096U)

typedef struct
{
    ELF64_EXECUTION *Execution;
} SPAWN_SLOT;

STATIC SPAWN_SLOT SpawnSlots[SPAWN_LIMIT];
STATIC UINT32     ProcessBusy;

STATIC BOOLEAN ProcessBegin(VOID)
{
    UINT32 Expected = 0;
    return ATOMIC_COMPARE_EXCHANGE(&ProcessBusy, &Expected, 1, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED);
}

STATIC INT64 ProcessEnd(INT64 Result)
{
    ATOMIC_STORE(&ProcessBusy, 0, ATOMIC_RELEASE);
    return Result;
}

STATIC SPAWN_SLOT *SpawnReserve(VOID)
{
    for (UINT32 Index = 0; Index < SPAWN_LIMIT; Index++)
    {
        SPAWN_SLOT      *Slot = &SpawnSlots[Index];
        ELF64_EXECUTION *Execution = Slot->Execution;
        if (Execution != NULL && Execution->Task != NULL && (!Execution->Task->Scheduled || LBPWRRRemoveTask(Execution->Task)) && ELF64Release(Execution))
        {
            KMemFreePages(Execution, SPAWN_EXECUTION_PAGES);
            Slot->Execution = NULL;
        }
    }
    for (UINT32 Index = 0; Index < SPAWN_LIMIT; Index++)
    {
        if (SpawnSlots[Index].Execution == NULL)
        {
            return &SpawnSlots[Index];
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

STATIC INT64 SpawnExecute(UINT64 Path, UINT64 Argv)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;
    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }
    INT64 Error = SyscallCopyPath(Task, Path, &Name);
    if (Error < 0)
    {
        return Error;
    }
    ELF64_ARGUMENTS *Arguments = KAlloc(sizeof(*Arguments));
    if (Arguments == NULL)
    {
        KFree(Name);
        return -ENOMEM;
    }
    Error = ProcessCopyArguments(Task, Argv, Arguments);
    SPAWN_SLOT *Slot = Error == 0 ? SpawnReserve() : NULL;
    if (Error == 0 && Slot == NULL)
    {
        Error = -EAGAIN;
    }
    ELF64_EXECUTION *Execution = Error == 0 ? KAllocPages(SPAWN_EXECUTION_PAGES) : NULL;
    if (Error == 0 && Execution == NULL)
    {
        Error = -ENOMEM;
    }
    if (Error == 0)
    {
        if (ELF64ExecuteArguments(Task->FileContext, Name, Arguments, Execution))
        {
            Slot->Execution = Execution;
            Error = Execution->Task->PID;
            DebugWriteLine("spawn: scheduled");
        }
        else
        {
            Error = Execution->Error != 0 ? Execution->Error : -ENOEXEC;
            KMemFreePages(Execution, SPAWN_EXECUTION_PAGES);
        }
    }
    KFree(Arguments);
    KFree(Name);
    return Error;
}

INT64 SyscallSpawn(UINT64 Path)
{
    if (!ProcessBegin())
    {
        return -EAGAIN;
    }
    return ProcessEnd(SpawnExecute(Path, 0));
}

INT64 SyscallSpawnV(UINT64 Path, UINT64 Argv)
{
    if (!ProcessBegin())
    {
        return -EAGAIN;
    }
    return ProcessEnd(SpawnExecute(Path, Argv));
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
    SPAWN_SLOT *Slot = SpawnReserve();
    if (Slot == NULL)
    {
        return ProcessEnd(-EAGAIN);
    }
    if (!VMMListUserPages(Parent->AddressSpace, NULL, 0, &Count) || Count > ELF64_EXEC_MAX_PAGES)
    {
        return ProcessEnd(-EAGAIN);
    }
    VMM_USER_PAGE   *Pages = KAlloc(Count * sizeof(*Pages));
    ELF64_EXECUTION *Execution = KAllocPages(SPAWN_EXECUTION_PAGES);
    if (Pages == NULL || Execution == NULL)
    {
        if (Pages != NULL)
            KFree(Pages);
        if (Execution != NULL)
            KMemFreePages(Execution, SPAWN_EXECUTION_PAGES);
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
    KMemFreePages(Execution, SPAWN_EXECUTION_PAGES);
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
    ELF64_EXECUTION *New = KAllocPages(SPAWN_EXECUTION_PAGES);
    if (Arguments == NULL || New == NULL)
    {
        KFree(Name);
        if (Arguments != NULL)
            KFree(Arguments);
        if (New != NULL)
            KMemFreePages(New, SPAWN_EXECUTION_PAGES);
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
            SyscallMemoryRelease(Task);
            while (Owner->PageCount != 0)
            {
                KMemFreePages(Owner->Pages[--Owner->PageCount], 1);
            }
            Owner->AddressSpace = New->AddressSpace;
            Owner->PageCount = New->PageCount;
            KMemCpy(Owner->Pages, New->Pages, New->PageCount * sizeof(VOID *));
            Task->AddressSpace = New->AddressSpace;
            Task->UserStackBase = New->Task->UserStackBase;
            Task->UserStackSize = New->Task->UserStackSize;
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
    KMemFreePages(New, SPAWN_EXECUTION_PAGES);
    KFree(Arguments);
    KFree(Name);
    return ProcessEnd(Result);
}
