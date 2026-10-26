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

INT64 SyscallSpawn(UINT64 Path)
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
    if (!ELF64Execute(Task->FileContext, Name, Execution))
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
