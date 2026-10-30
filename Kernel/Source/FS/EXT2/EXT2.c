// Kernel/Source/FS/EXT2/EXT2.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2.h>
#include <FS/EXT2/EXT2Superblock.h>
#include <FS/EXT2/EXT2Inode.h>
#include <FS/EXT2/EXT2Directory.h>
#include <FS/EXT2/EXT2File.h>
#include <Memory/PMM.h>
#include <Timer/Clock.h>

UINT16 EXT2Load16(CONST UINT8 *Data)
{
    return (UINT16) Data[0] | (UINT16) Data[1] << 8;
}

UINT32 EXT2Load32(CONST UINT8 *Data)
{
    return EXT2Load16(Data) | (UINT32) EXT2Load16(Data + 2) << 16;
}

VOID EXT2Store16(UINT8 *Data, UINT16 Value)
{
    Data[0] = (UINT8) Value;
    Data[1] = (UINT8) (Value >> 8);
}

VOID EXT2Store32(UINT8 *Data, UINT32 Value)
{
    EXT2Store16(Data, (UINT16) Value);
    EXT2Store16(Data + 2, (UINT16) (Value >> 16));
}

BOOLEAN EXT2Fail(EXT2_VOLUME *Volume, EXT2_ERROR Error)
{
    if (Volume != NULL)
    {
        Volume->Error = Error;
    }

    return FALSE;
}

STATIC BOOLEAN EXT2DiskBlock(EXT2_VOLUME *Volume, UINT32 Block, VOID *Buffer, BOOLEAN Write)
{
    UINT64 Sector = (UINT64) Block * (Volume->BlockSize / 512);
    UINT32 Count = Volume->BlockSize / 512;

    if (Block >= Volume->BlockCount || Sector >= Volume->Partition.SectorCount || Count > Volume->Partition.SectorCount - Sector)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    if (Write)
    {
        EXT2InvalidateReadCache(Volume);
        KMemCpy(Volume->DMABuffer, Buffer, Volume->BlockSize);

        if (!VirtIOBlockWriteSync(Volume->Device, Volume->Partition.FirstLBA + Sector, Count, Volume->DMABuffer))
        {
            return EXT2Fail(Volume, EXT2_ERROR_IO);
        }
    }
    else
    {
        if (!VirtIOBlockReadSync(Volume->Device, Volume->Partition.FirstLBA + Sector, Count, Volume->DMABuffer))
        {
            return EXT2Fail(Volume, EXT2_ERROR_IO);
        }

        KMemCpy(Buffer, Volume->DMABuffer, Volume->BlockSize);
    }

    return TRUE;
}

BOOLEAN EXT2ReadBlock(EXT2_VOLUME *Volume, UINT32 Block, VOID *Buffer)
{
    for (EXT2_CHANGE *Change = Volume->ChangeBuckets == NULL ? NULL : Volume->ChangeBuckets[Block % EXT2_CHANGE_BUCKETS]; Change != NULL; Change = Change->HashNext)
    {
        if (Change->Block == Block)
        {
            KMemCpy(Buffer, Change->Data, Volume->BlockSize);
            return TRUE;
        }
    }

    return EXT2DiskBlock(Volume, Block, Buffer, FALSE);
}

