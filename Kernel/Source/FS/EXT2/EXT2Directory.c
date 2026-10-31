// Kernel/Source/FS/EXT2/EXT2Directory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/EXT2/EXT2Directory.h>
#include <FS/EXT2/EXT2Inode.h>
#include <FS/EXT2/EXT2Bitmap.h>
#include <FS/EXT2/EXT2BlockGroup.h>
#include <FS/EXT2/EXT2File.h>
#include <Memory/PMM.h>

STATIC UINT32 EXT2NameLength(CONST CHAR8 *Name)
{
    UINT32 Length = 0;

    while (Length <= 255 && Name[Length] != 0)
    {
        if (Name[Length] == '/')
        {
            return 0;
        }

        Length++;
    }

    return Length <= 255 ? Length : 0;
}

STATIC BOOLEAN EXT2DirectoryScan(EXT2_VOLUME *Volume, EXT2_INODE *Directory, CONST CHAR8 *Name, UINT32 *Number, UINT32 Need, UINT32 *SlotBlock, UINT32 *SlotOffset)
{
    UINT64 Size = EXT2InodeSize(Directory);
    UINT32 NameLength = EXT2NameLength(Name);
    UINT8  Buffer[EXT2_MAX_BLOCK_SIZE];

    *Number = 0;
    *SlotBlock = 0;
    *SlotOffset = 0;

    if (NameLength == 0 || (EXT2Load16(Directory->Raw) & EXT2_MODE_TYPE) != EXT2_MODE_DIRECTORY)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (Size == 0 || Size % Volume->BlockSize != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    for (UINT64 Logical = 0; Logical < Size / Volume->BlockSize; Logical++)
    {
        UINT32 Physical;

        if (!EXT2MapBlock(Volume, Directory, Logical, FALSE, &Physical))
        {
            return FALSE;
        }

        if (Physical == 0)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        if (!EXT2ReadMetadataBlock(Volume, Physical, Buffer))
        {
            return FALSE;
        }

        for (UINT32 Offset = 0; Offset < Volume->BlockSize;)
        {
            UINT8  *Entry = Buffer + Offset;
            UINT32  Inode;
            UINT32  Record;
            UINT32  Length;
            UINT32  Used;
            BOOLEAN Equal;

            if (Volume->BlockSize - Offset < 8)
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }

            Inode = EXT2Load32(Entry);
            Record = EXT2Load16(Entry + 4);
            Length = Volume->FileType ? Entry[6] : EXT2Load16(Entry + 6);

            if (Record < 8 || (Record & 3) != 0 || Record > Volume->BlockSize - Offset || Length > 255 || Length > Record - 8 || (Volume->FileType && Entry[7] > 7))
            {
                return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
            }

            Used = Inode == 0 ? 0 : (8 + Length + 3) & ~3U;

            if (Inode != 0)
            {
                if (Length == 0 || !EXT2InodeAllocated(Volume, Inode))
                {
                    return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                }

                Equal = Length == NameLength;

                for (UINT32 Index = 0; Index < Length; Index++)
                {
                    if (Entry[8 + Index] == 0 || Entry[8 + Index] == '/')
                    {
                        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                    }

                    if (Index >= NameLength || Entry[8 + Index] != Name[Index])
                    {
                        Equal = FALSE;
                    }
                }

                if (Equal)
                {
                    if (*Number != 0)
                    {
                        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
                    }

                    *Number = Inode;
                }
            }

            if (Need != 0 && *SlotBlock == 0 && Record - Used >= Need)
            {
                *SlotBlock = Physical;
                *SlotOffset = Offset;
            }

            Offset += Record;
        }
    }

    return TRUE;
}

BOOLEAN EXT2FindEntry(EXT2_VOLUME *Volume, EXT2_INODE *Directory, CONST CHAR8 *Name, UINT32 *Number)
{
    UINT32 Block;
    UINT32 Offset;

    if (!EXT2DirectoryScan(Volume, Directory, Name, Number, 0, &Block, &Offset))
    {
        return FALSE;
    }

    if (*Number == 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_NOT_FOUND);
    }

    return TRUE;
}

STATIC VOID EXT2SetEntry(EXT2_VOLUME *Volume, UINT8 *Data, UINT32 Number, UINT16 Record, CONST CHAR8 *Name, UINT8 Type)
{
    UINT32 Length = EXT2NameLength(Name);

    KMemSet(Data, 0, Record);
    EXT2Store32(Data, Number);
    EXT2Store16(Data + 4, Record);
    EXT2Store16(Data + 6, (UINT16) Length);

    if (Volume->FileType)
    {
        Data[7] = Type;
    }

    KMemCpy(Data + 8, Name, Length);
}

