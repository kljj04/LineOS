// Kernel/Source/Syscall/Memory/SyscallMemory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Memory/SyscallMemory.h>
#include <Syscall/Memory/Memory.h>
#include <Syscall/Errno.h>
#include <Memory/PMM.h>
#include <Exec/ELF64.h>
#include <Scheduler/LBPWRR.h>

#define MMAP_MAX_LENGTH (64ULL * 1024 * 1024)
#define MMAP_MAX_PAGES  (128ULL * 1024 * 1024 / 4096)

typedef struct MMAP_REGION
{
    struct MMAP_REGION *Next;
    UINT64              Address;
    UINT32              Count;
    BOOLEAN             Owned;
    UINT64              Pages[];
} MMAP_REGION;

STATIC VOID MemoryFreePage(TASK *Task, UINT64 Physical)
{
    if (Task->Execution != NULL)
    {
        ELF64_EXECUTION *Execution = Task->Execution;

        for (UINT64 Index = 0; Index < Execution->PageCount; Index++)
        {
            if ((UINT64) Execution->Pages[Index] == Physical)
            {
                Execution->Pages[Index] = Execution->Pages[--Execution->PageCount];
                break;
            }
        }
    }

    KMemFreePages((VOID *) Physical, 1);
}

VOID SyscallMemoryRelease(TASK *Task)
{
    while (Task->Mappings != NULL)
    {
        MMAP_REGION *Region = Task->Mappings;

        Task->Mappings = Region->Next;

        for (UINT32 Index = 0; Region->Owned && Index < Region->Count; Index++)
        {
            if (Region->Pages[Index] != 0)
            {
                MemoryFreePage(Task, Region->Pages[Index]);
            }
        }

        KFree(Region);
    }
}

BOOLEAN SyscallMemoryShared(TASK *Task, UINT64 Address)
{
    if (Address >= VMM_SHARED_USER_START && Address < VMM_SHARED_USER_END)
    {
        return TRUE;
    }

    for (MMAP_REGION *Region = Task->Mappings; Region != NULL; Region = Region->Next)
    {
        if (Address >= Region->Address && Address - Region->Address < (UINT64) Region->Count * 4096)
        {
            return !Region->Owned;
        }
    }

    return FALSE;
}

