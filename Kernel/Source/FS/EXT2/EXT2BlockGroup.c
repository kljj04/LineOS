// Kernel/Source/FS/EXT2/EXT2BlockGroup.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2BlockGroup.h>

BOOLEAN EXT2GroupHasSuperblock(EXT2_VOLUME *Volume, UINT32 Group)
{
    CONST UINT32 Bases[3] = {3, 5, 7};

    if ((EXT2Load32(Volume->Superblock + 100) & 1) == 0 || Group <= 1)
    {
        return TRUE;
    }

    for (UINT32 Index = 0; Index < 3; Index++)
    {
        UINT32 Value = Group;

        while (Value % Bases[Index] == 0)
        {
            Value /= Bases[Index];
        }

        if (Value == 1)
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN EXT2ReadBlockGroup(EXT2_VOLUME *Volume, UINT32 Group, EXT2_BLOCK_GROUP *Descriptor)
{
    UINT8  Buffer[EXT2_MAX_BLOCK_SIZE];
    UINT8 *Data;

    if (Group >= Volume->GroupCount)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    if (!EXT2ReadBlock(Volume, Volume->DescriptorStart + Group / (Volume->BlockSize / 32), Buffer))
    {
        return FALSE;
    }

    Data = Buffer + (Group % (Volume->BlockSize / 32)) * 32;
    Descriptor->BlockBitmap = EXT2Load32(Data);
    Descriptor->InodeBitmap = EXT2Load32(Data + 4);
    Descriptor->InodeTable = EXT2Load32(Data + 8);
    Descriptor->FreeBlocks = EXT2Load16(Data + 12);
    Descriptor->FreeInodes = EXT2Load16(Data + 14);
    Descriptor->UsedDirectories = EXT2Load16(Data + 16);
    return TRUE;
}

BOOLEAN EXT2UpdateBlockGroup(EXT2_VOLUME *Volume, UINT32 Group, CONST EXT2_BLOCK_GROUP *Descriptor)
{
    UINT8 *Data;

    if (Group >= Volume->GroupCount)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    Data = EXT2EditBlock(Volume, Volume->DescriptorStart + Group / (Volume->BlockSize / 32));

    if (Data == NULL)
    {
        return FALSE;
    }

    Data += (Group % (Volume->BlockSize / 32)) * 32;
    EXT2Store16(Data + 12, Descriptor->FreeBlocks);
    EXT2Store16(Data + 14, Descriptor->FreeInodes);
    EXT2Store16(Data + 16, Descriptor->UsedDirectories);
    return TRUE;
}

BOOLEAN EXT2DataBlockValid(EXT2_VOLUME *Volume, UINT32 Block)
{
    EXT2_BLOCK_GROUP Descriptor;
    UINT8            Bitmap[EXT2_MAX_BLOCK_SIZE];
    UINT32           Group;
    UINT32           Start;
    UINT32           Reserved;
    UINT32           TableBlocks = (UINT32) (((UINT64) Volume->InodesPerGroup * Volume->InodeSize + Volume->BlockSize - 1) / Volume->BlockSize);

    if (Block < Volume->FirstDataBlock || Block >= Volume->BlockCount)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    Group = (Block - Volume->FirstDataBlock) / Volume->BlocksPerGroup;
    Start = Volume->FirstDataBlock + Group * Volume->BlocksPerGroup;
    Reserved = EXT2GroupHasSuperblock(Volume, Group) ? 1 + Volume->DescriptorBlocks + ((EXT2Load32(Volume->Superblock + 92) & 0x10) ? EXT2Load16(Volume->Superblock + 206) : 0) : 0;

    if (!EXT2ReadBlockGroup(Volume, Group, &Descriptor))
    {
        return FALSE;
    }

    if (Block < Start + Reserved || Block == Descriptor.BlockBitmap || Block == Descriptor.InodeBitmap || (Block >= Descriptor.InodeTable && Block < Descriptor.InodeTable + TableBlocks))
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    if (!EXT2ReadBlock(Volume, Descriptor.BlockBitmap, Bitmap))
    {
        return FALSE;
    }

    Block -= Start;

    if ((Bitmap[Block / 8] & (1U << (Block % 8))) == 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    return TRUE;
}