BOOLEAN EXT2AddEntry(EXT2_VOLUME *Volume, EXT2_INODE *Directory, CONST CHAR8 *Name, UINT32 Number, UINT8 Type)
{
    UINT32 Length = EXT2NameLength(Name);
    UINT32 Need = (8 + Length + 3) & ~3U;
    UINT32 Existing;
    UINT32 Block;
    UINT32 Offset;
    UINT8 *Data;
    UINT32 Record;

    if (EXT2Load32(Directory->Raw + 32) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (!EXT2DirectoryScan(Volume, Directory, Name, &Existing, Need, &Block, &Offset))
    {
        return FALSE;
    }

    if (Existing != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_EXISTS);
    }

    if (Block == 0)
    {
        UINT64 Size = EXT2InodeSize(Directory);

        if (Size > UINT32_MAX - Volume->BlockSize)
        {
            return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
        }

        if (!EXT2MapBlock(Volume, Directory, Size / Volume->BlockSize, TRUE, &Block))
        {
            return FALSE;
        }

        Data = EXT2EditBlock(Volume, Block);

        if (Data == NULL)
        {
            return FALSE;
        }

        EXT2SetEntry(Volume, Data, Number, (UINT16) Volume->BlockSize, Name, Type);
        EXT2Store32(Directory->Raw + 4, (UINT32) Size + Volume->BlockSize);

        return TRUE;
    }

    Data = EXT2EditBlock(Volume, Block);

    if (Data == NULL)
    {
        return FALSE;
    }

    Data += Offset;
    Record = EXT2Load16(Data + 4);

    if (EXT2Load32(Data) != 0)
    {
        UINT32 Used = (8 + (Volume->FileType ? Data[6] : EXT2Load16(Data + 6)) + 3) & ~3U;

        EXT2Store16(Data + 4, (UINT16) Used);
        Data += Used;
        Record -= Used;
    }

    EXT2SetEntry(Volume, Data, Number, (UINT16) Record, Name, Type);

    return TRUE;
}

BOOLEAN EXT2Resolve(EXT2_VOLUME *Volume, CONST CHAR8 *Path, UINT32 *Number, EXT2_INODE *Inode)
{
    CHAR8  *Pending = KAlloc(8192);
    CHAR8  *Target;
    UINT32  Length = 0;
    UINT32  Position = 0;
    UINT32  Links = 0;
    BOOLEAN Result = FALSE;

    if (Pending == NULL)
    {
        return EXT2Fail(Volume, EXT2_ERROR_MEMORY);
    }

    Target = Pending + 4096;

    while (Length < 4096 && Path[Length] != 0)
    {
        Length++;
    }

    if (Length == 4096 || Path[0] != '/')
    {
        EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);

        goto Done;
    }

    KMemCpy(Pending, Path, Length + 1);
    *Number = 2;

    if (!EXT2ReadInode(Volume, *Number, Inode))
    {
        goto Done;
    }

    for (;;)
    {
        CHAR8      Name[256];
        UINT32     NameLength = 0;
        UINT32     Child;
        EXT2_INODE Next;

        while (Pending[Position] == '/')
        {
            if ((EXT2Load16(Inode->Raw) & EXT2_MODE_TYPE) != EXT2_MODE_DIRECTORY || !EXT2Access(Volume, Inode, 1))
            {
                if (Volume->Error == EXT2_ERROR_NONE)
                {
                    EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
                }

                goto Done;
            }

            Position++;
        }

        if (Pending[Position] == 0)
        {
            Result = TRUE;
            break;
        }

        if (!EXT2Access(Volume, Inode, 1))
        {
            goto Done;
        }

        while (Pending[Position] != 0 && Pending[Position] != '/')
        {
            if (NameLength == 255)
            {
                EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);

                goto Done;
            }

            Name[NameLength++] = Pending[Position++];
        }

        Name[NameLength] = 0;

        if (!EXT2FindEntry(Volume, Inode, Name, &Child) || !EXT2ReadInode(Volume, Child, &Next))
        {
            goto Done;
        }

        if ((EXT2Load16(Next.Raw) & EXT2_MODE_TYPE) == EXT2_MODE_SYMLINK)
        {
            UINT32 TargetLength;
            UINT32 TailLength = Length - Position;

            if (++Links > 40)
            {
                EXT2Fail(Volume, EXT2_ERROR_LOOP);

                goto Done;
            }

            if (!EXT2ReadLinkData(Volume, &Next, Target, 4096, &TargetLength))
            {
                goto Done;
            }

            if (!EXT2TouchAccess(Volume, Child, &Next))
            {
                goto Done;
            }

            if (TargetLength + TailLength >= 4096)
            {
                EXT2Fail(Volume, EXT2_ERROR_LIMIT);

                goto Done;
            }

            KMemCpy(Target + TargetLength, Pending + Position, TailLength + 1);
            Length = TargetLength + TailLength;
            KMemCpy(Pending, Target, Length + 1);
            Position = 0;

            if (Pending[0] == '/')
            {
                *Number = 2;

                if (!EXT2ReadInode(Volume, *Number, Inode))
                {
                    goto Done;
                }
            }
        }
        else
        {
            *Number = Child;
            *Inode = Next;
        }
    }

