// Kernel/Source/FS/VFS/VFSEXT2.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/VFS/VFSEXT2.h>
#include <FS/EXT2/EXT2Inode.h>

STATIC VFS_ERROR VFSEXT2Error(EXT2_ERROR Error)
{
    switch (Error)
    {
    case EXT2_ERROR_NONE:
        return VFS_OK;
    case EXT2_ERROR_NOT_FOUND:
        return VFS_NOT_FOUND;
    case EXT2_ERROR_PERMISSION:
        return VFS_PERMISSION;
    case EXT2_ERROR_ARGUMENT:
        return VFS_INVALID;
    case EXT2_ERROR_UNSUPPORTED:
        return VFS_UNSUPPORTED;
    case EXT2_ERROR_READ_ONLY:
        return VFS_READ_ONLY;
    case EXT2_ERROR_EXISTS:
        return VFS_EXISTS;
    case EXT2_ERROR_NO_SPACE:
        return VFS_NO_SPACE;
    case EXT2_ERROR_MEMORY:
    case EXT2_ERROR_LIMIT:
        return VFS_LIMIT;
    case EXT2_ERROR_BUSY:
        return VFS_BUSY;
    case EXT2_ERROR_LOOP:
        return VFS_LOOP;
    case EXT2_ERROR_NOT_EMPTY:
        return VFS_NOT_EMPTY;
    case EXT2_ERROR_CLOCK:
        return VFS_CLOCK_ERROR;
    default:
        return VFS_IO;
    }
}

STATIC VOID VFSEXT2Node(UINT32 Number, CONST EXT2_INODE *Inode, VFS_NODE *Node)
{
    Node->Number = Number;
    Node->Partition = VFS_INVALID_PARTITION;
    Node->Mode = EXT2Load16(Inode->Raw);
    Node->Size = EXT2InodeSize(Inode);
    Node->UID = EXT2InodeUID(Inode);
    Node->GID = EXT2InodeGID(Inode);
    Node->AccessTime = EXT2Load32(Inode->Raw + 8);
    Node->ChangeTime = EXT2Load32(Inode->Raw + 12);
    Node->ModifyTime = EXT2Load32(Inode->Raw + 16);
}

STATIC BOOLEAN VFSEXT2Begin(EXT2_VOLUME *Volume, CONST VFS_CREDENTIALS *Credentials, UINT32 *UID, UINT32 *GID)
{
    *UID = Volume->UID;
    *GID = Volume->GID;
    return EXT2SetCredentials(Volume, Credentials->UID, Credentials->GID);
}

STATIC VFS_ERROR VFSEXT2End(EXT2_VOLUME *Volume, UINT32 UID, UINT32 GID, BOOLEAN Result)
{
    VFS_ERROR Error = Result ? VFS_OK : Volume->Error == EXT2_ERROR_NONE ? VFS_BUSY : VFSEXT2Error(Volume->Error);

    if (!EXT2SetCredentials(Volume, UID, GID) && Error == VFS_OK)
    {
        return VFS_BUSY;
    }

    return Error;
}

STATIC VFS_ERROR VFSEXT2Root(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_NODE *Node)
{
    EXT2_VOLUME *Volume = Backend;
    EXT2_INODE   Inode;
    UINT32       Number;
    UINT32       UID;
    UINT32       GID;
    BOOLEAN      Result;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    Result = EXT2Lookup(Volume, (CONST CHAR8 *) "/", &Number, &Inode);

    if (Result)
    {
        VFSEXT2Node(Number, &Inode, Node);
    }

    return VFSEXT2End(Volume, UID, GID, Result);
}

STATIC VFS_ERROR VFSEXT2Child(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Parent, CONST CHAR8 *Name, VFS_NODE *Node)
{
    EXT2_VOLUME *Volume = Backend;
    EXT2_INODE   Inode;
    UINT32       Number;
    UINT32       UID;
    UINT32       GID;
    BOOLEAN      Result;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    Result = EXT2LookupChild(Volume, Parent, Name, &Number, &Inode);

    if (Result)
    {
        VFSEXT2Node(Number, &Inode, Node);
    }

    return VFSEXT2End(Volume, UID, GID, Result);
}

STATIC VFS_ERROR VFSEXT2ReadLink(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, CHAR8 *Target, UINT32 *Length)
{
    EXT2_VOLUME *Volume = Backend;
    UINT32       UID;
    UINT32       GID;
    BOOLEAN      Result;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    Result = EXT2ReadLink(Volume, Number, Target, VFS_MAX_PATH, Length);
    return VFSEXT2End(Volume, UID, GID, Result);
}

STATIC VFS_ERROR VFSEXT2ReadDirectory(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 *Cursor, VFS_DIRECTORY_ENTRY *Entry)
{
    EXT2_VOLUME *Volume = Backend;
    EXT2_INODE   Inode;
    UINT32       Child;
    UINT32       UID;
    UINT32       GID;
    BOOLEAN      Result;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    Result = EXT2ReadDirectory(Volume, Number, Cursor, Entry->Name, &Child, &Inode, &Entry->End);

    if (Result && !Entry->End)
    {
        VFSEXT2Node(Child, &Inode, &Entry->Node);
    }

    return VFSEXT2End(Volume, UID, GID, Result);
}

