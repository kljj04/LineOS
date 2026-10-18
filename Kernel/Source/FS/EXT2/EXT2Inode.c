// Kernel/Source/FS/EXT2/EXT2Inode.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2Inode.h>
#include <FS/EXT2/EXT2BlockGroup.h>
#include <FS/EXT2/EXT2Bitmap.h>
#include <Memory/PMM.h>

BOOLEAN EXT2ReadInode(EXT2_VOLUME *Volume, UINT32 Number, EXT2_INODE *Inode)
{
    EXT2_BLOCK_GROUP Descriptor;
    UINT8            Buffer[EXT2_MAX_BLOCK_SIZE];
    UINT64           Offset;

    if (!EXT2InodeAllocated(Volume, Number) || !EXT2ReadBlockGroup(Volume, (Number - 1) / Volume->InodesPerGroup, &Descriptor))
    {
        return FALSE;
    }

    Offset = (UINT64) ((Number - 1) % Volume->InodesPerGroup) * Volume->InodeSize;

    if (!EXT2ReadBlock(Volume, Descriptor.InodeTable + (UINT32) (Offset / Volume->BlockSize), Buffer))
    {
        return FALSE;
    }

    KMemCpy(Inode->Raw, Buffer + Offset % Volume->BlockSize, 128);

    if (EXT2Load16(Inode->Raw + 26) == 0 || EXT2Load32(Inode->Raw + 20) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    return TRUE;
}

BOOLEAN EXT2WriteInode(EXT2_VOLUME *Volume, UINT32 Number, CONST EXT2_INODE *Inode, BOOLEAN New)
{
    EXT2_BLOCK_GROUP Descriptor;
    UINT64           Offset;
    UINT8           *Data;

    if (!EXT2InodeAllocated(Volume, Number) || !EXT2ReadBlockGroup(Volume, (Number - 1) / Volume->InodesPerGroup, &Descriptor))
    {
        return FALSE;
    }

    Offset = (UINT64) ((Number - 1) % Volume->InodesPerGroup) * Volume->InodeSize;
    Data = EXT2EditBlock(Volume, Descriptor.InodeTable + (UINT32) (Offset / Volume->BlockSize));

    if (Data == NULL)
    {
        return FALSE;
    }

    Data += Offset % Volume->BlockSize;

    if (New)
    {
        KMemSet(Data, 0, Volume->InodeSize);
    }

    KMemCpy(Data, Inode->Raw, 128);
    return TRUE;
}

UINT64 EXT2InodeSize(CONST EXT2_INODE *Inode)
{
    UINT64 Size = EXT2Load32(Inode->Raw + 4);

    if ((EXT2Load16(Inode->Raw) & EXT2_MODE_TYPE) == EXT2_MODE_FILE)
    {
        Size |= (UINT64) EXT2Load32(Inode->Raw + 108) << 32;
    }

    return Size;
}

STATIC BOOLEAN EXT2NewInodeBlock(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT32 *Block)
{
    UINT32 Sectors = EXT2Load32(Inode->Raw + 28);

    if (Sectors > UINT32_MAX - Volume->BlockSize / 512)
    {
        return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
    }

    if (!EXT2AllocateBlock(Volume, Block))
    {
        return FALSE;
    }

    EXT2Store32(Inode->Raw + 28, Sectors + Volume->BlockSize / 512);
    return TRUE;
}

BOOLEAN EXT2MapBlock(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Logical, BOOLEAN Allocate, UINT32 *Physical)
{
    UINT32 PerBlock = Volume->BlockSize / 4;
    UINT64 Span = PerBlock;
    UINT32 Level;
    UINT32 Root;
    UINT32 Block;
    UINT32 Seen[3] = {0};
    UINT8  Buffer[EXT2_MAX_BLOCK_SIZE];

    if (EXT2Load32(Inode->Raw + 32) & (0x4U | 0x80000U | 0x10000000U))
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (Logical < 12)
    {
        Root = (UINT32) Logical;
        Level = 0;
    }
    else
    {
        Logical -= 12;

        for (Level = 1; Level <= 3; Level++)
        {
            if (Logical < Span)
            {
                break;
            }

            Logical -= Span;
            Span *= PerBlock;
        }

        if (Level > 3)
        {
            return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
        }

        Root = 11 + Level;
    }

    Block = EXT2Load32(Inode->Raw + 40 + Root * 4);

    if (Block == 0 && Allocate)
    {
        if (!EXT2NewInodeBlock(Volume, Inode, &Block))
        {
            return FALSE;
        }

        EXT2Store32(Inode->Raw + 40 + Root * 4, Block);
    }

    for (UINT32 Depth = 0; Depth < Level; Depth++)
    {
        UINT32 Index;
        UINT32 Child;
        UINT8 *Data;

        if (Block == 0)
        {
            *Physical = 0;
            return TRUE;
        }

        if (!EXT2DataBlockValid(Volume, Block) || !EXT2ReadBlock(Volume, Block, Buffer))
        {
            return FALSE;
        }

        Seen[Depth] = Block;
        Span /= PerBlock;
        Index = (UINT32) (Logical / Span);
        Logical %= Span;
        Child = EXT2Load32(Buffer + Index * 4);

        if (Child == 0 && Allocate)
        {
            if (!EXT2NewInodeBlock(Volume, Inode, &Child))
            {
                return FALSE;
            }

            Data = EXT2EditBlock(Volume, Block);

            if (Data == NULL)
            {
                return FALSE;
            }

            EXT2Store32(Data + Index * 4, Child);
        }

        Block = Child;

        for (UINT32 Previous = 0; Previous <= Depth; Previous++)
        {
            if (Block != 0 && Block == Seen[Previous])
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }
        }
    }

    if (Block != 0 && !EXT2DataBlockValid(Volume, Block))
    {
        return FALSE;
    }

    *Physical = Block;
    return TRUE;
}