Done:
    KFree(Pending);

    return Result;
}

BOOLEAN EXT2CreateEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, BOOLEAN Directory, UINT16 Permissions, UINT32 Time, UINT32 *Number)
{
    EXT2_INODE ParentInode;
    EXT2_INODE Inode;
    UINT32     Existing;
    UINT32     SlotBlock;
    UINT32     SlotOffset;
    UINT32     Length = EXT2NameLength(Name);
    UINT16     Links;

    if (Length == 0 || (Length == 1 && Name[0] == '.') || (Length == 2 && Name[0] == '.' && Name[1] == '.') || (Parent != 2 && Parent < Volume->FirstInode))
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (!EXT2ReadInode(Volume, Parent, &ParentInode))
    {
        return FALSE;
    }

    if (EXT2Load32(ParentInode.Raw + 32) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (!EXT2DirectoryScan(Volume, &ParentInode, Name, &Existing, 0, &SlotBlock, &SlotOffset))
    {
        return FALSE;
    }

    if (Existing != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_EXISTS);
    }

    if (!EXT2Access(Volume, &ParentInode, 3))
    {
        return FALSE;
    }

    Links = EXT2Load16(ParentInode.Raw + 26);

    if (Directory && Links >= 32000)
    {
        return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
    }

    if (!EXT2AllocateInode(Volume, Directory, Number))
    {
        return FALSE;
    }

    KMemSet(&Inode, 0, sizeof(Inode));
    EXT2Store16(Inode.Raw, (Directory ? EXT2_MODE_DIRECTORY : EXT2_MODE_FILE) | Permissions);
    EXT2Store16(Inode.Raw + 26, Directory ? 2 : 1);
    EXT2InodeOwner(&Inode, Volume->UID, (EXT2Load16(ParentInode.Raw) & 02000) ? EXT2InodeGID(&ParentInode) : Volume->GID);

    if (Directory && (EXT2Load16(ParentInode.Raw) & 02000))
    {
        EXT2Store16(Inode.Raw, EXT2Load16(Inode.Raw) | 02000);
    }

    EXT2Store32(Inode.Raw + 8, Time);
    EXT2Store32(Inode.Raw + 12, Time);
    EXT2Store32(Inode.Raw + 16, Time);

    if (Directory)
    {
        UINT32 Block;
        UINT8 *Data;

        if (!EXT2MapBlock(Volume, &Inode, 0, TRUE, &Block))
        {
            return FALSE;
        }

        Data = EXT2EditBlock(Volume, Block);

        if (Data == NULL)
        {
            return FALSE;
        }

        EXT2SetEntry(Volume, Data, *Number, 12, (CONST CHAR8 *) ".", 2);
        EXT2SetEntry(Volume, Data + 12, Parent, (UINT16) (Volume->BlockSize - 12), (CONST CHAR8 *) "..", 2);
        EXT2Store32(Inode.Raw + 4, Volume->BlockSize);
        EXT2Store16(ParentInode.Raw + 26, Links + 1);
    }

    EXT2Store32(ParentInode.Raw + 12, Time);
    EXT2Store32(ParentInode.Raw + 16, Time);

    return EXT2WriteInode(Volume, *Number, &Inode, TRUE) && EXT2AddEntry(Volume, &ParentInode, Name, *Number, Directory ? 2 : 1) && EXT2WriteInode(Volume, Parent, &ParentInode, FALSE);
}

