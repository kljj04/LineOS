// Kernel/Source/FS/EXT2/EXT2File.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2File.h>
#include <FS/EXT2/EXT2Inode.h>
#include <Memory/PMM.h>

BOOLEAN EXT2ReadData(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength)
{
    UINT64 Size = EXT2InodeSize(Inode);
    UINT8  BlockBuffer[EXT2_MAX_BLOCK_SIZE];

    *ReadLength = 0;

    if (Offset >= Size)
    {
        return TRUE;
    }

    if (Length > Size - Offset)
    {
        Length = (UINT32) (Size - Offset);
    }
    while (*ReadLength < Length)
    {
        UINT32 Physical;
        UINT32 Within = (UINT32) (Offset % Volume->BlockSize);
        UINT32 Count = Volume->BlockSize - Within;

        if (Count > Length - *ReadLength)
        {
            Count = Length - *ReadLength;
        }

        if (!EXT2MapBlock(Volume, Inode, Offset / Volume->BlockSize, FALSE, &Physical))
        {
            return FALSE;
        }

        if (Physical == 0)
        {
            KMemSet((UINT8 *) Buffer + *ReadLength, 0, Count);
        }
        else
        {
            if (!EXT2ReadBlock(Volume, Physical, BlockBuffer))
            {
                return FALSE;
            }

            KMemCpy((UINT8 *) Buffer + *ReadLength, BlockBuffer + Within, Count);
        }

        *ReadLength += Count;
        Offset += Count;
    }

    return TRUE;
}

BOOLEAN EXT2WriteData(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Offset, CONST VOID *Buffer, UINT32 Length)
{
    UINT64 OldSize = EXT2InodeSize(Inode);
    UINT64 End;
    UINT32 Done = 0;

    if (Offset > EXT2MaximumFileSize(Volume) || Length > EXT2MaximumFileSize(Volume) - Offset || OldSize > EXT2MaximumFileSize(Volume))
    {
        return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
    }

    End = Offset + Length;

    if (Offset > OldSize && OldSize % Volume->BlockSize != 0)
    {
        UINT32 Physical;

        if (!EXT2MapBlock(Volume, Inode, OldSize / Volume->BlockSize, FALSE, &Physical))
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

            KMemSet(Data + OldSize % Volume->BlockSize, 0, Volume->BlockSize - OldSize % Volume->BlockSize);
        }
    }
    while (Done < Length)
    {
        UINT32 Physical;
        UINT32 Within = (UINT32) (Offset % Volume->BlockSize);
        UINT32 Count = Volume->BlockSize - Within;
        UINT8 *Data;

        if (Count > Length - Done)
        {
            Count = Length - Done;
        }

        if (!EXT2MapBlock(Volume, Inode, Offset / Volume->BlockSize, TRUE, &Physical))
        {
            return FALSE;
        }

        Data = EXT2EditBlock(Volume, Physical);

        if (Data == NULL)
        {
            return FALSE;
        }

        KMemCpy(Data + Within, (CONST UINT8 *) Buffer + Done, Count);
        Done += Count;
        Offset += Count;
    }

    if (End > OldSize)
    {
        EXT2Store32(Inode->Raw + 4, (UINT32) End);

        if ((EXT2Load16(Inode->Raw) & EXT2_MODE_TYPE) == EXT2_MODE_FILE)
        {
            UINT8 *Super;

            EXT2Store32(Inode->Raw + 108, (UINT32) (End >> 32));

            if (End > 0x7FFFFFFF)
            {
                Super = EXT2EditBlock(Volume, 1024 / Volume->BlockSize);

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
    }

    return TRUE;
}