UINT32 EXT2InodeUID(CONST EXT2_INODE *Inode)
{
    return EXT2Load16(Inode->Raw + 2) | (UINT32) EXT2Load16(Inode->Raw + 120) << 16;
}

UINT32 EXT2InodeGID(CONST EXT2_INODE *Inode)
{
    return EXT2Load16(Inode->Raw + 24) | (UINT32) EXT2Load16(Inode->Raw + 122) << 16;
}

VOID EXT2InodeOwner(EXT2_INODE *Inode, UINT32 UID, UINT32 GID)
{
    EXT2Store16(Inode->Raw + 2, (UINT16) UID);
    EXT2Store16(Inode->Raw + 120, (UINT16) (UID >> 16));
    EXT2Store16(Inode->Raw + 24, (UINT16) GID);
    EXT2Store16(Inode->Raw + 122, (UINT16) (GID >> 16));
}

BOOLEAN EXT2Access(EXT2_VOLUME *Volume, CONST EXT2_INODE *Inode, UINT16 Mask)
{
    UINT16 Mode = EXT2Load16(Inode->Raw);
    UINT16 Shift = Volume->UID == EXT2InodeUID(Inode) ? 6 : Volume->GID == EXT2InodeGID(Inode) ? 3 : 0;

    if (Volume->UID == 0 || (((Mode >> Shift) & Mask) == Mask))
    {
        return TRUE;
    }

    return EXT2Fail(Volume, EXT2_ERROR_PERMISSION);
}

STATIC BOOLEAN EXT2PruneTree(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT32 *Block, UINT32 Level, UINT64 Start, UINT64 Span, UINT64 Keep)
{
    UINT8  *Buffer;
    BOOLEAN Empty = TRUE;
    UINT32  Original = *Block;
    UINT32  Sectors;

    if (Original == 0 || Start + Span <= Keep)
    {
        return TRUE;
    }

    if (!EXT2DataBlockValid(Volume, Original))
    {
        return FALSE;
    }

    if (Level != 0)
    {
        Buffer = EXT2EditBlock(Volume, Original);

        if (Buffer == NULL)
        {
            return FALSE;
        }

        Span /= Volume->BlockSize / 4;

        for (UINT32 Index = 0; Index < Volume->BlockSize / 4; Index++)
        {
            UINT32 Child = EXT2Load32(Buffer + Index * 4);

            if (!EXT2PruneTree(Volume, Inode, &Child, Level - 1, Start + Index * Span, Span, Keep))
            {
                return FALSE;
            }

            EXT2Store32(Buffer + Index * 4, Child);
            Empty = Empty && Child == 0;
        }

        if (!Empty)
        {
            return TRUE;
        }
    }

    Sectors = EXT2Load32(Inode->Raw + 28);

    if (Sectors < Volume->BlockSize / 512 || !EXT2FreeBlock(Volume, Original))
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    EXT2Store32(Inode->Raw + 28, Sectors - Volume->BlockSize / 512);
    *Block = 0;
    return TRUE;
}