STATIC BOOLEAN EXT2MutableName(EXT2_VOLUME *Volume, CONST CHAR8 *Name)
{
    UINT32 Length = Name == NULL ? 0 : EXT2NameLength(Name);

    if (Length == 0 || (Length == 1 && Name[0] == '.') || (Length == 2 && Name[0] == '.' && Name[1] == '.'))
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    return TRUE;
}

STATIC BOOLEAN EXT2EraseEntry(EXT2_VOLUME *Volume, EXT2_INODE *Directory, CONST CHAR8 *Name)
{
    UINT32 Number;
    UINT32 Length = EXT2NameLength(Name);

    if (!EXT2FindEntry(Volume, Directory, Name, &Number))
    {
        return FALSE;
    }

    for (UINT64 Logical = 0; Logical < EXT2InodeSize(Directory) / Volume->BlockSize; Logical++)
    {
        UINT32 Block;
        UINT32 Previous = UINT32_MAX;
        UINT8 *Data;

        if (!EXT2MapBlock(Volume, Directory, Logical, FALSE, &Block))
        {
            return FALSE;
        }

        Data = EXT2EditBlock(Volume, Block);

        if (Data == NULL)
        {
            return FALSE;
        }

        for (UINT32 Offset = 0; Offset < Volume->BlockSize;)
        {
            UINT32  Record = EXT2Load16(Data + Offset + 4);
            UINT32  EntryLength = Volume->FileType ? Data[Offset + 6] : EXT2Load16(Data + Offset + 6);
            BOOLEAN Equal = EXT2Load32(Data + Offset) == Number && EntryLength == Length;

            for (UINT32 Index = 0; Equal && Index < Length; Index++)
            {
                Equal = Data[Offset + 8 + Index] == Name[Index];
            }

            if (Equal)
            {
                if (Previous == UINT32_MAX)
                {
                    EXT2Store32(Data + Offset, 0);
                }
                else
                {
                    EXT2Store16(Data + Previous + 4, (UINT16) (EXT2Load16(Data + Previous + 4) + Record));
                }

                return TRUE;
            }

            Previous = Offset;
            Offset += Record;
        }
    }

    return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
}

STATIC BOOLEAN EXT2EmptyDirectory(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT32 Number, UINT32 Parent)
{
    UINT32 Found;
    UINT8  Buffer[EXT2_MAX_BLOCK_SIZE];

    if (!EXT2FindEntry(Volume, Inode, (CONST CHAR8 *) ".", &Found) || Found != Number || !EXT2FindEntry(Volume, Inode, (CONST CHAR8 *) "..", &Found) || Found != Parent)
    {
        return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
    }

    for (UINT64 Logical = 0; Logical < EXT2InodeSize(Inode) / Volume->BlockSize; Logical++)
    {
        UINT32 Block;

        if (!EXT2MapBlock(Volume, Inode, Logical, FALSE, &Block) || !EXT2ReadMetadataBlock(Volume, Block, Buffer))
        {
            return FALSE;
        }

        for (UINT32 Offset = 0; Offset < Volume->BlockSize; Offset += EXT2Load16(Buffer + Offset + 4))
        {
            UINT32  Length = Volume->FileType ? Buffer[Offset + 6] : EXT2Load16(Buffer + Offset + 6);
            BOOLEAN Dot = Length == 1 && Buffer[Offset + 8] == '.';
            BOOLEAN DotDot = Length == 2 && Buffer[Offset + 8] == '.' && Buffer[Offset + 9] == '.';

            if (EXT2Load32(Buffer + Offset) != 0 && !Dot && !DotDot)
            {
                return EXT2Fail(Volume, EXT2_ERROR_NOT_EMPTY);
            }
        }
    }

    return TRUE;
}

