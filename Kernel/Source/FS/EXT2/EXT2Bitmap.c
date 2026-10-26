// Kernel/Source/FS/EXT2/EXT2Bitmap.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2Bitmap.h>
#include <FS/EXT2/EXT2BlockGroup.h>
#include <Memory/PMM.h>

BOOLEAN EXT2InodeAllocated(EXT2_VOLUME *Volume, UINT32 Number)
{
    EXT2_BLOCK_GROUP Descriptor;
    UINT8            Bitmap[EXT2_MAX_BLOCK_SIZE];
    UINT32           Bit;

    if (Number == 0 || Number > Volume->InodeCount)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    if (!EXT2ReadBlockGroup(Volume, (Number - 1) / Volume->InodesPerGroup, &Descriptor) || !EXT2ReadMetadataBlock(Volume, Descriptor.InodeBitmap, Bitmap))
    {
        return FALSE;
    }

    Bit = (Number - 1) % Volume->InodesPerGroup;

    if ((Bitmap[Bit / 8] & (1U << (Bit % 8))) == 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_NOT_FOUND);
    }

    return TRUE;
}

STATIC BOOLEAN EXT2Allocate(EXT2_VOLUME *Volume, BOOLEAN Inode, BOOLEAN Directory, UINT32 *Number)
{
    if (!Inode && Volume->UID != 0)
    {
        UINT8 *Super = EXT2EditBlock(Volume, 1024 / Volume->BlockSize);

        if (Super == NULL)
        {
            return FALSE;
        }

        Super += 1024 % Volume->BlockSize;

        if (EXT2Load32(Super + 12) <= EXT2Load32(Super + 8))
        {
            return EXT2Fail(Volume, EXT2_ERROR_NO_SPACE);
        }
    }

    for (UINT32 Group = 0; Group < Volume->GroupCount; Group++)
    {
        EXT2_BLOCK_GROUP Descriptor;
        UINT8           *Bitmap;
        UINT8           *Super;
        UINT32           Count;
        UINT32           Start;
        UINT32           CounterOffset = Inode ? 16 : 12;

        if (!EXT2ReadBlockGroup(Volume, Group, &Descriptor))
        {
            return FALSE;
        }

        if ((Inode ? Descriptor.FreeInodes : Descriptor.FreeBlocks) == 0)
        {
            continue;
        }

        Bitmap = EXT2EditBlock(Volume, Inode ? Descriptor.InodeBitmap : Descriptor.BlockBitmap);

        if (Bitmap == NULL)
        {
            return FALSE;
        }

        Start = Inode ? Group * Volume->InodesPerGroup + 1 : Volume->FirstDataBlock + Group * Volume->BlocksPerGroup;
        Count = Inode ? Volume->InodeCount - Start + 1 : Volume->BlockCount - Start;

        if (Count > (Inode ? Volume->InodesPerGroup : Volume->BlocksPerGroup))
        {
            Count = Inode ? Volume->InodesPerGroup : Volume->BlocksPerGroup;
        }

        for (UINT32 Bit = 0; Bit < Count; Bit++)
        {
            if ((Bitmap[Bit / 8] & (1U << (Bit % 8))) != 0)
            {
                continue;
            }

            if (Inode && Start + Bit < Volume->FirstInode)
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }

            Bitmap[Bit / 8] |= (UINT8) (1U << (Bit % 8));
            Super = EXT2EditBlock(Volume, 1024 / Volume->BlockSize);

            if (Super == NULL)
            {
                return FALSE;
            }

            Super += 1024 % Volume->BlockSize;

            if (EXT2Load32(Super + CounterOffset) == 0)
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }

            EXT2Store32(Super + CounterOffset, EXT2Load32(Super + CounterOffset) - 1);

            if (Inode)
            {
                Descriptor.FreeInodes--;

                if (Directory)
                {
                    if (Descriptor.UsedDirectories == 65535)
                    {
                        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                    }

                    Descriptor.UsedDirectories++;
                }
            }
            else
            {
                UINT8 *Data;

                Descriptor.FreeBlocks--;

                if (!EXT2DataBlockValid(Volume, Start + Bit))
                {
                    return FALSE;
                }

                Data = EXT2EditBlock(Volume, Start + Bit);

                if (Data == NULL)
                {
                    return FALSE;
                }

                KMemSet(Data, 0, Volume->BlockSize);
            }

            if (!EXT2UpdateBlockGroup(Volume, Group, &Descriptor))
            {
                return FALSE;
            }

            *Number = Start + Bit;
            return TRUE;
        }

        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    return EXT2Fail(Volume, EXT2_ERROR_NO_SPACE);
}