BOOLEAN EXT2ResizeInode(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Size)
{
    UINT64 OldSize = EXT2InodeSize(Inode);
    UINT64 Keep = (Size + Volume->BlockSize - 1) / Volume->BlockSize;
    UINT64 Start = 0;
    UINT64 Span = 1;
    UINT16 Type = EXT2Load16(Inode->Raw) & EXT2_MODE_TYPE;

    if (Size > EXT2MaximumFileSize(Volume) || OldSize > EXT2MaximumFileSize(Volume) || (Type != EXT2_MODE_FILE && Size > UINT32_MAX) || EXT2Load32(Inode->Raw + 32) != 0 || EXT2Load32(Inode->Raw + 104) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (Type == EXT2_MODE_SYMLINK && EXT2Load32(Inode->Raw + 28) == 0)
    {
        if (Size != 0 || OldSize > 60)
        {
            return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
        }

        KMemSet(Inode->Raw + 40, 0, 60);
        EXT2Store32(Inode->Raw + 4, 0);
        return TRUE;
    }

    if (Size <= OldSize)
    {
        for (UINT32 Index = 0; Index < 15; Index++)
        {
            UINT32 Block = EXT2Load32(Inode->Raw + 40 + Index * 4);
            UINT32 Level = Index < 12 ? 0 : Index - 11;

            if (Index >= 12)
            {
                Span *= Volume->BlockSize / 4;
            }

            if (!EXT2PruneTree(Volume, Inode, &Block, Level, Start, Span, Keep))
            {
                return FALSE;
            }

            EXT2Store32(Inode->Raw + 40 + Index * 4, Block);
            Start += Span;
        }
    }

    if ((Size < OldSize ? Size : OldSize) % Volume->BlockSize != 0)
    {
        UINT64 Boundary = Size < OldSize ? Size : OldSize;
        UINT32 Physical;

        if (!EXT2MapBlock(Volume, Inode, Boundary / Volume->BlockSize, FALSE, &Physical))
        {
            return FALSE;
        }

        if (Physical != 0)
        {
            UINT8 *Data = EXT2EditBlock(Volume, Physical);

            if (Data == NULL)
            {
                return FALSE;
            }

            KMemSet(Data + Boundary % Volume->BlockSize, 0, Volume->BlockSize - Boundary % Volume->BlockSize);
        }
    }

    EXT2Store32(Inode->Raw + 4, (UINT32) Size);

    if (Type == EXT2_MODE_FILE)
    {
        EXT2Store32(Inode->Raw + 108, (UINT32) (Size >> 32));

        if (Size > 0x7FFFFFFF)
        {
            UINT8 *Super = EXT2EditBlock(Volume, 1024 / Volume->BlockSize);

            if (Super == NULL)
            {
                return FALSE;
            }

            Super += 1024 % Volume->BlockSize;

            if (EXT2Load32(Super + 76) == 0)
            {
                return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
            }

            EXT2Store32(Super + 100, EXT2Load32(Super + 100) | 2);
        }
    }

    return TRUE;
}

STATIC BOOLEAN EXT2AuditTree(EXT2_VOLUME *Volume, UINT32 Block, UINT32 Level, UINT8 *Seen, UINT32 *Count, BOOLEAN Metadata)
{
    UINT8 *Buffer;

    if (Block == 0)
    {
        return TRUE;
    }

    if (Metadata)
    {
        EXT2_BLOCK_GROUP Descriptor;
        UINT8            Bitmap[EXT2_MAX_BLOCK_SIZE];
        UINT32           Bit;

        if (Block < Volume->FirstDataBlock || Block >= Volume->BlockCount || !EXT2ReadBlockGroup(Volume, (Block - Volume->FirstDataBlock) / Volume->BlocksPerGroup, &Descriptor) || !EXT2ReadBlock(Volume, Descriptor.BlockBitmap, Bitmap))
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        Bit = (Block - Volume->FirstDataBlock) % Volume->BlocksPerGroup;

        if ((Bitmap[Bit / 8] & (1U << (Bit % 8))) == 0)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }
    }

    if ((!Metadata && !EXT2DataBlockValid(Volume, Block)) || (Seen[Block / 8] & (1U << (Block % 8))) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    Seen[Block / 8] |= (UINT8) (1U << (Block % 8));
    (*Count)++;

    if (Level == 0)
    {
        return TRUE;
    }

    Buffer = KAlloc(Volume->BlockSize);

    if (Buffer == NULL)
    {
        return EXT2Fail(Volume, EXT2_ERROR_MEMORY);
    }

    if (!EXT2ReadBlock(Volume, Block, Buffer))
    {
        KFree(Buffer);
        return FALSE;
    }

    for (UINT32 Index = 0; Index < Volume->BlockSize / 4; Index++)
    {
        if (!EXT2AuditTree(Volume, EXT2Load32(Buffer + Index * 4), Level - 1, Seen, Count, Metadata))
        {
            KFree(Buffer);
            return FALSE;
        }
    }

    KFree(Buffer);
    return TRUE;
}

BOOLEAN EXT2ValidateOwnership(EXT2_VOLUME *Volume)
{
    UINT8  *Seen = KAlloc(((UINT64) Volume->BlockCount + 7) / 8);
    UINT8   Bitmap[EXT2_MAX_BLOCK_SIZE];
    BOOLEAN Result = FALSE;

    if (Seen == NULL)
    {
        return EXT2Fail(Volume, EXT2_ERROR_MEMORY);
    }

    KMemSet(Seen, 0, ((UINT64) Volume->BlockCount + 7) / 8);

    {
        EXT2_BLOCK_GROUP Descriptor;

        if (!EXT2ReadBlockGroup(Volume, 0, &Descriptor))
        {
            goto Done;
        }

        for (UINT32 Number = 1; Number < Volume->FirstInode; Number++)
        {
            UINT64     Offset = (UINT64) (Number - 1) * Volume->InodeSize;
            UINT8      Buffer[EXT2_MAX_BLOCK_SIZE];
            EXT2_INODE Inode;
            UINT32     Count = 0;

            if (Number == 2)
            {
                continue;
            }

            if (!EXT2ReadBlock(Volume, Descriptor.InodeTable + (UINT32) (Offset / Volume->BlockSize), Buffer))
            {
                goto Done;
            }

            KMemCpy(Inode.Raw, Buffer + Offset % Volume->BlockSize, 128);

            if (Number == 7 && (EXT2Load32(Volume->Superblock + 92) & 0x10) != 0)
            {
                for (UINT32 Index = 0; Index < 15; Index++)
                {
                    if (!EXT2AuditTree(Volume, EXT2Load32(Inode.Raw + 40 + Index * 4), Index < 12 ? 0 : Index - 11, Seen, &Count, TRUE))
                    {
                        goto Done;
                    }
                }

                if ((UINT64) Count * (Volume->BlockSize / 512) != EXT2Load32(Inode.Raw + 28))
                {
                    EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                    goto Done;
                }
            }
            else if (EXT2Load32(Inode.Raw + 28) != 0 || EXT2Load32(Inode.Raw + 104) != 0)
            {
                EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
                goto Done;
            }
        }
    }

    for (UINT32 Group = 0; Group < Volume->GroupCount; Group++)
    {
        EXT2_BLOCK_GROUP Descriptor;

        if (!EXT2ReadBlockGroup(Volume, Group, &Descriptor) || !EXT2ReadBlock(Volume, Descriptor.InodeBitmap, Bitmap))
        {
            goto Done;
        }

        for (UINT32 Bit = 0; Bit < Volume->InodesPerGroup; Bit++)
        {
            EXT2_INODE Inode;
            UINT32     Number = Group * Volume->InodesPerGroup + Bit + 1;
            UINT32     Count = 0;
            UINT16     Type;

            if (Number > Volume->InodeCount || (Number < Volume->FirstInode && Number != 2) || (Bitmap[Bit / 8] & (1U << (Bit % 8))) == 0)
            {
                continue;
            }

            if (!EXT2ReadInode(Volume, Number, &Inode))
            {
                goto Done;
            }

            Type = EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE;

            if (EXT2Load32(Inode.Raw + 104) != 0 || EXT2Load32(Inode.Raw + 112) != 0 || EXT2Load32(Inode.Raw + 36) != 0 || Inode.Raw[116] != 0 || Inode.Raw[117] != 0 || (EXT2Load32(Inode.Raw + 32) & ~0x30FFU) != 0)
            {
                EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
                goto Done;
            }

            if (Type != EXT2_MODE_FILE && Type != EXT2_MODE_DIRECTORY && Type != EXT2_MODE_SYMLINK)
            {
                if (EXT2Load32(Inode.Raw + 28) != 0)
                {
                    EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
                    goto Done;
                }

                continue;
            }

            if (Type == EXT2_MODE_SYMLINK && EXT2Load32(Inode.Raw + 28) == 0)
            {
                if (EXT2InodeSize(&Inode) > 60)
                {
                    EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                    goto Done;
                }

                continue;
            }

            for (UINT32 Index = 0; Index < 15; Index++)
            {
                if (!EXT2AuditTree(Volume, EXT2Load32(Inode.Raw + 40 + Index * 4), Index < 12 ? 0 : Index - 11, Seen, &Count, FALSE))
                {
                    goto Done;
                }
            }

            if ((UINT64) Count * (Volume->BlockSize / 512) != EXT2Load32(Inode.Raw + 28))
            {
                EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                goto Done;
            }
        }
    }

    Result = TRUE;
Done:
    KFree(Seen);
    return Result;
}

UINT64 EXT2MaximumFileSize(EXT2_VOLUME *Volume)
{
    UINT64 Count = Volume->BlockSize / 4;
    UINT64 Limit = (12 + Count + Count * Count + Count * Count * Count) * Volume->BlockSize;
    UINT64 SectorLimit = (UINT64) UINT32_MAX * 512;

    return Limit < SectorLimit ? Limit : SectorLimit;
}
