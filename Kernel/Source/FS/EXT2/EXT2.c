// Kernel/Source/FS/EXT2/EXT2.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2.h>
#include <FS/EXT2/EXT2Superblock.h>
#include <FS/EXT2/EXT2Inode.h>
#include <FS/EXT2/EXT2Directory.h>
#include <FS/EXT2/EXT2File.h>
#include <Memory/PMM.h>

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
    for (EXT2_CHANGE *Change = Volume->Changes; Change != NULL; Change = Change->Next)
    {
        if (Change->Block == Block)
        {
            KMemCpy(Buffer, Change->Data, Volume->BlockSize);
            return TRUE;
        }
    }

    return EXT2DiskBlock(Volume, Block, Buffer, FALSE);
}

UINT8 *EXT2EditBlock(EXT2_VOLUME *Volume, UINT32 Block)
{
    EXT2_CHANGE *Change;

    if (!Volume->Transaction || !Volume->Writable || Volume->Poisoned)
    {
        EXT2Fail(Volume, EXT2_ERROR_READ_ONLY);
        return NULL;
    }

    for (Change = Volume->Changes; Change != NULL; Change = Change->Next)
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

    Change = KAlloc(sizeof(*Change));

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
    Volume->ChangeCount++;
    return Change->Data;
}

BOOLEAN EXT2Begin(EXT2_VOLUME *Volume)
{
    if (!Volume->Writable || Volume->Poisoned || Volume->Transaction)
    {
        return EXT2Fail(Volume, EXT2_ERROR_READ_ONLY);
    }

    Volume->Transaction = TRUE;
    return TRUE;
}

VOID EXT2Abort(EXT2_VOLUME *Volume)
{
    while (Volume->Changes != NULL)
    {
        EXT2_CHANGE *Change = Volume->Changes;

        Volume->Changes = Change->Next;
        KFree(Change);
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

    if (!__atomic_compare_exchange_n(&Volume->Busy, &Expected, 1, FALSE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    {
        return FALSE;
    }

    Volume->Error = EXT2_ERROR_NONE;

    if (Volume->Poisoned)
    {
        Volume->Error = EXT2_ERROR_IO;
        __atomic_store_n(&Volume->Busy, 0, __ATOMIC_RELEASE);
        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN EXT2Leave(EXT2_VOLUME *Volume, BOOLEAN Result)
{
    __atomic_store_n(&Volume->Busy, 0, __ATOMIC_RELEASE);
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

    Volume->DMABuffer = KAllocPagesBelow(1, 0x100000000ULL);

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

        KMemFreePages(Volume->DMABuffer, 1);
        Volume->DMABuffer = NULL;
        return FALSE;
    }

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

    if (!__atomic_compare_exchange_n(&Volume->Busy, &Expected, 1, FALSE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    {
        return FALSE;
    }

    EXT2Abort(Volume);
    KMemFreePages(Volume->DMABuffer, 1);
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
                EXT2InodeOwner(&Inode, UID, GID);
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