STATIC BOOLEAN EXT2MayDelete(EXT2_VOLUME *Volume, CONST EXT2_INODE *Parent, CONST EXT2_INODE *Inode)
{
    if (EXT2Load32(Parent->Raw + 32) != 0 || EXT2Load32(Inode->Raw + 32) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (!EXT2Access(Volume, Parent, 3))
    {
        return FALSE;
    }

    if ((EXT2Load16(Parent->Raw) & 01000) != 0 && Volume->UID != 0 && Volume->UID != EXT2InodeUID(Parent) && Volume->UID != EXT2InodeUID(Inode))
    {
        return EXT2Fail(Volume, EXT2_ERROR_PERMISSION);
    }

    return TRUE;
}

BOOLEAN EXT2RemoveEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 Time)
{
    EXT2_INODE ParentInode;
    EXT2_INODE Inode;
    UINT32     Number;
    UINT16     Links;
    BOOLEAN    Directory;

    if (!EXT2MutableName(Volume, Name) || !EXT2ReadInode(Volume, Parent, &ParentInode) || !EXT2FindEntry(Volume, &ParentInode, Name, &Number) || !EXT2ReadInode(Volume, Number, &Inode) || !EXT2MayDelete(Volume, &ParentInode, &Inode))
    {
        return FALSE;
    }

    if (Number < Volume->FirstInode || EXT2Load32(Inode.Raw + 104) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    Directory = (EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE) == EXT2_MODE_DIRECTORY;
    Links = EXT2Load16(Inode.Raw + 26);

    if (Directory && (!EXT2EmptyDirectory(Volume, &Inode, Number, Parent) || Links != 2 || EXT2Load16(ParentInode.Raw + 26) < 3))
    {
        if (Volume->Error == EXT2_ERROR_NONE)
        {
            EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        return FALSE;
    }

    if (!EXT2EraseEntry(Volume, &ParentInode, Name))
    {
        return FALSE;
    }

    if (Directory || Links == 1)
    {
        UINT16 Type = EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE;

        if (Type != EXT2_MODE_FILE && Type != EXT2_MODE_DIRECTORY && Type != EXT2_MODE_SYMLINK)
        {
            return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
        }

        if (!EXT2ResizeInode(Volume, &Inode, 0))
        {
            return FALSE;
        }

        KMemSet(&Inode, 0, sizeof(Inode));

        if (!EXT2WriteInode(Volume, Number, &Inode, FALSE) || !EXT2FreeInode(Volume, Number, Directory))
        {
            return FALSE;
        }

        if (Directory)
        {
            EXT2Store16(ParentInode.Raw + 26, EXT2Load16(ParentInode.Raw + 26) - 1);
        }
    }
    else
    {
        EXT2Store16(Inode.Raw + 26, Links - 1);
        EXT2Store32(Inode.Raw + 12, Time);

        if (!EXT2WriteInode(Volume, Number, &Inode, FALSE))
        {
            return FALSE;
        }
    }

    EXT2Store32(ParentInode.Raw + 12, Time);
    EXT2Store32(ParentInode.Raw + 16, Time);

    return EXT2WriteInode(Volume, Parent, &ParentInode, FALSE);
}

BOOLEAN EXT2RenameEntry(EXT2_VOLUME *Volume, UINT32 OldParent, CONST CHAR8 *OldName, UINT32 NewParent, CONST CHAR8 *NewName, UINT32 Time)
{
    EXT2_INODE OldDirectory;
    EXT2_INODE NewDirectory;
    EXT2_INODE Inode;
    EXT2_INODE Target;
    UINT32     Number;
    UINT32     Existing;
    UINT32     SlotBlock;
    UINT32     SlotOffset;
    BOOLEAN    Directory;
    UINT16     Type;

    if (!EXT2MutableName(Volume, OldName) || !EXT2MutableName(Volume, NewName) || !EXT2ReadInode(Volume, OldParent, &OldDirectory) || !EXT2ReadInode(Volume, NewParent, &NewDirectory) || !EXT2FindEntry(Volume, &OldDirectory, OldName, &Number) || !EXT2ReadInode(Volume, Number, &Inode) || !EXT2MayDelete(Volume, &OldDirectory, &Inode) || !EXT2Access(Volume, &NewDirectory, 3))
    {
        return FALSE;
    }

    if (Number < Volume->FirstInode || EXT2Load32(NewDirectory.Raw + 32) != 0)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    Type = EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE;
    Directory = Type == EXT2_MODE_DIRECTORY;

    if (Type != EXT2_MODE_FILE && Type != EXT2_MODE_DIRECTORY && Type != EXT2_MODE_SYMLINK)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (!EXT2DirectoryScan(Volume, &NewDirectory, NewName, &Existing, 0, &SlotBlock, &SlotOffset))
    {
        return FALSE;
    }

    if (Existing == Number)
    {
        return TRUE;
    }

    if (Directory)
    {
        UINT32 Current = NewParent;
        UINT32 Steps;

        for (Steps = 0; Steps < Volume->InodeCount; Steps++)
        {
            EXT2_INODE Ancestor;
            UINT32     Next;

            if (Current == Number)
            {
                return EXT2Fail(Volume, EXT2_ERROR_LOOP);
            }

            if (Current == 2)
            {
                break;
            }

            if (!EXT2ReadInode(Volume, Current, &Ancestor) || !EXT2FindEntry(Volume, &Ancestor, (CONST CHAR8 *) "..", &Next))
            {
                return FALSE;
            }

            Current = Next;
        }

        if (Steps == Volume->InodeCount)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }
    }

    if (Existing != 0)
    {
        if (!EXT2ReadInode(Volume, Existing, &Target))
        {
            return FALSE;
        }

        if (((EXT2Load16(Target.Raw) & EXT2_MODE_TYPE) == EXT2_MODE_DIRECTORY) != Directory)
        {
            return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
        }

        if (!EXT2RemoveEntry(Volume, NewParent, NewName, Time))
        {
            return FALSE;
        }
    }

    if (!EXT2ReadInode(Volume, OldParent, &OldDirectory) || !EXT2EraseEntry(Volume, &OldDirectory, OldName))
    {
        return FALSE;
    }

    if (Directory && OldParent != NewParent)
    {
        UINT32 ParentNumber;

        if (EXT2Load16(OldDirectory.Raw + 26) < 3 || !EXT2FindEntry(Volume, &Inode, (CONST CHAR8 *) "..", &ParentNumber) || ParentNumber != OldParent || !EXT2EraseEntry(Volume, &Inode, (CONST CHAR8 *) "..") || !EXT2AddEntry(Volume, &Inode, (CONST CHAR8 *) "..", NewParent, 2))
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        EXT2Store16(OldDirectory.Raw + 26, EXT2Load16(OldDirectory.Raw + 26) - 1);
        EXT2Store32(Inode.Raw + 16, Time);
    }

    EXT2Store32(OldDirectory.Raw + 12, Time);
    EXT2Store32(OldDirectory.Raw + 16, Time);

    if (!EXT2WriteInode(Volume, OldParent, &OldDirectory, FALSE) || !EXT2ReadInode(Volume, NewParent, &NewDirectory))
    {
        return FALSE;
    }

    if (Directory && OldParent != NewParent)
    {
        if (EXT2Load16(NewDirectory.Raw + 26) >= 32000)
        {
            return EXT2Fail(Volume, EXT2_ERROR_LIMIT);
        }

        EXT2Store16(NewDirectory.Raw + 26, EXT2Load16(NewDirectory.Raw + 26) + 1);
    }

    if (!EXT2AddEntry(Volume, &NewDirectory, NewName, Number, Directory ? 2 : Type == EXT2_MODE_SYMLINK ? 7 : 1))
    {
        return FALSE;
    }

    EXT2Store32(Inode.Raw + 12, Time);
    EXT2Store32(NewDirectory.Raw + 12, Time);
    EXT2Store32(NewDirectory.Raw + 16, Time);

    return EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2WriteInode(Volume, NewParent, &NewDirectory, FALSE);
}

BOOLEAN EXT2LinkEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, UINT32 Number, UINT32 Time)
{
    EXT2_INODE Inode;
    EXT2_INODE Directory;
    UINT16     Type;
    UINT16     Links;

    if (!EXT2MutableName(Volume, Name) || !EXT2ReadInode(Volume, Number, &Inode) || !EXT2ReadInode(Volume, Parent, &Directory) || !EXT2Access(Volume, &Directory, 3))
    {
        return FALSE;
    }

    Type = EXT2Load16(Inode.Raw) & EXT2_MODE_TYPE;
    Links = EXT2Load16(Inode.Raw + 26);

    if (Number < Volume->FirstInode || Type == EXT2_MODE_DIRECTORY || (Type != EXT2_MODE_FILE && Type != EXT2_MODE_SYMLINK) || EXT2Load32(Inode.Raw + 32) != 0 || Links >= 32000)
    {
        return EXT2Fail(Volume, EXT2_ERROR_UNSUPPORTED);
    }

    if (Volume->UID != 0 && Volume->UID != EXT2InodeUID(&Inode))
    {
        return EXT2Fail(Volume, EXT2_ERROR_PERMISSION);
    }

    if (!EXT2AddEntry(Volume, &Directory, Name, Number, Type == EXT2_MODE_SYMLINK ? 7 : 1))
    {
        return FALSE;
    }

    EXT2Store16(Inode.Raw + 26, Links + 1);
    EXT2Store32(Inode.Raw + 12, Time);
    EXT2Store32(Directory.Raw + 12, Time);
    EXT2Store32(Directory.Raw + 16, Time);

    return EXT2WriteInode(Volume, Number, &Inode, FALSE) && EXT2WriteInode(Volume, Parent, &Directory, FALSE);
}

