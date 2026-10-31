// Kernel/Source/Syscall/Asset/SyscallAsset.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Asset/SyscallAsset.h>
#include <Scheduler/LBPWRR.h>
#include <Arch/AMD64/SpinLock.h>
#include <Memory/PMM.h>
#include <Syscall/Asset/Asset.h>
#include <Syscall/Errno.h>

#define ASSET_LIMIT          16U
#define ASSET_PAGE_SIZE      4096ULL
#define ASSET_PAGE_LIMIT     (64ULL * 1024 * 1024 / ASSET_PAGE_SIZE)
#define ASSET_MAPPING_STRIDE (LOS_ASSET_MAX_SIZE + ASSET_PAGE_SIZE)

typedef struct
{
    CHAR8   Name[LOS_ASSET_NAME_LIMIT];
    UINT64 *Pages;
    UINT64  Size;
    UINT32  PageCount;
} ASSET_ENTRY;

STATIC ASSET_ENTRY AssetEntries[ASSET_LIMIT];
STATIC SPIN_LOCK   AssetLock;
STATIC UINT64      AssetPages;
STATIC UINT64      AssetHandle;

STATIC BOOLEAN AssetReadName(TASK *Task, UINT64 Name, CHAR8 *Buffer)
{
    if (Name == 0)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < LOS_ASSET_NAME_LIMIT; Index++)
    {
        if (Name > UINT64_MAX - Index || !VMMCopyFromUser(Task->AddressSpace, Buffer + Index, Name + Index, 1))
        {
            return FALSE;
        }

        if (Buffer[Index] == 0)
        {
            return Index != 0;
        }
    }

    return FALSE;
}