BOOLEAN EXT2AllocateBlock(EXT2_VOLUME *Volume, UINT32 *Block)
{
    return EXT2Allocate(Volume, FALSE, FALSE, Block);
}

BOOLEAN EXT2AllocateInode(EXT2_VOLUME *Volume, BOOLEAN Directory, UINT32 *Number)
{
    return EXT2Allocate(Volume, TRUE, Directory, Number);
}

STATIC BOOLEAN EXT2FreeBitmap(EXT2_VOLUME *Volume, UINT32 Number, BOOLEAN Inode, BOOLEAN Directory)
{
    EXT2_BLOCK_GROUP Descriptor;
    UINT32           Group;
    UINT32           Bit;
    UINT8           *Bitmap;
    UINT8           *Super;
    UINT32           Counter = Inode ? 16 : 12;

    if (Inode)
    {
        if (Number < Volume->FirstInode || Number > Volume->InodeCount || !EXT2InodeAllocated(Volume, Number))
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        Group = (Number - 1) / Volume->InodesPerGroup;
        Bit = (Number - 1) % Volume->InodesPerGroup;
    }
    else
    {
        if (!EXT2DataBlockValid(Volume, Number))
        {
            return FALSE;
        }

        Group = (Number - Volume->FirstDataBlock) / Volume->BlocksPerGroup;
        Bit = (Number - Volume->FirstDataBlock) % Volume->BlocksPerGroup;
    }

    if (!EXT2ReadBlockGroup(Volume, Group, &Descriptor))
    {
        return FALSE;
    }

    Bitmap = EXT2EditBlock(Volume, Inode ? Descriptor.InodeBitmap : Descriptor.BlockBitmap);
    Super = EXT2EditBlock(Volume, 1024 / Volume->BlockSize);

    if (Bitmap == NULL || Super == NULL)
    {
        return FALSE;
    }

    Super += 1024 % Volume->BlockSize;

    if ((Bitmap[Bit / 8] & (1U << (Bit % 8))) == 0 || EXT2Load32(Super + Counter) >= (Inode ? Volume->InodeCount : Volume->BlockCount))
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    Bitmap[Bit / 8] &= (UINT8) ~(1U << (Bit % 8));
    EXT2Store32(Super + Counter, EXT2Load32(Super + Counter) + 1);

    if (Inode)
    {
        if (Descriptor.FreeInodes == 65535 || (Directory && Descriptor.UsedDirectories == 0))
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        Descriptor.FreeInodes++;

        if (Directory)
        {
            Descriptor.UsedDirectories--;
        }
    }
    else
    {
        if (Descriptor.FreeBlocks == 65535)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        Descriptor.FreeBlocks++;
    }

    return EXT2UpdateBlockGroup(Volume, Group, &Descriptor);
}

BOOLEAN EXT2FreeBlock(EXT2_VOLUME *Volume, UINT32 Block)
{
    return EXT2FreeBitmap(Volume, Block, FALSE, FALSE);
}

BOOLEAN EXT2FreeInode(EXT2_VOLUME *Volume, UINT32 Number, BOOLEAN Directory)
{
    return EXT2FreeBitmap(Volume, Number, TRUE, Directory);
}