BOOLEAN EXT2ReadBlocks(EXT2_VOLUME *Volume, UINT32 Block, UINT32 Count, VOID *Buffer)
{
    UINT64 Sector = (UINT64) Block * (Volume->BlockSize / 512);
    UINT64 Sectors = (UINT64) Count * (Volume->BlockSize / 512);

    if (Count == 0 || Count > (UINT64) Volume->DMAPages * EXT2_MAX_BLOCK_SIZE / Volume->BlockSize || Block >= Volume->BlockCount || Count > Volume->BlockCount - Block || Sector >= Volume->Partition.SectorCount || Sectors > Volume->Partition.SectorCount - Sector)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    if (Volume->Transaction)
    {
        for (UINT32 Index = 0; Index < Count; Index++)
        {
            if (!EXT2ReadBlock(Volume, Block + Index, (UINT8 *) Buffer + (UINT64) Index * Volume->BlockSize))
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    if (!VirtIOBlockReadSync(Volume->Device, Volume->Partition.FirstLBA + Sector, (UINT32) Sectors, Volume->DMABuffer))
    {
        return EXT2Fail(Volume, EXT2_ERROR_IO);
    }

    KMemCpy(Buffer, Volume->DMABuffer, (UINT64) Count * Volume->BlockSize);
    return TRUE;
}

VOID EXT2InvalidateReadCache(EXT2_VOLUME *Volume)
{
    if (Volume->ReadCache != NULL)
    {
        for (UINT32 Index = 0; Index < EXT2_READ_CACHE_BLOCKS; Index++)
        {
            Volume->ReadCache[Index].Valid = FALSE;
        }
    }

    Volume->ReadCacheNext = 0;
}

BOOLEAN EXT2ReadMetadataBlock(EXT2_VOLUME *Volume, UINT32 Block, VOID *Buffer)
{
    EXT2_READ_CACHE *Entry;

    if (Volume->ReadCache == NULL || Volume->Transaction)
    {
        return EXT2ReadBlock(Volume, Block, Buffer);
    }

    for (UINT32 Index = 0; Index < EXT2_READ_CACHE_BLOCKS; Index++)
    {
        Entry = &Volume->ReadCache[Index];

        if (Entry->Valid && Entry->Block == Block)
        {
            KMemCpy(Buffer, Entry->Data, Volume->BlockSize);
            return TRUE;
        }
    }

    if (!EXT2ReadBlock(Volume, Block, Buffer))
    {
        return FALSE;
    }

    Entry = &Volume->ReadCache[Volume->ReadCacheNext];
    Entry->Block = Block;
    KMemCpy(Entry->Data, Buffer, Volume->BlockSize);
    Entry->Valid = TRUE;
    Volume->ReadCacheNext = (Volume->ReadCacheNext + 1) % EXT2_READ_CACHE_BLOCKS;
    return TRUE;
}

UINT8 *EXT2EditBlock(EXT2_VOLUME *Volume, UINT32 Block)
{
    EXT2_CHANGE *Change;

    if (!Volume->Transaction || !Volume->Writable || Volume->Poisoned)
    {
        EXT2Fail(Volume, EXT2_ERROR_READ_ONLY);
        return NULL;
    }

    for (Change = Volume->ChangeBuckets[Block % EXT2_CHANGE_BUCKETS]; Change != NULL; Change = Change->HashNext)
    {
        if (Change->Block == Block)
        {
            return Change->Data;
        }
    }

    if (Volume->ChangeCount >= EXT2_TRANSACTION_LIMIT)
    {
        EXT2Fail(Volume, EXT2_ERROR_LIMIT);
        return NULL;
    }

    Change = KAlloc(sizeof(*Change) + Volume->BlockSize);

    if (Change == NULL)
    {
        EXT2Fail(Volume, EXT2_ERROR_MEMORY);
        return NULL;
    }

    if (!EXT2DiskBlock(Volume, Block, Change->Data, FALSE))
    {
        KFree(Change);
        return NULL;
    }

    Change->Block = Block;
    Change->Next = Volume->Changes;
    Volume->Changes = Change;
    Change->HashNext = Volume->ChangeBuckets[Block % EXT2_CHANGE_BUCKETS];
    Volume->ChangeBuckets[Block % EXT2_CHANGE_BUCKETS] = Change;
    Volume->ChangeCount++;
    return Change->Data;
}

BOOLEAN EXT2Begin(EXT2_VOLUME *Volume)
{
    if (!Volume->Writable || Volume->Poisoned || Volume->Transaction)
    {
        return EXT2Fail(Volume, EXT2_ERROR_READ_ONLY);
    }

    Volume->ChangeBuckets = KAlloc(EXT2_CHANGE_BUCKETS * sizeof(*Volume->ChangeBuckets));

    if (Volume->ChangeBuckets == NULL)
    {
        return EXT2Fail(Volume, EXT2_ERROR_MEMORY);
    }

    KMemSet(Volume->ChangeBuckets, 0, EXT2_CHANGE_BUCKETS * sizeof(*Volume->ChangeBuckets));
    EXT2InvalidateReadCache(Volume);
    Volume->Transaction = TRUE;
    return TRUE;
}

VOID EXT2Abort(EXT2_VOLUME *Volume)
{
    EXT2InvalidateReadCache(Volume);
    while (Volume->Changes != NULL)
    {
        EXT2_CHANGE *Change = Volume->Changes;

        Volume->Changes = Change->Next;
        KFree(Change);
    }

    if (Volume->ChangeBuckets != NULL)
    {
        KFree(Volume->ChangeBuckets);
        Volume->ChangeBuckets = NULL;
    }

    Volume->ChangeCount = 0;
    Volume->Transaction = FALSE;
}

BOOLEAN EXT2Commit(EXT2_VOLUME *Volume)
{
    UINT8   Superblock[1024];
    UINT32  SuperBlock = 1024 / Volume->BlockSize;
    UINT32  SuperOffset = 1024 % Volume->BlockSize;
    BOOLEAN Result = FALSE;

    if (!Volume->Transaction)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    KMemCpy(Superblock, Volume->Superblock, 1024);
    EXT2Store16(Superblock + 58, 0);

    if (!EXT2WriteSuperblock(Volume, Superblock) || !VirtIOBlockFlushSync(Volume->Device))
    {
        goto Failed;
    }

    for (EXT2_CHANGE *Change = Volume->Changes; Change != NULL; Change = Change->Next)
    {
        if (Change->Block == SuperBlock)
        {
            EXT2Store16(Change->Data + SuperOffset + 58, 0);
            KMemCpy(Superblock, Change->Data + SuperOffset, 1024);
        }

        if (!EXT2DiskBlock(Volume, Change->Block, Change->Data, TRUE))
        {
            goto Failed;
        }
    }

    if (!VirtIOBlockFlushSync(Volume->Device))
    {
        goto Failed;
    }

    EXT2Store16(Superblock + 58, 1);

    if (!EXT2WriteSuperblock(Volume, Superblock) || !VirtIOBlockFlushSync(Volume->Device))
    {
        goto Failed;
    }

    KMemCpy(Volume->Superblock, Superblock, 1024);
    Result = TRUE;
    goto Done;
Failed:
    Volume->Poisoned = TRUE;
    Volume->Writable = FALSE;
    EXT2Fail(Volume, EXT2_ERROR_IO);
    EXT2Store16(Superblock + 58, 0);
    EXT2WriteSuperblock(Volume, Superblock);
    VirtIOBlockFlushSync(Volume->Device);
Done:
    EXT2Abort(Volume);
    return Result;
}

STATIC BOOLEAN EXT2Enter(EXT2_VOLUME *Volume)
{
    UINT32 Expected = 0;

    if (Volume == NULL || !Volume->Mounted)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (!ATOMIC_COMPARE_EXCHANGE(&Volume->Busy, &Expected, 1, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED))
    {
        return FALSE;
    }

    Volume->Error = EXT2_ERROR_NONE;

    if (Volume->Poisoned)
    {
        Volume->Error = EXT2_ERROR_IO;
        ATOMIC_STORE(&Volume->Busy, 0, ATOMIC_RELEASE);
        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN EXT2Leave(EXT2_VOLUME *Volume, BOOLEAN Result)
{
    ATOMIC_STORE(&Volume->Busy, 0, ATOMIC_RELEASE);
    return Result;
}

BOOLEAN EXT2Mount(EXT2_VOLUME *Volume, VIRTIO_BLOCK_DEVICE *Device, CONST GPT_PARTITION *Partition, BOOLEAN Writable)
{
    EXT2_INODE Root;

    if (Volume == NULL || Device == NULL || Partition == NULL)
    {
        return FALSE;
    }

    KMemSet(Volume, 0, sizeof(*Volume));
    Volume->Device = Device;
    Volume->Partition = *Partition;
    Volume->Writable = Writable;

    if (Partition->SectorCount < 4 || Partition->FirstLBA >= VirtIOBlockGetCapacity(Device) || Partition->SectorCount > VirtIOBlockGetCapacity(Device) - Partition->FirstLBA)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (Writable && (!Device->FlushSupported || (Partition->Attributes & (1ULL << 60)) != 0))
    {
        return EXT2Fail(Volume, EXT2_ERROR_READ_ONLY);
    }

    Volume->DMAPages = EXT2_READ_DMA_PAGES;
    Volume->DMABuffer = KAllocPagesBelow(Volume->DMAPages, 0x100000000ULL);

    if (Volume->DMABuffer == NULL)
    {
        Volume->DMAPages = 1;
        Volume->DMABuffer = KAllocPagesBelow(Volume->DMAPages, 0x100000000ULL);
    }

    if (Volume->DMABuffer == NULL)
    {
        return EXT2Fail(Volume, EXT2_ERROR_MEMORY);
    }

    if (!EXT2LoadSuperblock(Volume) || !EXT2ValidateGroups(Volume) || (Writable && (!EXT2ValidateOwnership(Volume) || !EXT2ValidateDirectories(Volume))) || !EXT2ReadInode(Volume, 2, &Root) || (EXT2Load16(Root.Raw) & EXT2_MODE_TYPE) != EXT2_MODE_DIRECTORY)
    {
        if (Volume->Error == EXT2_ERROR_NONE)
        {
            Volume->Error = EXT2_ERROR_CORRUPT;
        }

        KMemFreePages(Volume->DMABuffer, Volume->DMAPages);
        Volume->DMABuffer = NULL;
        return FALSE;
    }

    Volume->ReadCache = KAlloc(EXT2_READ_CACHE_BLOCKS * sizeof(*Volume->ReadCache));
    EXT2InvalidateReadCache(Volume);
    Volume->Mounted = TRUE;
    return TRUE;
}

BOOLEAN EXT2MountP2(EXT2_VOLUME *Volume, VIRTIO_BLOCK_DEVICE *Device, BOOLEAN Writable)
{
    GPT_PARTITION Partition;
    CONST UINT8   LinuxGUID[16] = {0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4};

    if (!GPTReadPartition(Device, 2, &Partition))
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    for (UINT32 Index = 0; Index < 16; Index++)
    {
        if (Partition.TypeGUID[Index] != LinuxGUID[Index])
        {
            return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }
    }

    return EXT2Mount(Volume, Device, &Partition, Writable);
}

BOOLEAN EXT2Unmount(EXT2_VOLUME *Volume)
{
    UINT32 Expected = 0;

    if (Volume == NULL || !Volume->Mounted)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (!ATOMIC_COMPARE_EXCHANGE(&Volume->Busy, &Expected, 1, FALSE, ATOMIC_ACQUIRE, ATOMIC_RELAXED))
    {
        return FALSE;
    }

    EXT2Abort(Volume);

    if (Volume->ReadCache != NULL)
    {
        KFree(Volume->ReadCache);
        Volume->ReadCache = NULL;
    }

    KMemFreePages(Volume->DMABuffer, Volume->DMAPages);
    Volume->DMABuffer = NULL;
    Volume->Mounted = FALSE;
    return EXT2Leave(Volume, TRUE);
}

BOOLEAN EXT2Lookup(EXT2_VOLUME *Volume, CONST CHAR8 *Path, UINT32 *Number, EXT2_INODE *Inode)
{
    BOOLEAN Result;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = Path != NULL && Number != NULL && Inode != NULL ? EXT2Resolve(Volume, Path, Number, Inode) : EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2GetCurrentTime(EXT2_VOLUME *Volume, UINT32 *Time)
{
    UINT64 Now = ClockGetUnixTime();

    if (Time == NULL || Now == 0 || Now > UINT32_MAX)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CLOCK);
    }

    *Time = (UINT32) Now;
    return TRUE;
}

BOOLEAN EXT2TouchAccess(EXT2_VOLUME *Volume, UINT32 Number, EXT2_INODE *Inode)
{
    UINT32 Now;
    UINT32 Access = EXT2Load32(Inode->Raw + 8);

    if (!Volume->Writable || (EXT2Load32(Inode->Raw + 32) & 0x80) != 0)
    {
        return TRUE;
    }

    if (!EXT2GetCurrentTime(Volume, &Now))
    {
        return FALSE;
    }

    if (Access >= EXT2Load32(Inode->Raw + 12) && Access >= EXT2Load32(Inode->Raw + 16) && (Now < Access || Now - Access < 86400))
    {
        return TRUE;
    }

    if (!EXT2Begin(Volume))
    {
        return FALSE;
    }

    EXT2Store32(Inode->Raw + 8, Now);

    if (!EXT2WriteInode(Volume, Number, Inode, FALSE))
    {
        EXT2Abort(Volume);
        return FALSE;
    }

    return EXT2Commit(Volume);
}

BOOLEAN EXT2ReadFile(EXT2_VOLUME *Volume, UINT32 Number, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength)
{
    EXT2_INODE Inode;
    BOOLEAN    Result;

    if (ReadLength != NULL)
    {
        *ReadLength = 0;
    }

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = FALSE;

    if (ReadLength == NULL || (Length != 0 && Buffer == NULL))
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else if (EXT2ReadInode(Volume, Number, &Inode))
    {
        if ((EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) != EXT2_MODE_FILE)
        {
            EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }
        else
        {
            Result = EXT2Access(Volume, &Inode, 4) && EXT2ReadData(Volume, &Inode, Offset, Buffer, Length, ReadLength);

            if (Result && *ReadLength != 0)
            {
                Result = EXT2TouchAccess(Volume, Number, &Inode);
            }
        }
    }

    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2WriteFile(EXT2_VOLUME *Volume, UINT32 Number, UINT64 Offset, CONST VOID *Buffer, UINT32 Length, UINT32 Time)
{
    EXT2_INODE Inode;
    BOOLEAN    Result = FALSE;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if ((Length != 0 && Buffer == NULL) || Number < Volume->FirstInode)
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else if (EXT2Begin(Volume) && EXT2ReadInode(Volume, Number, &Inode))
    {
        if ((EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) != EXT2_MODE_FILE || EXT2Load32(Inode.Raw + 32) != 0)
        {
            EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }
        else if (!EXT2Access(Volume, &Inode, 2))
        {
            Result = FALSE;
        }
        else if (Length == 0)
        {
            Result = TRUE;
        }
        else if (EXT2WriteData(Volume, &Inode, Offset, Buffer, Length))
        {
            if (Volume->UID != 0)
            {
                EXT2Store16(Inode.Raw, EXT2Load16(Inode.Raw) & ~06000U);
            }

            EXT2Store32(Inode.Raw + 12, Time);
            EXT2Store32(Inode.Raw + 16, Time);
            Result = EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2Commit(Volume);
        }
    }

    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Create(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, BOOLEAN Directory, UINT16 Permissions, UINT32 Time, UINT32 *Number)
{
    BOOLEAN Result = FALSE;
    UINT32  NewNumber = 0;

    if (Number != NULL)
    {
        *Number = 0;
    }

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (Name == NULL || Number == NULL || (Permissions & ~0777U) != 0)
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else if (EXT2Begin(Volume) && EXT2CreateEntry(Volume, Parent, Name, Directory, Permissions, Time, &NewNumber) && EXT2Commit(Volume))
    {
        *Number = NewNumber;
        Result = TRUE;
    }

    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Truncate(EXT2_VOLUME *Volume, UINT32 Number, UINT64 Size, UINT32 Time)
{
    EXT2_INODE Inode;
    BOOLEAN    Result = FALSE;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (Number < Volume->FirstInode || Size > EXT2MaximumFileSize(Volume))
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else if (EXT2Begin(Volume) && EXT2ReadInode(Volume, Number, &Inode))
    {
        if ((EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) != EXT2_MODE_FILE)
        {
            EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
        }
        else if (EXT2Access(Volume, &Inode, 2) && EXT2ResizeInode(Volume, &Inode, Size))
        {
            if (Volume->UID != 0)
            {
                EXT2Store16(Inode.Raw, EXT2Load16(Inode.Raw) & ~06000U);
            }

            EXT2Store32(Inode.Raw + 12, Time);
            EXT2Store32(Inode.Raw + 16, Time);
            Result = EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2Commit(Volume);
        }
    }

    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Remove(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 Time)
{
    BOOLEAN Result;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = EXT2Begin(Volume) && EXT2RemoveEntry(Volume, Parent, Name, Time) && EXT2Commit(Volume);
    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Rename(EXT2_VOLUME *Volume, UINT32 OldParent, CONST CHAR8 *OldName, UINT32 NewParent, CONST CHAR8 *NewName, UINT32 Time)
{
    BOOLEAN Result;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = EXT2Begin(Volume) && EXT2RenameEntry(Volume, OldParent, OldName, NewParent, NewName, Time) && EXT2Commit(Volume);
    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Link(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 Number, UINT32 Time)
{
    BOOLEAN Result;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = EXT2Begin(Volume) && EXT2LinkEntry(Volume, Parent, Name, Number, Time) && EXT2Commit(Volume);
    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Symlink(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, CONST CHAR8 *Target, UINT32 Time, UINT32 *Number)
{
    BOOLEAN Result = FALSE;
    UINT32  NewNumber;

    if (Number != NULL)
    {
        *Number = 0;
    }

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (Name == NULL || Target == NULL || Number == NULL)
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else if (EXT2Begin(Volume) && EXT2SymlinkEntry(Volume, Parent, Name, Target, Time, &NewNumber) && EXT2Commit(Volume))
    {
        *Number = NewNumber;
        Result = TRUE;
    }

    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2ReadLink(EXT2_VOLUME *Volume, UINT32 Number, CHAR8 *Buffer, UINT32 Capacity, UINT32 *Length)
{
    EXT2_INODE Inode;
    BOOLEAN    Result = FALSE;

    if (Length != NULL)
    {
        *Length = 0;
    }

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (Buffer == NULL || Length == NULL || Capacity == 0)
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else
    {
        Result = EXT2ReadInode(Volume, Number, &Inode) && EXT2ReadLinkData(Volume, &Inode, Buffer, Capacity, Length);

        if (Result)
        {
            Result = EXT2TouchAccess(Volume, Number, &Inode);
        }
    }

    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2SetCredentials(EXT2_VOLUME *Volume, UINT32 UID, UINT32 GID)
{
    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Volume->UID = UID;
    Volume->GID = GID;
    return EXT2Leave(Volume, TRUE);
}

STATIC BOOLEAN EXT2SetAttributes(EXT2_VOLUME *Volume, UINT32 Number, UINT16 Mode, UINT32 UID, UINT32 GID, BOOLEAN Owner, UINT32 Time)
{
    EXT2_INODE Inode;
    BOOLEAN    Result = FALSE;

    if (Number != 2 && Number < Volume->FirstInode)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (EXT2Begin(Volume) && EXT2ReadInode(Volume, Number, &Inode))
    {
        if (EXT2Load32(Inode.Raw + 32) != 0 || (EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) == EXT2_MODE_SYMLINK)
        {
            EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }
        else if ((Owner && Volume->UID != 0) || (!Owner && Volume->UID != 0 && Volume->UID != EXT2InodeUID(&Inode)))
        {
            EXT2Fail(Volume, EXT2_ERROR_PERMISSION);
        }
        else if (!Owner && (Mode & ~07777U) != 0)
        {
            EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
        }
        else
        {
            if (Owner)
            {
                EXT2InodeOwner(&Inode, UID == UINT32_MAX ? EXT2InodeUID(&Inode) : UID, GID == UINT32_MAX ? EXT2InodeGID(&Inode) : GID);
                EXT2Store16(Inode.Raw, EXT2Load16(Inode.Raw) & ~06000U);
            }
            else
            {
                if (Volume->UID != 0 && Volume->GID != EXT2InodeGID(&Inode))
                {
                    Mode &= ~02000U;
                }

                EXT2Store16(Inode.Raw, (EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) | Mode);
            }

            EXT2Store32(Inode.Raw + 12, Time);
            Result = EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2Commit(Volume);
        }
    }

    EXT2Abort(Volume);
    return Result;
}

BOOLEAN EXT2SetMode(EXT2_VOLUME *Volume, UINT32 Number, UINT16 Mode, UINT32 Time)
{
    BOOLEAN Result;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = EXT2SetAttributes(Volume, Number, Mode, 0, 0, FALSE, Time);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2SetOwner(EXT2_VOLUME *Volume, UINT32 Number, UINT32 UID, UINT32 GID, UINT32 Time)
{
    BOOLEAN Result;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    Result = EXT2SetAttributes(Volume, Number, 0, UID, GID, TRUE, Time);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2LookupChild(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 *Number, EXT2_INODE *Inode)
{
    EXT2_INODE Directory;
    BOOLEAN    Result = FALSE;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (Name == NULL || Number == NULL || Inode == NULL)
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }
    else
    {
        Result = EXT2ReadInode(Volume, Parent, &Directory) && EXT2Access(Volume, &Directory, 1) && EXT2FindEntry(Volume, &Directory, Name, Number) && EXT2ReadInode(Volume, *Number, Inode);
    }

    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2ReadDirectory(EXT2_VOLUME *Volume, UINT32 Number, UINT64 *Cursor, CHAR8 *Name, UINT32 *Child, EXT2_INODE *Inode, BOOLEAN *End)
{
    EXT2_INODE Directory;
    UINT8      Buffer[EXT2_MAX_BLOCK_SIZE];
    UINT32     Found;
    BOOLEAN    Result = FALSE;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (Cursor == NULL || Name == NULL || Child == NULL || Inode == NULL || End == NULL)
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
        goto Done;
    }

    *End = FALSE;

    if (!EXT2ReadInode(Volume, Number, &Directory) || !EXT2Access(Volume, &Directory, 5) || !EXT2FindEntry(Volume, &Directory, (CONST CHAR8 *) ".", &Found))
    {
        goto Done;
    }

    if (*Cursor > EXT2InodeSize(&Directory))
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
        goto Done;
    }

    while (*Cursor < EXT2InodeSize(&Directory))
    {
        UINT32 Block;
        UINT32 Within = (UINT32) (*Cursor % Volume->BlockSize);
        UINT32 Offset = 0;

        if (!EXT2MapBlock(Volume, &Directory, *Cursor / Volume->BlockSize, FALSE, &Block) || !EXT2ReadMetadataBlock(Volume, Block, Buffer))
        {
            goto Done;
        }

        while (Offset < Within)
        {
            Offset += EXT2Load16(Buffer + Offset + 4);
        }

        if (Offset != Within)
        {
            EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
            goto Done;
        }

        *Cursor += EXT2Load16(Buffer + Within + 4);
        Found = EXT2Load32(Buffer + Within);

        if (Found != 0)
        {
            UINT32 Length = Volume->FileType ? Buffer[Within + 6] : EXT2Load16(Buffer + Within + 6);

            KMemCpy(Name, Buffer + Within + 8, Length);
            Name[Length] = 0;
            *Child = Found;
            Result = EXT2ReadInode(Volume, Found, Inode) && EXT2TouchAccess(Volume, Number, &Directory);
            goto Done;
        }
    }

    *End = TRUE;
    Result = TRUE;
Done:
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2Touch(EXT2_VOLUME *Volume, UINT32 Number, UINT32 Time)
{
    EXT2_INODE Inode;
    BOOLEAN    Result = FALSE;

    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }

    if (EXT2Begin(Volume) && EXT2ReadInode(Volume, Number, &Inode))
    {
        if (EXT2Load32(Inode.Raw + 32) != 0)
        {
            EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }
        else if (Volume->UID == 0 || Volume->UID == EXT2InodeUID(&Inode) || EXT2Access(Volume, &Inode, 2))
        {
            EXT2Store32(Inode.Raw + 8, Time);
            EXT2Store32(Inode.Raw + 12, Time);
            EXT2Store32(Inode.Raw + 16, Time);
            Result = EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2Commit(Volume);
        }
    }

    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}

BOOLEAN EXT2SetTimes(EXT2_VOLUME *Volume, UINT32 Number, CONST TIME_SPEC Times[2], UINT32 Time)
{
    EXT2_INODE Inode;
    BOOLEAN Result = FALSE;
    if (!EXT2Enter(Volume))
    {
        return FALSE;
    }
    if (EXT2Begin(Volume) && EXT2ReadInode(Volume, Number, &Inode))
    {
        BOOLEAN Current = Times[0].Nanoseconds == LOS_UTIME_NOW && Times[1].Nanoseconds == LOS_UTIME_NOW;
        if (EXT2Load32(Inode.Raw + 32) != 0)
        {
            EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }
        else if (Volume->UID != 0 && Volume->UID != EXT2InodeUID(&Inode) && (!Current || !EXT2Access(Volume, &Inode, 2)))
        {
            EXT2Fail(Volume, EXT2_ERROR_PERMISSION);
        }
        else
        {
            for (UINTN Index = 0; Index < 2; Index++)
            {
                if (Times[Index].Nanoseconds != LOS_UTIME_OMIT)
                {
                    UINT32 Value = Times[Index].Nanoseconds == LOS_UTIME_NOW ? Time : (UINT32) Times[Index].Seconds;
                    EXT2Store32(Inode.Raw + (Index == 0 ? 8 : 16), Value);
                }
            }
            EXT2Store32(Inode.Raw + 12, Time);
            Result = EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2Commit(Volume);
        }
    }
    EXT2Abort(Volume);
    return EXT2Leave(Volume, Result);
}
