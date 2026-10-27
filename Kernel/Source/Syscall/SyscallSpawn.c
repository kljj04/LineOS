// Kernel/Source/Syscall/SyscallSpawn.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallSpawn.h>
#include <Scheduler/LBPWRR.h>
#include <Exec/ELF64.h>
#include <Memory/PMM.h>
#include <Debug/Debug.h>

#define SPAWN_LIMIT           16U
#define SPAWN_EMPTY           0U
#define SPAWN_BUSY            1U
#define SPAWN_ACTIVE          2U
#define SPAWN_EXECUTION_PAGES ((sizeof(ELF64_EXECUTION) + 4095U) / 4096U)

typedef struct
{
    UINT32           State;
    ELF64_EXECUTION *Execution;
} SPAWN_SLOT;

STATIC SPAWN_SLOT SpawnSlots[SPAWN_LIMIT];

STATIC VOID SpawnReap(VOID)
{
    for (UINT32 Index = 0; Index < SPAWN_LIMIT; Index++)
    {
        UINT32           Expected = SPAWN_ACTIVE;
        SPAWN_SLOT      *Slot = &SpawnSlots[Index];
        ELF64_EXECUTION *Execution;

        if (!ATOMIC_COMPARE_EXCHANGE(&Slot->State, &Expected, SPAWN_BUSY, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED))
        {
            continue;
        }
        Execution = Slot->Execution;
        if (Execution->Task != NULL && (!Execution->Task->Scheduled || LBPWRRRemoveTask(Execution->Task)) && ELF64Release(Execution))
        {
            KMemFreePages(Execution, SPAWN_EXECUTION_PAGES);
            Slot->Execution = NULL;
            ATOMIC_STORE(&Slot->State, SPAWN_EMPTY, ATOMIC_RELEASE);
        }
        else
        {
            ATOMIC_STORE(&Slot->State, SPAWN_ACTIVE, ATOMIC_RELEASE);
        }
    }
}

STATIC INT64 SpawnExecute(UINT64 Path, CONST ELF64_ARGUMENTS *Arguments)
{
    TASK            *Task = LBPWRRGetCurrentTask();
    CHAR8           *Name;
    SPAWN_SLOT      *Slot = NULL;
    ELF64_EXECUTION *Execution;
    INT64            PID;
    BOOLEAN          Valid = FALSE;

    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL || Path == 0)
    {
        return -1;
    }
    Name = KAlloc(VFS_MAX_PATH);
    if (Name == NULL)
    {
        return -1;
    }
    for (UINTN Index = 0; Index < VFS_MAX_PATH; Index++)
    {
        if (Path > UINT64_MAX - Index || !VMMCopyFromUser(Task->AddressSpace, &Name[Index], Path + Index, 1))
        {
            break;
        }
        if (Name[Index] == 0)
        {
            Valid = Index != 0;
            break;
        }
    }
    if (!Valid)
    {
        KFree(Name);
        return -1;
    }
    SpawnReap();
    for (UINT32 Index = 0; Index < SPAWN_LIMIT; Index++)
    {
        UINT32 Expected = SPAWN_EMPTY;
        if (ATOMIC_COMPARE_EXCHANGE(&SpawnSlots[Index].State, &Expected, SPAWN_BUSY, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED))
        {
            Slot = &SpawnSlots[Index];
            break;
        }
    }
    if (Slot == NULL)
    {
        KFree(Name);
        return -1;
    }
    Execution = KAllocPages(SPAWN_EXECUTION_PAGES);
    if (Execution == NULL)
    {
        KFree(Name);
        ATOMIC_STORE(&Slot->State, SPAWN_EMPTY, ATOMIC_RELEASE);
        return -1;
    }
    if (!ELF64ExecuteArguments(Task->FileContext, Name, Arguments, Execution))
    {
        DebugWriteLine("spawn: ELF64Execute failed");
        KFree(Name);
        KMemFreePages(Execution, SPAWN_EXECUTION_PAGES);
        ATOMIC_STORE(&Slot->State, SPAWN_EMPTY, ATOMIC_RELEASE);
        return -1;
    }
    KFree(Name);
    PID = Execution->Task->PID;
    DebugWriteLine("spawn: scheduled");
    Slot->Execution = Execution;
    ATOMIC_STORE(&Slot->State, SPAWN_ACTIVE, ATOMIC_RELEASE);
    return PID;
}

INT64 SyscallSpawn(UINT64 Path)
{
    return SpawnExecute(Path, NULL);
}

INT64 SyscallSpawnV(UINT64 Path, UINT64 Argv)
{
    TASK            *Task = LBPWRRGetCurrentTask();
    ELF64_ARGUMENTS *Arguments;
    INT64            Result = -1;

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -1;
    }
    if (Argv == 0)
    {
        return SpawnExecute(Path, NULL);
    }
    Arguments = KAlloc(sizeof(*Arguments));
    if (Arguments == NULL)
    {
        return -1;
    }
    KMemSet(Arguments, 0, sizeof(*Arguments));
    for (UINT32 Index = 0; Index <= ELF64_ARG_MAX_COUNT; Index++)
    {
        UINT64  Pointer;
        UINT64  Offset = (UINT64) Index * sizeof(Pointer);
        BOOLEAN Terminated = FALSE;

        if (Argv > UINT64_MAX - Offset || !VMMCopyFromUser(Task->AddressSpace, &Pointer, Argv + Offset, sizeof(Pointer)))
        {
            goto Cleanup;
        }
        if (Pointer == 0)
        {
            Result = SpawnExecute(Path, Arguments);
            goto Cleanup;
        }
        if (Index == ELF64_ARG_MAX_COUNT)
        {
            goto Cleanup;
        }
        Arguments->Offsets[Index] = Arguments->Bytes;
        for (UINT32 Position = 0; Position < ELF64_ARG_MAX_LENGTH; Position++)
        {
            if (Arguments->Bytes == ELF64_ARG_MAX_BYTES || Pointer > UINT64_MAX - Position || !VMMCopyFromUser(Task->AddressSpace, &Arguments->Data[Arguments->Bytes], Pointer + Position, 1))
            {
                goto Cleanup;
            }
            if (Arguments->Data[Arguments->Bytes++] == 0)
            {
                Terminated = TRUE;
                break;
            }
        }
        if (!Terminated)
        {
            goto Cleanup;
        }
        Arguments->Count++;
    }

Cleanup:
    KFree(Arguments);
    return Result;
}