STATIC BOOLEAN AssetNameEqual(CONST CHAR8 *Left, CONST CHAR8 *Right)
{
    for (UINT32 Index = 0; Index < LOS_ASSET_NAME_LIMIT; Index++)
    {
        if (Left[Index] != Right[Index])
        {
            return FALSE;
        }

        if (Left[Index] == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

INT64 SyscallAssetPublish(UINT64 Name, UINT64 Data, UINT64 Size)
{
    TASK        *Task = LBPWRRGetCurrentTask();
    CHAR8        Buffer[LOS_ASSET_NAME_LIMIT] = {0};
    ASSET_ENTRY *Entry = NULL;
    UINT64       Flags;
    UINT32       Pages = 0;
    UINT32       Count;
    INT64        Result = -ENOMEM;

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (!Task->AssetPublisher)
    {
        return -EPERM;
    }

    if (Size == 0 || Size > LOS_ASSET_MAX_SIZE)
    {
        return -EINVAL;
    }

    if (Data == 0 || Data >= (1ULL << 47) || Size > (1ULL << 47) - Data || !AssetReadName(Task, Name, Buffer))
    {
        return -EFAULT;
    }

    Count = (UINT32) ((Size + ASSET_PAGE_SIZE - 1) / ASSET_PAGE_SIZE);
    Flags = SpinLockAcquireIRQSave(&AssetLock);

    for (UINT32 Index = 0; Index < ASSET_LIMIT; Index++)
    {
        if (AssetEntries[Index].Pages != NULL && AssetNameEqual(AssetEntries[Index].Name, Buffer))
        {
            Result = -EEXIST;

            goto Cleanup;
        }

        if (Entry == NULL && AssetEntries[Index].Pages == NULL)
        {
            Entry = &AssetEntries[Index];
        }
    }

    if (Entry == NULL || Count > ASSET_PAGE_LIMIT - AssetPages)
    {
        Result = -ENOSPC;

        goto Cleanup;
    }

    Entry->Pages = KAlloc((UINT64) Count * sizeof(*Entry->Pages));

    if (Entry->Pages == NULL)
    {
        goto Cleanup;
    }

    while (Pages < Count)
    {
        VOID  *Page = KAllocPages(1);
        UINT64 Offset = (UINT64) Pages * ASSET_PAGE_SIZE;
        UINT64 Length = Size - Offset > ASSET_PAGE_SIZE ? ASSET_PAGE_SIZE : Size - Offset;

        if (Page == NULL)
        {
            goto Failed;
        }

        Entry->Pages[Pages++] = (UINT64) Page;

        if (!VMMCopyFromUser(Task->AddressSpace, Page, Data + Offset, Length))
        {
            Result = -EFAULT;

            goto Failed;
        }
    }

    KMemCpy(Entry->Name, Buffer, sizeof(Buffer));
    Entry->Size = Size;
    Entry->PageCount = Count;
    AssetPages += Count;
    Result = 0;

    goto Cleanup;
Failed:
    while (Pages != 0)
    {
        KMemFreePages((VOID *) Entry->Pages[--Pages], 1);
    }

    KFree(Entry->Pages);
    KMemSet(Entry, 0, sizeof(*Entry));
Cleanup:
    SpinLockReleaseIRQRestore(&AssetLock, Flags);

    return Result;
}

INT64 SyscallAssetAcquire(UINT64 Name, UINT64 UserAsset)
{
    TASK        *Task = LBPWRRGetCurrentTask();
    CHAR8        Buffer[LOS_ASSET_NAME_LIMIT];
    ASSET_ENTRY *Entry = NULL;
    TASK_ASSET  *Mapping = NULL;
    UINT32       Index = 0;
    UINT32       Slot = 0;
    UINT64       Flags;
    LOS_ASSET    Asset = {0};
    INT64        Result = -ENOENT;

    if (Task == NULL || Task->AddressSpace == NULL || UserAsset == 0 || !AssetReadName(Task, Name, Buffer))
    {
        return -EFAULT;
    }

    Flags = SpinLockAcquireIRQSave(&AssetLock);

    for (Index = 0; Index < ASSET_LIMIT; Index++)
    {
        if (AssetEntries[Index].Pages != NULL && AssetNameEqual(AssetEntries[Index].Name, Buffer))
        {
            Entry = &AssetEntries[Index];
            break;
        }
    }

    if (Entry == NULL)
    {
        goto Cleanup;
    }

    for (UINT32 Current = 0; Current < LOS_ASSET_MAPPING_LIMIT; Current++)
    {
        if (Task->Assets[Current].Handle != 0 && Task->Assets[Current].Index == Index)
        {
            Mapping = &Task->Assets[Current];
            Slot = Current;
            break;
        }

        if (Mapping == NULL && Task->Assets[Current].Handle == 0)
        {
            Mapping = &Task->Assets[Current];
            Slot = Current;
        }
    }

    Result = -ENOSPC;

    if (Mapping == NULL || Mapping->References == UINT32_MAX)
    {
        goto Cleanup;
    }

    Result = -EFAULT;

    if (!VMMCopyToUser(Task->AddressSpace, UserAsset, &Asset, sizeof(Asset)))
    {
        goto Cleanup;
    }

    Asset.Data = (CONST VOID *) (VMM_SHARED_USER_START + Slot * ASSET_MAPPING_STRIDE);
    Asset.Size = Entry->Size;

    if (Mapping->Handle == 0)
    {
        Result = -ENOMEM;

        if (AssetHandle == UINT64_MAX || !VMMMapSharedPages(Task->AddressSpace, (UINT64) Asset.Data, Entry->Pages, Entry->PageCount))
        {
            goto Cleanup;
        }

        Mapping->Handle = ++AssetHandle;
        Mapping->Index = Index;
    }

    Asset.Handle = Mapping->Handle;

    Result = -EFAULT;

    if (!VMMCopyToUser(Task->AddressSpace, UserAsset, &Asset, sizeof(Asset)))
    {
        if (Mapping->References == 0 && VMMUnmapSharedPages(Task->AddressSpace, (UINT64) Asset.Data, Entry->Pages, Entry->PageCount))
        {
            KMemSet(Mapping, 0, sizeof(*Mapping));
        }

        goto Cleanup;
    }

    Mapping->References++;
    Result = 0;
Cleanup:
    SpinLockReleaseIRQRestore(&AssetLock, Flags);

    return Result;
}

INT64 SyscallAssetRelease(UINT64 Handle)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    UINT64 Flags;
    INT64  Result = -EINVAL;

    if (Task == NULL || Task->AddressSpace == NULL || Handle == 0)
    {
        return -EINVAL;
    }

    Flags = SpinLockAcquireIRQSave(&AssetLock);

    for (UINT32 Slot = 0; Slot < LOS_ASSET_MAPPING_LIMIT; Slot++)
    {
        TASK_ASSET *Mapping = &Task->Assets[Slot];

        if (Mapping->Handle == Handle && Mapping->References != 0)
        {
            ASSET_ENTRY *Entry = &AssetEntries[Mapping->Index];

            if (Mapping->References > 1)
            {
                Mapping->References--;
                Result = 0;
            }
            else if (VMMUnmapSharedPages(Task->AddressSpace, VMM_SHARED_USER_START + Slot * ASSET_MAPPING_STRIDE, Entry->Pages, Entry->PageCount))
            {
                KMemSet(Mapping, 0, sizeof(*Mapping));
                Result = 0;
            }

            break;
        }
    }

    SpinLockReleaseIRQRestore(&AssetLock, Flags);

    return Result;
}