STATIC VFS_ERROR VFSEXT2Read(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength)
{
    EXT2_VOLUME *Volume = Backend;
    UINT32       UID;
    UINT32       GID;
    BOOLEAN      Result;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    Result = EXT2ReadFile(Volume, Number, Offset, Buffer, Length, ReadLength);
    return VFSEXT2End(Volume, UID, GID, Result);
}

STATIC VFS_ERROR VFSEXT2Write(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 Offset, CONST VOID *Buffer, UINT32 Length)
{
    EXT2_VOLUME *Volume = Backend;
    UINT32       UID;
    UINT32       GID;
    UINT32       Time;
    BOOLEAN      Result;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    Result = EXT2GetCurrentTime(Volume, &Time) && EXT2WriteFile(Volume, Number, Offset, Buffer, Length, Time);
    return VFSEXT2End(Volume, UID, GID, Result);
}

STATIC VFS_ERROR VFSEXT2Mutate(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_MUTATION *Mutation)
{
    EXT2_VOLUME *Volume = Backend;
    UINT32       UID;
    UINT32       GID;
    UINT32       Time;
    UINT32       Number;
    BOOLEAN      Result = FALSE;

    if (!VFSEXT2Begin(Volume, Credentials, &UID, &GID))
    {
        return VFS_BUSY;
    }

    if (EXT2GetCurrentTime(Volume, &Time))
    {
        switch (Mutation->Type)
        {
        case VFS_MUTATE_CREATE:
            Result = EXT2Create(Volume, Mutation->Parent, Mutation->Name, Mutation->Directory, Mutation->Mode, Time, &Number);
            break;
        case VFS_MUTATE_REMOVE:
            Result = EXT2Remove(Volume, Mutation->Parent, Mutation->Name, Time);
            break;
        case VFS_MUTATE_RENAME:
            Result = EXT2Rename(Volume, Mutation->Parent, Mutation->Name, Mutation->OtherParent, Mutation->OtherName, Time);
            break;
        case VFS_MUTATE_TRUNCATE:
            Result = EXT2Truncate(Volume, Mutation->Number, Mutation->Size, Time);
            break;
        case VFS_MUTATE_LINK:
            Result = EXT2Link(Volume, Mutation->Parent, Mutation->Name, Mutation->Number, Time);
            break;
        case VFS_MUTATE_SYMLINK:
            Result = EXT2Symlink(Volume, Mutation->Parent, Mutation->Name, Mutation->Target, Time, &Number);
            break;
        case VFS_MUTATE_MODE:
            Result = EXT2SetMode(Volume, Mutation->Number, Mutation->Mode, Time);
            break;
        case VFS_MUTATE_TOUCH:
            Result = EXT2Touch(Volume, Mutation->Number, Time);
            break;
        case VFS_MUTATE_OWNER:
            Result = EXT2SetOwner(Volume, Mutation->Number, Mutation->UID, Mutation->GID, Time);
            break;
        default:
            Volume->Error = EXT2_ERROR_ARGUMENT;
            break;
        }
    }

    return VFSEXT2End(Volume, UID, GID, Result);
}

CONST VFS_BACKEND_OPERATIONS VFSEXT2Operations = {VFSEXT2Root, VFSEXT2Child, VFSEXT2ReadLink, VFSEXT2ReadDirectory, VFSEXT2Read, VFSEXT2Write, VFSEXT2Mutate};

VFS_ERROR VFSMountEXT2(VFS_REGISTRY *Registry, CONST CHAR8 *Name, EXT2_VOLUME *Volume, VIRTIO_BLOCK_DEVICE *Device, UINT32 GPTIndex, BOOLEAN Writable, UINT32 *Partition)
{
    GPT_PARTITION GPT;
    VFS_ERROR     Error;
    CONST UINT8   LinuxGUID[16] = {0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4};

    if (Registry == NULL || Volume == NULL || Device == NULL || Name == NULL || Partition == NULL)
    {
        return VFS_INVALID;
    }

    if (Volume->Mounted)
    {
        return VFS_BUSY;
    }

    if (!GPTReadPartition(Device, GPTIndex, &GPT))
    {
        return VFS_IO;
    }

    for (UINT32 Index = 0; Index < 16; Index++)
    {
        if (GPT.TypeGUID[Index] != LinuxGUID[Index])
        {
            return VFS_UNSUPPORTED;
        }
    }

    if (!EXT2Mount(Volume, Device, &GPT, Writable))
    {
        return VFSEXT2Error(Volume->Error);
    }

    Error = VFSRegisterPartition(Registry, Name, GPTIndex, Volume, &VFSEXT2Operations, Partition);

    if (Error != VFS_OK)
    {
        EXT2Unmount(Volume);
    }

    return Error;
}