BOOLEAN SyscallMemoryClone(TASK *Child, TASK *Parent)
{
    for (MMAP_REGION *Region = Parent->Mappings; Region != NULL; Region = Region->Next)
    {
        MMAP_REGION *Copy = KAlloc(sizeof(*Copy) + (UINT64) Region->Count * sizeof(UINT64));

        if (Copy == NULL)
        {
            return FALSE;
        }

        KMemSet(Copy, 0, sizeof(*Copy) + (UINT64) Region->Count * sizeof(UINT64));
        Copy->Address = Region->Address;
        Copy->Count = Region->Count;
        Copy->Owned = Region->Owned;
        Copy->Next = Child->Mappings;
        Child->Mappings = Copy;

        for (UINT32 Index = 0; Index < Copy->Count; Index++)
        {
            if (Region->Pages[Index] != 0)
            {
                if (!VMMTranslateAddressSpace(Child->AddressSpace, Copy->Address + (UINT64) Index * 4096, &Copy->Pages[Index]))
                {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

INT64 SyscallMmap(UINT64 Address, UINT64 Length, UINT64 Protection, UINT64 Flags, INT64 FD, UINT64 Offset)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    UINT64 Physical = 0;

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (Length == 0 || Length > UINT64_MAX - 4095 || (Offset & 4095) != 0 || (Protection & ~7ULL) != 0 || ((Flags & 3) != MAP_PRIVATE && (Flags & 3) != MAP_SHARED))
    {
        return -EINVAL;
    }

    if ((Flags & ~(MAP_PRIVATE | MAP_SHARED | MAP_ANONYMOUS)) != 0 || Protection == PROT_NONE)
    {
        return -EOPNOTSUPP;
    }

    if (Length > MMAP_MAX_LENGTH)
    {
        return -ENOMEM;
    }

    BOOLEAN Owned = (Flags & MAP_ANONYMOUS) != 0;

    if (Owned && ((Flags & 3) != MAP_PRIVATE || Offset != 0))
    {
        return -EOPNOTSUPP;
    }

    if (!Owned)
    {
        INT64 Error = SyscallMapDevice(FD, Length, Protection, Flags, Offset, &Physical);

        if (Error < 0)
        {
            return Error;
        }
    }

    UINT32 Count = (UINT32) ((Length + 4095) / 4096);
    UINT64 Used = 0;

    for (MMAP_REGION *Current = Task->Mappings; Current != NULL; Current = Current->Next)
    {
        Used += Current->Count;
    }

    if (Count > MMAP_MAX_PAGES - Used)
    {
        return -ENOMEM;
    }

    if (Address < VMM_MMAP_START || Address >= VMM_MMAP_END || Count > (VMM_MMAP_END - Address) / 4096)
    {
        Address = VMM_MMAP_START;
    }

    Address &= ~4095ULL;

    for (;;)
    {
        BOOLEAN Found = TRUE;

        for (UINT32 Index = 0; Index < Count; Index++)
        {
            UINT64 Existing;

            if (VMMTranslateAddressSpace(Task->AddressSpace, Address + (UINT64) Index * 4096, &Existing))
            {
                Address += ((UINT64) Index + 1) * 4096;
                Found = FALSE;
                break;
            }
        }

        if (Address >= VMM_MMAP_END || Count > (VMM_MMAP_END - Address) / 4096)
        {
            return -ENOMEM;
        }

        if (Found)
        {
            break;
        }
    }

    MMAP_REGION *Region = KAlloc(sizeof(*Region) + (UINT64) Count * sizeof(UINT64));

    if (Region == NULL)
    {
        return -ENOMEM;
    }

    KMemSet(Region, 0, sizeof(*Region) + (UINT64) Count * sizeof(UINT64));
    Region->Address = Address;
    Region->Count = Count;
    Region->Owned = Owned;

    for (UINT32 Index = 0; Index < Count; Index++)
    {
        Region->Pages[Index] = Owned ? (UINT64) KAllocPages(1) : Physical + (UINT64) Index * 4096;

        if (Region->Pages[Index] == 0)
        {
            goto Failed;
        }
    }

    if (!VMMMapUserPages(Task->AddressSpace, Address, Region->Pages, Count, ((Protection & PROT_WRITE) != 0 ? VMM_FLAG_WRITABLE : 0) | ((Protection & PROT_EXEC) == 0 ? VMM_FLAG_NO_EXECUTE : 0)))
    {
        goto Failed;
    }

    Region->Next = Task->Mappings;
    Task->Mappings = Region;

    return (INT64) Address;
Failed:
    for (UINT32 Index = 0; Owned && Index < Count; Index++)
    {
        if (Region->Pages[Index] != 0)
        {
            KMemFreePages((VOID *) Region->Pages[Index], 1);
        }
    }

    KFree(Region);

    return -ENOMEM;
}

INT64 SyscallMunmap(UINT64 Address, UINT64 Length)
{
    TASK *Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if ((Address & 4095) != 0 || Length == 0 || Address < VMM_MMAP_START || Address >= VMM_MMAP_END || Length > VMM_MMAP_END - Address)
    {
        return -EINVAL;
    }

    Length = (Length + 4095) & ~4095ULL;

    MMAP_REGION **Link = &Task->Mappings;

    while (*Link != NULL)
    {
        MMAP_REGION *Region = *Link;
        UINT64       Start = Address > Region->Address ? Address : Region->Address;
        UINT64       End = Address + Length < Region->Address + (UINT64) Region->Count * 4096 ? Address + Length : Region->Address + (UINT64) Region->Count * 4096;

        if (Start < End)
        {
            UINT32 First = (UINT32) ((Start - Region->Address) / 4096);
            UINT32 Count = (UINT32) ((End - Start) / 4096);

            if (!VMMUnmapUserPages(Task->AddressSpace, Start, Region->Pages + First, Count))
            {
                return -EBUSY;
            }

            for (UINT32 Index = First; Index < First + Count; Index++)
            {
                if (Region->Owned && Region->Pages[Index] != 0)
                {
                    MemoryFreePage(Task, Region->Pages[Index]);
                }

                Region->Pages[Index] = 0;
            }
        }

        BOOLEAN Empty = TRUE;

        for (UINT32 Index = 0; Index < Region->Count; Index++)
        {
            Empty = Empty && Region->Pages[Index] == 0;
        }

        if (Empty)
        {
            *Link = Region->Next;
            KFree(Region);
        }
        else
        {
            Link = &Region->Next;
        }
    }

    return 0;
}
