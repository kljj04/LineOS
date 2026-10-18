// Kernel/Source/Disk/GPT.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Disk/GPT.h>
#include <Memory/PMM.h>

STATIC UINT32 GPTRead32(CONST UINT8 *Data)
{
    return (UINT32) Data[0] | (UINT32) Data[1] << 8 | (UINT32) Data[2] << 16 | (UINT32) Data[3] << 24;
}

STATIC UINT64 GPTRead64(CONST UINT8 *Data)
{
    return GPTRead32(Data) | (UINT64) GPTRead32(Data + 4) << 32;
}

STATIC UINT32 GPTCRC32(UINT32 CRC, CONST UINT8 *Data, UINT32 Size)
{
    for (UINT32 Index = 0; Index < Size; Index++)
    {
        CRC ^= Data[Index];

        for (UINT32 Bit = 0; Bit < 8; Bit++)
        {
            CRC = (CRC >> 1) ^ (0xEDB88320U & (0U - (CRC & 1)));
        }
    }

    return CRC;
}

BOOLEAN GPTReadPartition(VIRTIO_BLOCK_DEVICE *Device, UINT32 Index, GPT_PARTITION *Partition)
{
    UINT8  *Buffer;
    UINT8   Entry[128];
    UINT64  Capacity;
    UINT64  EntryLBA;
    UINT64  Bytes;
    UINT64  Sectors;
    UINT64  First;
    UINT64  Last;
    UINT64  Offset;
    UINT32  Count;
    UINT32  Size;
    UINT32  CRC;
    UINT32  ExpectedCRC;
    BOOLEAN Result = FALSE;

    if (Device == NULL || Partition == NULL || Index == 0)
    {
        return FALSE;
    }

    Capacity = VirtIOBlockGetCapacity(Device);

    if (Capacity < 6)
    {
        return FALSE;
    }

    Buffer = KAllocPagesBelow(1, 0x100000000ULL);

    if (Buffer == NULL)
    {
        return FALSE;
    }

    if (!VirtIOBlockReadSync(Device, 1, 1, Buffer) || GPTRead64(Buffer) != 0x5452415020494645ULL || GPTRead32(Buffer + 8) != 0x10000 || GPTRead32(Buffer + 12) < 92 || GPTRead32(Buffer + 12) > 512 || GPTRead32(Buffer + 20) != 0 || GPTRead64(Buffer + 24) != 1 || GPTRead64(Buffer + 32) != Capacity - 1)
    {
        goto Done;
    }

    ExpectedCRC = GPTRead32(Buffer + 16);
    KMemSet(Buffer + 16, 0, 4);

    if (~GPTCRC32(0xFFFFFFFFU, Buffer, GPTRead32(Buffer + 12)) != ExpectedCRC)
    {
        goto Done;
    }

    First = GPTRead64(Buffer + 40);
    Last = GPTRead64(Buffer + 48);
    EntryLBA = GPTRead64(Buffer + 72);
    Count = GPTRead32(Buffer + 80);
    Size = GPTRead32(Buffer + 84);
    ExpectedCRC = GPTRead32(Buffer + 88);

    if (Index > Count || Size < 128 || Size > 4096 || (Size & (Size - 1)) != 0 || Count > 16384)
    {
        goto Done;
    }

    Bytes = (UINT64) Count * Size;
    Sectors = (Bytes + 511) / 512;

    if (EntryLBA < 2 || EntryLBA >= Capacity || Sectors > Capacity - EntryLBA || First < EntryLBA + Sectors || First > Last || Last >= Capacity - 1 || Sectors > Capacity - 1 - Last)
    {
        goto Done;
    }

    CRC = 0xFFFFFFFFU;
    KMemSet(Entry, 0, sizeof(Entry));

    for (UINT64 Sector = 0; Sector < Sectors; Sector++)
    {
        UINT32 Length = Bytes - Sector * 512 < 512 ? (UINT32) (Bytes - Sector * 512) : 512;

        if (!VirtIOBlockReadSync(Device, EntryLBA + Sector, 1, Buffer))
        {
            goto Done;
        }

        CRC = GPTCRC32(CRC, Buffer, Length);
        Offset = (UINT64) (Index - 1) * Size;

        if (Offset / 512 == Sector)
        {
            KMemCpy(Entry, Buffer + Offset % 512, 128);
        }
    }

    if (~CRC != ExpectedCRC)
    {
        goto Done;
    }

    {
        UINT8 Nonzero = 0;

        for (UINT32 Byte = 0; Byte < 16; Byte++)
        {
            Nonzero |= Entry[Byte];
        }

        if (Nonzero == 0)
        {
            goto Done;
        }
    }

    Partition->FirstLBA = GPTRead64(Entry + 32);
    Offset = GPTRead64(Entry + 40);

    if (Partition->FirstLBA < First || Offset < Partition->FirstLBA || Offset > Last)
    {
        goto Done;
    }

    Partition->SectorCount = Offset - Partition->FirstLBA + 1;
    Partition->Attributes = GPTRead64(Entry + 48);
    KMemCpy(Partition->TypeGUID, Entry, 16);
    Result = TRUE;
Done:
    KMemFreePages(Buffer, 1);
    return Result;
}
