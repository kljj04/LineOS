// Scripts/Validation/LinuxSyscalls/Generation.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <FS/EXT2/EXT2Inode.h>
#include <FS/EXT2/EXT2BlockGroup.h>

STATIC UINT8 Block[4096];
BOOLEAN      EXT2InodeAllocated(EXT2_VOLUME *Volume, UINT32 Number)
{
    return TRUE;
}
BOOLEAN EXT2ReadBlockGroup(EXT2_VOLUME *Volume, UINT32 Group, EXT2_BLOCK_GROUP *Descriptor)
{
    memset(Descriptor, 0, sizeof(*Descriptor));
    Descriptor->InodeTable = 5;
    return TRUE;
}
UINT8 *EXT2EditBlock(EXT2_VOLUME *Volume, UINT32 Number)
{
    return Number == 5 ? Block : NULL;
}
BOOLEAN EXT2Fail(EXT2_VOLUME *Volume, EXT2_ERROR Error)
{
    Volume->Error = Error;
    return FALSE;
}
UINT32 EXT2Load32(CONST UINT8 *Data)
{
    UINT32 Value;
    memcpy(&Value, Data, 4);
    return Value;
}
VOID EXT2Store32(UINT8 *Data, UINT32 Value)
{
    memcpy(Data, &Value, 4);
}
VOID *KMemCpy(VOID *Destination, CONST VOID *Source, UINTN Size)
{
    return memcpy(Destination, Source, Size);
}
VOID *KMemSet(VOID *Destination, UINT8 Value, UINTN Size)
{
    return memset(Destination, Value, Size);
}
INT32 main(VOID)
{
    EXT2_VOLUME Volume = {.BlockSize = 4096, .InodeSize = 256, .InodesPerGroup = 32};
    EXT2_INODE  Inode  = {0};
    assert(EXT2WriteInode(&Volume, 1, &Inode, TRUE));
    assert(EXT2Load32(Block + 100) == 1);
    EXT2Store32(Inode.Raw + 100, 1);
    assert(EXT2WriteInode(&Volume, 1, &Inode, FALSE));
    assert(EXT2Load32(Block + 100) == 1);
    memset(&Inode, 0, sizeof(Inode));
    assert(EXT2WriteInode(&Volume, 1, &Inode, TRUE));
    assert(EXT2Load32(Block + 100) == 2);
    EXT2Store32(Block + 100, UINT32_MAX);
    assert(!EXT2WriteInode(&Volume, 1, &Inode, TRUE));
    assert(EXT2Load32(Block + 100) == UINT32_MAX);
    puts("EXT2 inode generation persistence/reuse/overflow tests PASS");
    return 0;
}
