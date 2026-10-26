// Kernel/Source/FS/EXT2/EXT2Superblock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2Superblock.h>
#include <FS/EXT2/EXT2BlockGroup.h>
#include <Memory/PMM.h>

BOOLEAN EXT2LoadSuperblock(EXT2_VOLUME *Volume)
{
    UINT8 *Super = Volume->Superblock;
    UINT32 Revision;
    UINT32 Incompat;
    UINT32 ROCompat;
    UINT32 Compat;
    UINT32 Log;

    if (!VirtIOBlockReadSync(Volume->Device, Volume->Partition.FirstLBA + 2, 2, Volume->DMABuffer))
    {
        return EXT2Fail(Volume, EXT2_ERROR_IO);
    }

    KMemCpy(Super, Volume->DMABuffer, 1024);
    Revision = EXT2Load32(Super + 76);
    Log = EXT2Load32(Super + 24);

    if (EXT2Load16(Super + 56) != 0xEF53 || Log > 2 || Revision > 1)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    Incompat = Revision == 0 ? 0 : EXT2Load32(Super + 96);
    ROCompat = Revision == 0 ? 0 : EXT2Load32(Super + 100);
    Compat = Revision == 0 ? 0 : EXT2Load32(Super + 92);

    if ((Incompat & ~2U) != 0 || (ROCompat & ~3U) != 0 || (Compat & 4U) != 0 || (Volume->Writable && ((ROCompat & ~3U) != 0 || EXT2Load32(Super + 72) != 0 || (Compat & ~0x38U) != 0 || EXT2Load16(Super + 58) != 1)))
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    Volume->BlockSize = 1024U << Log;
    Volume->BlockCount = EXT2Load32(Super + 4);
    Volume->InodeCount = EXT2Load32(Super);
    Volume->FirstDataBlock = EXT2Load32(Super + 20);
    Volume->BlocksPerGroup = EXT2Load32(Super + 32);
    Volume->InodesPerGroup = EXT2Load32(Super + 40);
    Volume->InodeSize = Revision == 0 ? 128 : EXT2Load16(Super + 88);
    Volume->FirstInode = Revision == 0 ? 11 : EXT2Load32(Super + 84);
    Volume->FileType = (Incompat & 2) != 0;

    if (Volume->FirstDataBlock != (Log == 0 ? 1U : 0U) || Volume->BlockCount <= Volume->FirstDataBlock || Volume->InodeCount < 11 || Volume->FirstInode < 11 || Volume->FirstInode > Volume->InodeCount || Volume->BlocksPerGroup == 0 || Volume->BlocksPerGroup > Volume->BlockSize * 8 || Volume->InodesPerGroup == 0 || Volume->InodesPerGroup > Volume->BlockSize * 8 || Volume->InodeSize < 128 || Volume->InodeSize > Volume->BlockSize || (Volume->InodeSize & (Volume->InodeSize - 1)) != 0 || EXT2Load32(Super + 28) != Log || EXT2Load32(Super + 36) != Volume->BlocksPerGroup || (UINT64) Volume->BlockCount * (Volume->BlockSize / 512) > Volume->Partition.SectorCount || EXT2Load32(Super + 12) > Volume->BlockCount || EXT2Load32(Super + 16) > Volume->InodeCount)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    Volume->GroupCount = (UINT32) (((UINT64) Volume->BlockCount - Volume->FirstDataBlock + Volume->BlocksPerGroup - 1) / Volume->BlocksPerGroup);

    if (((UINT64) Volume->InodeCount + Volume->InodesPerGroup - 1) / Volume->InodesPerGroup != Volume->GroupCount || Volume->InodesPerGroup % (Volume->BlockSize / Volume->InodeSize) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    Volume->DescriptorStart = Volume->FirstDataBlock + 1;
    Volume->DescriptorBlocks = (UINT32) (((UINT64) Volume->GroupCount * 32 + Volume->BlockSize - 1) / Volume->BlockSize);
    return TRUE;
}

BOOLEAN EXT2WriteSuperblock(EXT2_VOLUME *Volume, CONST UINT8 *Superblock)
{
    EXT2InvalidateReadCache(Volume);
    KMemCpy(Volume->DMABuffer, Superblock, 1024);

    if (!VirtIOBlockWriteSync(Volume->Device, Volume->Partition.FirstLBA + 2, 2, Volume->DMABuffer))
    {
        return EXT2Fail(Volume, EXT2_ERROR_IO);
    }

    return TRUE;
}

BOOLEAN EXT2ValidateGroups(EXT2_VOLUME *Volume)
{
    UINT64 FreeBlocks = 0;
    UINT64 FreeInodes = 0;
    UINT8  Bitmap[EXT2_MAX_BLOCK_SIZE];
    UINT32 TableBlocks = (UINT32) (((UINT64) Volume->InodesPerGroup * Volume->InodeSize + Volume->BlockSize - 1) / Volume->BlockSize);

    for (UINT32 Group = 0; Group < Volume->GroupCount; Group++)
    {
        EXT2_BLOCK_GROUP Descriptor;
        UINT32           Start = Volume->FirstDataBlock + Group * Volume->BlocksPerGroup;
        UINT32           Count = Volume->BlockCount - Start;
        UINT32           Inodes = Volume->InodeCount - Group * Volume->InodesPerGroup;
        UINT32           Reserved = EXT2GroupHasSuperblock(Volume, Group) ? 1 + Volume->DescriptorBlocks + ((EXT2Load32(Volume->Superblock + 92) & 0x10) ? EXT2Load16(Volume->Superblock + 206) : 0) : 0;
        UINT32           Actual = 0;

        if (Count > Volume->BlocksPerGroup)
        {
            Count = Volume->BlocksPerGroup;
        }

        if (Inodes > Volume->InodesPerGroup)
        {
            Inodes = Volume->InodesPerGroup;
        }

        if (!EXT2ReadBlockGroup(Volume, Group, &Descriptor))
        {
            return FALSE;
        }

        if (Reserved > Count || Descriptor.BlockBitmap < Start + Reserved || Descriptor.BlockBitmap >= Start + Count || Descriptor.InodeBitmap < Start + Reserved || Descriptor.InodeBitmap >= Start + Count || Descriptor.BlockBitmap == Descriptor.InodeBitmap || Descriptor.InodeTable < Start + Reserved || Descriptor.InodeTable >= Start + Count || TableBlocks > Start + Count - Descriptor.InodeTable || (Descriptor.BlockBitmap >= Descriptor.InodeTable && Descriptor.BlockBitmap < Descriptor.InodeTable + TableBlocks) || (Descriptor.InodeBitmap >= Descriptor.InodeTable && Descriptor.InodeBitmap < Descriptor.InodeTable + TableBlocks) || Descriptor.UsedDirectories > Inodes)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        if (!EXT2ReadBlock(Volume, Descriptor.BlockBitmap, Bitmap))
        {
            return FALSE;
        }

        for (UINT32 Bit = 0; Bit < Volume->BlockSize * 8; Bit++)
        {
            BOOLEAN Set = (Bitmap[Bit / 8] & (1U << (Bit % 8))) != 0;
            UINT32  Block = Start + Bit;

            if (!Set && (Bit >= Count || Bit < Reserved || Block == Descriptor.BlockBitmap || Block == Descriptor.InodeBitmap || (Block >= Descriptor.InodeTable && Block < Descriptor.InodeTable + TableBlocks)))
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }

            if (!Set && Bit < Count)
            {
                Actual++;
            }
        }

        if (Actual != Descriptor.FreeBlocks)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        FreeBlocks += Actual;
        Actual = 0;

        if (!EXT2ReadBlock(Volume, Descriptor.InodeBitmap, Bitmap))
        {
            return FALSE;
        }

        for (UINT32 Bit = 0; Bit < Volume->BlockSize * 8; Bit++)
        {
            BOOLEAN Set = (Bitmap[Bit / 8] & (1U << (Bit % 8))) != 0;

            if (!Set && (Bit >= Inodes || (Group == 0 && Bit + 1 < Volume->FirstInode)))
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }

            if (!Set && Bit < Inodes)
            {
                Actual++;
            }
        }

        if (Actual != Descriptor.FreeInodes)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        FreeInodes += Actual;
    }

    if (FreeBlocks != EXT2Load32(Volume->Superblock + 12) || FreeInodes != EXT2Load32(Volume->Superblock + 16))
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    return TRUE;
}