BOOLEAN EXT2ReadLinkData(EXT2_VOLUME *Volume, EXT2_INODE *Inode, CHAR8 *Buffer, UINT32 Capacity, UINT32 *Length)
{
    UINT64 Size = EXT2InodeSize(Inode);

    if ((EXT2Load16(Inode->Raw) & EXT2_MODE_TYPE) != EXT2_MODE_SYMLINK || Size == 0 || Size >= Capacity || Size >= 4096)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (EXT2Load32(Inode->Raw + 28) == 0)
    {
        if (Size > 60)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }

        KMemCpy(Buffer, Inode->Raw + 40, (UINT32) Size);
        *Length = (UINT32) Size;
    }
    else if (!EXT2ReadData(Volume, Inode, 0, Buffer, (UINT32) Size, Length) || *Length != Size)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < *Length; Index++)
    {
        if (Buffer[Index] == 0)
        {
            return EXT2Fail(Volume, EXT2_ERROR_CORRUPT);
        }
    }

    Buffer[*Length] = 0;

    return TRUE;
}

BOOLEAN EXT2SymlinkEntry(EXT2_VOLUME *Volume, UINT32 Parent, CONST CHAR8 *Name, CONST CHAR8 *Target, UINT32 Time, UINT32 *Number)
{
    UINT32     Length = 0;
    EXT2_INODE Inode;

    while (Length < 4096 && Target[Length] != 0)
    {
        Length++;
    }

    if (Length == 0 || Length == 4096)
    {
        return EXT2Fail(Volume, EXT2_ERROR_ARGUMENT);
    }

    if (!EXT2CreateEntry(Volume, Parent, Name, FALSE, 0777, Time, Number) || !EXT2ReadInode(Volume, *Number, &Inode))
    {
        return FALSE;
    }

    EXT2Store16(Inode.Raw, EXT2_MODE_SYMLINK | 0777);

    if (Length < 60)
    {
        KMemCpy(Inode.Raw + 40, Target, Length);
        EXT2Store32(Inode.Raw + 4, Length);
    }
    else if (!EXT2WriteData(Volume, &Inode, 0, Target, Length))
    {
        return FALSE;
    }

    {
        EXT2_INODE Directory;

        if (!EXT2ReadInode(Volume, Parent, &Directory) || !EXT2EraseEntry(Volume, &Directory, Name) || !EXT2AddEntry(Volume, &Directory, Name, *Number, 7) || !EXT2WriteInode(Volume, Parent, &Directory, FALSE))
        {
            return FALSE;
        }
    }

    return EXT2WriteInode(Volume, *Number, &Inode, FALSE);
}

