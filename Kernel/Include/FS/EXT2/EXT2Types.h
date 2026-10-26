// Kernel/Include/FS/EXT2/EXT2Types.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Disk/GPT.h>

#define EXT2_ROOT_INODE        2U
#define EXT2_MODE_DIRECTORY    0x4000U
#define EXT2_MODE_SYMLINK      0xA000U
#define EXT2_MODE_FILE         0x8000U
#define EXT2_MODE_TYPE         0xF000U
#define EXT2_MAX_BLOCK_SIZE    4096U
#define EXT2_TRANSACTION_LIMIT 32768U
#define EXT2_CHANGE_BUCKETS    4096U
#define EXT2_READ_CACHE_BLOCKS 16U
#define EXT2_READ_DMA_PAGES    16U

typedef struct
{
    UINT8 Raw[128];
} EXT2_INODE;

typedef struct
{
    UINT32 BlockBitmap;
    UINT32 InodeBitmap;
    UINT32 InodeTable;
    UINT16 FreeBlocks;
    UINT16 FreeInodes;
    UINT16 UsedDirectories;
} EXT2_BLOCK_GROUP;

typedef struct EXT2_CHANGE
{
    struct EXT2_CHANGE *Next;
    struct EXT2_CHANGE *HashNext;
    UINT32              Block;
    UINT8               Data[];
} EXT2_CHANGE;

typedef struct
{
    UINT32  Block;
    BOOLEAN Valid;
    UINT8   Data[EXT2_MAX_BLOCK_SIZE];
} EXT2_READ_CACHE;

typedef enum
{
    EXT2_ERROR_NONE,
    EXT2_ERROR_ARGUMENT,
    EXT2_ERROR_IO,
    EXT2_ERROR_CORRUPT,
    EXT2_ERROR_UNSUPPORTED,
    EXT2_ERROR_READ_ONLY,
    EXT2_ERROR_NOT_FOUND,
    EXT2_ERROR_EXISTS,
    EXT2_ERROR_NO_SPACE,
    EXT2_ERROR_MEMORY,
    EXT2_ERROR_BUSY,
    EXT2_ERROR_LIMIT,
    EXT2_ERROR_PERMISSION,
    EXT2_ERROR_NOT_EMPTY,
    EXT2_ERROR_LOOP,
    EXT2_ERROR_CLOCK
} EXT2_ERROR;

typedef struct
{
    VIRTIO_BLOCK_DEVICE *Device;
    GPT_PARTITION        Partition;
    UINT8                Superblock[1024];
    UINT8               *DMABuffer;
    UINT32               DMAPages;
    UINT32               BlockSize;
    UINT32               BlockCount;
    UINT32               InodeCount;
    UINT32               FirstDataBlock;
    UINT32               BlocksPerGroup;
    UINT32               InodesPerGroup;
    UINT32               GroupCount;
    UINT32               InodeSize;
    UINT32               FirstInode;
    UINT32               DescriptorStart;
    UINT32               DescriptorBlocks;
    UINT32               ChangeCount;
    UINT32               UID;
    UINT32               GID;
    EXT2_CHANGE         *Changes;
    EXT2_CHANGE        **ChangeBuckets;
    EXT2_READ_CACHE     *ReadCache;
    UINT32               ReadCacheNext;
    VOLATILE UINT32      Busy;
    EXT2_ERROR           Error;
    BOOLEAN              Mounted;
    BOOLEAN              Writable;
    BOOLEAN              Poisoned;
    BOOLEAN              Transaction;
    BOOLEAN              FileType;
} EXT2_VOLUME;

UINT16  EXT2Load16(CONST UINT8 *Data);
UINT32  EXT2Load32(CONST UINT8 *Data);
VOID    EXT2Store16(UINT8 *Data, UINT16 Value);
VOID    EXT2Store32(UINT8 *Data, UINT32 Value);
BOOLEAN EXT2Fail(EXT2_VOLUME *Volume, EXT2_ERROR Error);
BOOLEAN EXT2ReadBlock(EXT2_VOLUME *Volume, UINT32 Block, VOID *Buffer);
BOOLEAN EXT2ReadBlocks(EXT2_VOLUME *Volume, UINT32 Block, UINT32 Count, VOID *Buffer);
BOOLEAN EXT2ReadMetadataBlock(EXT2_VOLUME *Volume, UINT32 Block, VOID *Buffer);
VOID    EXT2InvalidateReadCache(EXT2_VOLUME *Volume);
UINT8  *EXT2EditBlock(EXT2_VOLUME *Volume, UINT32 Block);
BOOLEAN EXT2Begin(EXT2_VOLUME *Volume);
BOOLEAN EXT2Commit(EXT2_VOLUME *Volume);
VOID    EXT2Abort(EXT2_VOLUME *Volume);
BOOLEAN EXT2DataBlockValid(EXT2_VOLUME *Volume, UINT32 Block);

BOOLEAN EXT2TouchAccess(EXT2_VOLUME *Volume, UINT32 Number, EXT2_INODE *Inode);
