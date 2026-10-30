// Kernel/Include/FS/VFS/VFSTypes.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define VFS_OPEN_ACCMODE   3U
#define VFS_OPEN_CREATE    0x40U
#define VFS_OPEN_EXCLUSIVE 0x80U
#define VFS_OPEN_TRUNCATE  0x200U
#define VFS_OPEN_APPEND    0x400U

#define VFS_MAX_PARTITIONS     32U
#define VFS_MAX_PATH           4096U
#define VFS_MAX_NAME           255U
#define VFS_MAX_PARTITION_NAME 31U
#define VFS_INVALID_PARTITION  UINT32_MAX
#define VFS_MODE_DIRECTORY     0x4000U
#define VFS_MODE_SYMLINK       0xA000U
#define VFS_MODE_TYPE          0xF000U

typedef enum
{
    VFS_OK,
    VFS_NOT_FOUND,
    VFS_PERMISSION,
    VFS_INVALID,
    VFS_UNSUPPORTED,
    VFS_BUSY,
    VFS_IO,
    VFS_READ_ONLY,
    VFS_EXISTS,
    VFS_NO_SPACE,
    VFS_LIMIT,
    VFS_LOOP,
    VFS_NOT_EMPTY,
    VFS_CROSS_PARTITION,
    VFS_CLOCK_ERROR,
    VFS_NOT_DIRECTORY,
    VFS_IS_DIRECTORY,
    VFS_NAME_TOO_LONG,
    VFS_NO_MEMORY
} VFS_ERROR;

typedef struct
{
    UINT32  UID;
    UINT32  GID;
    BOOLEAN Administrator;
} VFS_CREDENTIALS;

typedef struct
{
    UINT64 NamespaceGeneration;
    UINT32 Number;
    UINT32 Partition;
    UINT16 Mode;
    UINT64 Size;
    UINT32 UID;
    UINT32 GID;
    UINT32 AccessTime;
    UINT32 ChangeTime;
    UINT32 ModifyTime;
} VFS_NODE;

typedef struct
{
    CHAR8    Name[256];
    VFS_NODE Node;
    BOOLEAN  End;
} VFS_DIRECTORY_ENTRY;

typedef enum
{
    VFS_MUTATE_CREATE,
    VFS_MUTATE_REMOVE,
    VFS_MUTATE_RENAME,
    VFS_MUTATE_TRUNCATE,
    VFS_MUTATE_LINK,
    VFS_MUTATE_SYMLINK,
    VFS_MUTATE_MODE,
    VFS_MUTATE_OWNER,
    VFS_MUTATE_TOUCH,
    VFS_MUTATE_TIMES
} VFS_MUTATION_TYPE;

typedef struct
{
    VFS_MUTATION_TYPE Type;
    UINT32            Parent;
    UINT32            OtherParent;
    UINT32            Number;
    CONST CHAR8      *Name;
    CONST CHAR8      *OtherName;
    CONST CHAR8      *Target;
    UINT64            Size;
    UINT16            Mode;
    UINT32            UID;
    UINT32            GID;
    BOOLEAN           Directory;
    INT64             TimeSeconds[2];
    INT64             TimeNanoseconds[2];
} VFS_MUTATION;

typedef struct
{
    VFS_ERROR (*Root)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_NODE *Node);
    VFS_ERROR (*Child)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Parent, CONST CHAR8 *Name, VFS_NODE *Node);
    VFS_ERROR (*ReadLink)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, CHAR8 *Target, UINT32 *Length);
    VFS_ERROR (*ReadDirectory)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 *Cursor, VFS_DIRECTORY_ENTRY *Entry);
    VFS_ERROR (*Read)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength);
    VFS_ERROR (*Write)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 Offset, CONST VOID *Buffer, UINT32 Length);
    VFS_ERROR (*Mutate)(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_MUTATION *Mutation);
} VFS_BACKEND_OPERATIONS;

typedef struct
{
    CHAR8                         Name[32];
    UINT32                        GPTIndex;
    VOID                         *Backend;
    CONST VFS_BACKEND_OPERATIONS *Operations;
    BOOLEAN                       Registered;
} VFS_PARTITION;

typedef struct
{
    VFS_PARTITION   Partitions[VFS_MAX_PARTITIONS];
    VOLATILE UINT32 Busy;
    UINT64          IRQFlags;
    UINT64          NamespaceGeneration;
    UINT32          RootPartition;
    UINT32          DevicePartition;
} VFS_REGISTRY;

typedef struct
{
    VFS_REGISTRY   *Registry;
    VFS_CREDENTIALS Credentials;
    UINT32          Partition;
    CHAR8           Cwd[VFS_MAX_PATH];
} VFS_CONTEXT;

typedef struct
{
    VFS_CONTEXT *Session;
    VFS_CONTEXT  Context;
    BOOLEAN      Active;
} VFS_COMMAND;

typedef struct
{
    UINT32   Partition;
    VFS_NODE Node;
    CHAR8    Path[VFS_MAX_PATH];
} VFS_RESOLVED;