BOOLEAN EXT2ValidateDirectories(EXT2_VOLUME *Volume)
{
    UINT64  Entries = (UINT64) Volume->InodeCount + 1;
    UINT32 *References = KAlloc(Entries * sizeof(UINT32));
    UINT32 *Parents = KAlloc(Entries * sizeof(UINT32));
    UINT32 *DotParents = KAlloc(Entries * sizeof(UINT32));
    UINT16 *Modes = KAlloc(Entries * sizeof(UINT16));
    UINT16 *Links = KAlloc(Entries * sizeof(UINT16));
    UINT8   Bitmap[EXT2_MAX_BLOCK_SIZE];
    UINT8   Buffer[EXT2_MAX_BLOCK_SIZE];
    BOOLEAN Result = FALSE;

    if (References == NULL || Parents == NULL || DotParents == NULL || Modes == NULL || Links == NULL)
    {
        EXT2Fail(Volume, EXT2_ERROR_MEMORY);

        goto Done;
    }

    KMemSet(References, 0, Entries * sizeof(UINT32));
    KMemSet(Parents, 0, Entries * sizeof(UINT32));
    KMemSet(DotParents, 0, Entries * sizeof(UINT32));
    KMemSet(Modes, 0, Entries * sizeof(UINT16));
    KMemSet(Links, 0, Entries * sizeof(UINT16));
    Parents[2] = 2;

    for (UINT32 Group = 0; Group < Volume->GroupCount; Group++)
    {
        EXT2_BLOCK_GROUP Descriptor;
        UINT32           Directories = 0;

        if (!EXT2ReadBlockGroup(Volume, Group, &Descriptor) || !EXT2ReadMetadataBlock(Volume, Descriptor.InodeBitmap, Bitmap))
        {
            goto Done;
        }

        for (UINT32 Bit = 0; Bit < Volume->InodesPerGroup; Bit++)
        {
            EXT2_INODE Inode;
            UINT32     Number = Group * Volume->InodesPerGroup + Bit + 1;
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

            if (Type != 0x1000 && Type != 0x2000 && Type != 0x4000 && Type != 0x6000 && Type != 0x8000 && Type != 0xA000 && Type != 0xC000)
            {
                EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

                goto Done;
            }

            Modes[Number] = Type;
            Links[Number] = EXT2Load16(Inode.Raw + 26);
            Directories += Type == EXT2_MODE_DIRECTORY;
        }

        if (Directories != Descriptor.UsedDirectories)
        {
            EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

            goto Done;
        }
    }

    for (UINT32 Number = 2; Number <= Volume->InodeCount; Number++)
    {
        EXT2_INODE Inode;
        UINT32     Dot;
        UINT32     DotDot;

        if (Modes[Number] != EXT2_MODE_DIRECTORY)
        {
            continue;
        }

        if (!EXT2ReadInode(Volume, Number, &Inode) || !EXT2FindEntry(Volume, &Inode, (CONST CHAR8 *) ".", &Dot) || !EXT2FindEntry(Volume, &Inode, (CONST CHAR8 *) "..", &DotDot) || Dot != Number || Modes[DotDot] != EXT2_MODE_DIRECTORY)
        {
            EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

            goto Done;
        }

        DotParents[Number] = DotDot;

        for (UINT64 Logical = 0; Logical < EXT2InodeSize(&Inode) / Volume->BlockSize; Logical++)
        {
            UINT32 Block;

            if (!EXT2MapBlock(Volume, &Inode, Logical, FALSE, &Block) || !EXT2ReadMetadataBlock(Volume, Block, Buffer))
            {
                goto Done;
            }

            for (UINT32 Offset = 0; Offset < Volume->BlockSize; Offset += EXT2Load16(Buffer + Offset + 4))
            {
                UINT32  Child = EXT2Load32(Buffer + Offset);
                UINT32  Length = Volume->FileType ? Buffer[Offset + 6] : EXT2Load16(Buffer + Offset + 6);
                BOOLEAN DotName = Length == 1 && Buffer[Offset + 8] == '.';
                BOOLEAN DotDotName = Length == 2 && Buffer[Offset + 8] == '.' && Buffer[Offset + 9] == '.';
                UINT16  Type;
                UINT8   FileType;

                if (Child == 0)
                {
                    continue;
                }

                Type = Modes[Child];
                FileType = Type == 0x8000 ? 1 : Type == 0x4000 ? 2 : Type == 0x2000 ? 3 : Type == 0x6000 ? 4 : Type == 0x1000 ? 5 : Type == 0xC000 ? 6 : 7;

                if (Type == 0 || References[Child] == UINT32_MAX || (Volume->FileType && Buffer[Offset + 7] != 0 && Buffer[Offset + 7] != FileType))
                {
                    EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

                    goto Done;
                }

                References[Child]++;

                if (Type == EXT2_MODE_DIRECTORY && !DotName && !DotDotName)
                {
                    if (Parents[Child] != 0)
                    {
                        EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

                        goto Done;
                    }

                    Parents[Child] = Number;
                }
            }
        }
    }

    for (UINT32 Number = 2; Number <= Volume->InodeCount; Number++)
    {
        if (Modes[Number] == 0)
        {
            continue;
        }

        if (References[Number] != Links[Number] || (Modes[Number] == EXT2_MODE_DIRECTORY && (Parents[Number] == 0 || DotParents[Number] != Parents[Number])))
        {
            EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

            goto Done;
        }

        if (Modes[Number] == EXT2_MODE_DIRECTORY)
        {
            UINT32 Current = Number;
            UINT32 Steps = 0;

            while (Current != 2 && Steps++ < Volume->InodeCount)
            {
                Current = Parents[Current];
            }

            if (Current != 2)
            {
                EXT2Fail(Volume, EXT2_ERROR_CORRUPT);

                goto Done;
            }
        }
    }

    Result = TRUE;
Done:

    if (References != NULL)
    {
        KFree(References);
    }

    if (Parents != NULL)
    {
        KFree(Parents);
    }

    if (DotParents != NULL)
    {
        KFree(DotParents);
    }

    if (Modes != NULL)
    {
        KFree(Modes);
    }

    if (Links != NULL)
    {
        KFree(Links);
    }

    return Result;
}
