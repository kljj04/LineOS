// Kernel/Source/FS/VFS/VFS.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/VFS/VFS.h>
#include <Memory/PMM.h>

STATIC UINT32 VFSLength(CONST CHAR8 *Text, UINT32 Limit)
{
    UINT32 Length = 0;

    if (Text == NULL)
    {
        return Limit;
    }

    while (Length < Limit && Text[Length] != 0)
    {
        Length++;
    }

    return Length;
}

STATIC BOOLEAN VFSEqual(CONST CHAR8 *A, CONST CHAR8 *B)
{
    while (*A != 0 && *A == *B)
    {
        A++;
        B++;
    }

    return *A == *B;
}

STATIC BOOLEAN VFSLock(VFS_REGISTRY *Registry)
{
    UINT32 Expected = 0;
    return Registry != NULL && __atomic_compare_exchange_n(&Registry->Busy, &Expected, 1, FALSE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

STATIC VFS_ERROR VFSUnlock(VFS_REGISTRY *Registry, VFS_ERROR Error)
{
    __atomic_store_n(&Registry->Busy, 0, __ATOMIC_RELEASE);
    return Error;
}

STATIC UINT32 VFSFindPartition(VFS_REGISTRY *Registry, CONST CHAR8 *Name)
{
    for (UINT32 Index = 0; Index < VFS_MAX_PARTITIONS; Index++)
    {
        if (Registry->Partitions[Index].Registered && VFSEqual(Registry->Partitions[Index].Name, Name))
        {
            return Index;
        }
    }

    return VFS_INVALID_PARTITION;
}

STATIC VFS_PARTITION *VFSPartition(VFS_CONTEXT *Context, UINT32 Number)
{
    if (Number >= VFS_MAX_PARTITIONS || !Context->Registry->Partitions[Number].Registered)
    {
        return NULL;
    }

    return &Context->Registry->Partitions[Number];
}

VOID VFSInit(VFS_REGISTRY *Registry)
{
    if (Registry != NULL)
    {
        KMemSet(Registry, 0, sizeof(*Registry));
    }
}

VFS_ERROR VFSRegisterPartition(VFS_REGISTRY *Registry, CONST CHAR8 *Name, UINT32 GPTIndex, VOID *Backend, CONST VFS_BACKEND_OPERATIONS *Operations, UINT32 *Partition)
{
    UINT32 Length = VFSLength(Name, 32);

    if (Registry == NULL || Partition == NULL || Length == 0 || Length >= 32 || (Operations != NULL && (Backend == NULL || Operations->Root == NULL || Operations->Child == NULL || Operations->ReadLink == NULL)))
    {
        return VFS_INVALID;
    }

    for (UINT32 Index = 0; Index < Length; Index++)
    {
        CHAR8 Ch = Name[Index];

        if (!((Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z') || (Ch >= '0' && Ch <= '9') || Ch == '_' || Ch == '-'))
        {
            return VFS_INVALID;
        }
    }

    if (!VFSLock(Registry))
    {
        return VFS_BUSY;
    }

    if (VFSFindPartition(Registry, Name) != VFS_INVALID_PARTITION)
    {
        return VFSUnlock(Registry, VFS_EXISTS);
    }

    for (UINT32 Index = 0; Index < VFS_MAX_PARTITIONS; Index++)
    {
        if (Backend != NULL && Registry->Partitions[Index].Registered && Registry->Partitions[Index].Backend == Backend)
        {
            return VFSUnlock(Registry, VFS_EXISTS);
        }
    }

    for (UINT32 Index = 0; Index < VFS_MAX_PARTITIONS; Index++)
    {
        VFS_PARTITION *Entry = &Registry->Partitions[Index];

        if (!Entry->Registered)
        {
            KMemCpy(Entry->Name, Name, Length + 1);
            Entry->GPTIndex = GPTIndex;
            Entry->Backend = Backend;
            Entry->Operations = Operations;
            Entry->Registered = TRUE;
            *Partition = Index;
            return VFSUnlock(Registry, VFS_OK);
        }
    }

    return VFSUnlock(Registry, VFS_LIMIT);
}

VFS_ERROR VFSContextInit(VFS_CONTEXT *Context, VFS_REGISTRY *Registry, UINT32 Partition, VFS_CREDENTIALS Credentials)
{
    if (Context == NULL || Registry == NULL || Partition >= VFS_MAX_PARTITIONS)
    {
        return VFS_INVALID;
    }

    if (!VFSLock(Registry))
    {
        return VFS_BUSY;
    }

    if (!Registry->Partitions[Partition].Registered)
    {
        return VFSUnlock(Registry, VFS_NOT_FOUND);
    }

    KMemSet(Context, 0, sizeof(*Context));
    Context->Registry = Registry;
    Context->Credentials = Credentials;
    Context->Partition = Partition;
    Context->Cwd[0] = '/';
    return VFSUnlock(Registry, VFS_OK);
}

VFS_ERROR VFSBeginCommand(VFS_CONTEXT *Session, BOOLEAN Elevate, BOOLEAN Authorized, VFS_COMMAND *Command)
{
    if (Session == NULL || Session->Registry == NULL || Command == NULL)
    {
        return VFS_INVALID;
    }

    if (Elevate && !Authorized && !Session->Credentials.Administrator)
    {
        return VFS_PERMISSION;
    }

    KMemSet(Command, 0, sizeof(*Command));
    Command->Session = Session;
    Command->Context = *Session;
    Command->Active = TRUE;

    if (Elevate)
    {
        Command->Context.Credentials.UID = 0;
        Command->Context.Credentials.GID = 0;
        Command->Context.Credentials.Administrator = TRUE;
    }

    return VFS_OK;
}

VFS_ERROR VFSEndCommand(VFS_COMMAND *Command, BOOLEAN KeepLocation)
{
    VFS_ERROR Error = VFS_OK;

    if (Command == NULL || !Command->Active || Command->Session == NULL)
    {
        return VFS_INVALID;
    }

    if (KeepLocation)
    {
        Command->Session->Partition = Command->Context.Partition;
        Command->Session->AdministratorRoot = Command->Context.AdministratorRoot;
        KMemCpy(Command->Session->Cwd, Command->Context.Cwd, VFS_MAX_PATH);
    }

    KMemSet(&Command->Context.Credentials, 0, sizeof(Command->Context.Credentials));
    Command->Context.Registry = NULL;
    Command->Session = NULL;
    Command->Active = FALSE;
    return Error;
}

VFS_ERROR VFSGetCwd(CONST VFS_CONTEXT *Context, CHAR8 *Buffer, UINT32 Capacity)
{
    UINT32 Length;

    if (Context == NULL || Buffer == NULL || Context->Registry == NULL)
    {
        return VFS_INVALID;
    }

    Length = Context->AdministratorRoot ? 1 : VFSLength(Context->Cwd, VFS_MAX_PATH);

    if (Length >= Capacity || Length == VFS_MAX_PATH)
    {
        return VFS_LIMIT;
    }

    if (Context->AdministratorRoot)
    {
        Buffer[0] = '^';
        Buffer[1] = 0;
    }
    else
    {
        KMemCpy(Buffer, Context->Cwd, Length + 1);
    }

    return VFS_OK;
}

STATIC VFS_ERROR VFSRoot(VFS_CONTEXT *Context, VFS_RESOLVED *Resolved)
{
    VFS_PARTITION *Partition = VFSPartition(Context, Resolved->Partition);

    if (Partition == NULL)
    {
        return VFS_NOT_FOUND;
    }

    if (Partition->Operations == NULL)
    {
        return VFS_UNSUPPORTED;
    }

    Resolved->AdministratorRoot = FALSE;
    Resolved->Path[0] = '/';
    Resolved->Path[1] = 0;
    return Partition->Operations->Root(Partition->Backend, &Context->Credentials, &Resolved->Node);
}

STATIC VFS_ERROR VFSResolve(VFS_CONTEXT *Context, CONST CHAR8 *Path, BOOLEAN FollowFinal, VFS_RESOLVED *Resolved)
{
    UINT32    Length = VFSLength(Path, VFS_MAX_PATH);
    UINT32    Position = 0;
    UINT32    Links = 0;
    CHAR8    *Pending;
    CHAR8    *Target;
    VFS_ERROR Error = VFS_OK;

    if (Length == 0 || Length >= VFS_MAX_PATH)
    {
        return VFS_INVALID;
    }

    if ((Path[0] == '^' || (Path[0] != '/' && Context->AdministratorRoot)) && !Context->Credentials.Administrator)
    {
        return VFS_NOT_FOUND;
    }

    Pending = KAlloc(VFS_MAX_PATH * 2);

    if (Pending == NULL)
    {
        return VFS_LIMIT;
    }

    Target = Pending + VFS_MAX_PATH;
    KMemSet(Resolved, 0, sizeof(*Resolved));
    Resolved->Partition = Context->Partition;
    Resolved->AdministratorRoot = Path[0] == '^' || (Path[0] != '/' && Context->AdministratorRoot);

    if (Path[0] == '^')
    {
        if (Path[1] != 0 && Path[1] != '/')
        {
            Error = VFS_NOT_FOUND;
            goto Done;
        }

        KMemCpy(Pending, Path + 1, Length);
        Length--;
    }
    else if (Path[0] != '/' && !Resolved->AdministratorRoot)
    {
        UINT32 Prefix = VFSLength(Context->Cwd, VFS_MAX_PATH);

        if (Prefix >= VFS_MAX_PATH || Prefix + Length + 1 >= VFS_MAX_PATH)
        {
            Error = VFS_LIMIT;
            goto Done;
        }

        KMemCpy(Pending, Context->Cwd, Prefix);
        Pending[Prefix] = '/';
        KMemCpy(Pending + Prefix + 1, Path, Length + 1);
        Length += Prefix + 1;
    }
    else
    {
        KMemCpy(Pending, Path, Length + 1);
    }

    if (!Resolved->AdministratorRoot)
    {
        Error = VFSRoot(Context, Resolved);

        if (Error != VFS_OK)
        {
            goto Done;
        }
    }

    for (;;)
    {
        CHAR8          Name[256];
        UINT32         NameLength = 0;
        VFS_PARTITION *Partition;
        VFS_NODE       Child;

        while (Pending[Position] == '/')
        {
            if (!Resolved->AdministratorRoot && (Resolved->Node.Mode & VFS_MODE_TYPE) != VFS_MODE_DIRECTORY)
            {
                Error = VFS_INVALID;
                goto Done;
            }

            Position++;
        }

        if (Pending[Position] == 0)
        {
            if (Resolved->AdministratorRoot)
            {
                Resolved->Path[0] = '^';
                Resolved->Path[1] = 0;
                Resolved->Node.Mode = VFS_MODE_DIRECTORY | 0700;
            }

            break;
        }

        while (Pending[Position] != 0 && Pending[Position] != '/')
        {
            if (NameLength == 255)
            {
                Error = VFS_LIMIT;
                goto Done;
            }

            Name[NameLength++] = Pending[Position++];
        }

        Name[NameLength] = 0;

        if (Name[0] == '^')
        {
            Error = VFS_NOT_FOUND;
            goto Done;
        }

        if (Resolved->AdministratorRoot)
        {
            if (VFSEqual(Name, (CONST CHAR8 *) ".") || VFSEqual(Name, (CONST CHAR8 *) ".."))
            {
                continue;
            }

            Resolved->Partition = VFSFindPartition(Context->Registry, Name);
            Error = VFSRoot(Context, Resolved);

            if (Error != VFS_OK)
            {
                goto Done;
            }

            continue;
        }

        Partition = VFSPartition(Context, Resolved->Partition);
        Error = Partition->Operations->Child(Partition->Backend, &Context->Credentials, Resolved->Node.Number, Name, &Child);

        if (Error != VFS_OK)
        {
            goto Done;
        }

        if ((Child.Mode & VFS_MODE_TYPE) == VFS_MODE_SYMLINK && (FollowFinal || Pending[Position] != 0))
        {
            UINT32 TargetLength;
            UINT32 TailLength = Length - Position;

            if (++Links > 40)
            {
                Error = VFS_LOOP;
                goto Done;
            }

            Error = Partition->Operations->ReadLink(Partition->Backend, &Context->Credentials, Child.Number, Target, &TargetLength);

            if (Error != VFS_OK)
            {
                goto Done;
            }

            if (TargetLength == 0 || TargetLength + TailLength >= VFS_MAX_PATH)
            {
                Error = VFS_LIMIT;
                goto Done;
            }

            if (Target[0] == '^')
            {
                Error = VFS_NOT_FOUND;
                goto Done;
            }

            KMemCpy(Target + TargetLength, Pending + Position, TailLength + 1);
            Length = TargetLength + TailLength;
            KMemCpy(Pending, Target, Length + 1);
            Position = 0;

            if (Pending[0] == '/')
            {
                Error = VFSRoot(Context, Resolved);

                if (Error != VFS_OK)
                {
                    goto Done;
                }
            }

            continue;
        }

        if (VFSEqual(Name, (CONST CHAR8 *) ".."))
        {
            UINT32 End = VFSLength(Resolved->Path, VFS_MAX_PATH);

            while (End > 1 && Resolved->Path[End - 1] != '/')
            {
                End--;
            }

            Resolved->Path[End > 1 ? End - 1 : 1] = 0;
        }
        else if (!VFSEqual(Name, (CONST CHAR8 *) "."))
        {
            UINT32 End = VFSLength(Resolved->Path, VFS_MAX_PATH);
            UINT32 Separator = End == 1 ? 0 : 1;

            if (End + Separator + NameLength >= VFS_MAX_PATH)
            {
                Error = VFS_LIMIT;
                goto Done;
            }

            if (Separator)
            {
                Resolved->Path[End++] = '/';
            }

            KMemCpy(Resolved->Path + End, Name, NameLength + 1);
        }

        Resolved->Node = Child;
    }

Done:
    KFree(Pending);
    return Error;
}

VFS_ERROR VFSSelectPartition(VFS_CONTEXT *Context, CONST CHAR8 *Name)
{
    CHAR8  Path[35];
    UINT32 Length = VFSLength(Name, 32);

    if (Context == NULL || Length == 0 || Length >= 32)
    {
        return VFS_INVALID;
    }

    if (!Context->Credentials.Administrator)
    {
        return VFS_NOT_FOUND;
    }

    for (UINT32 Index = 0; Index < Length; Index++)
    {
        CHAR8 Ch = Name[Index];

        if (!((Ch >= 'A' && Ch <= 'Z') || (Ch >= 'a' && Ch <= 'z') || (Ch >= '0' && Ch <= '9') || Ch == '_' || Ch == '-'))
        {
            return VFS_INVALID;
        }
    }

    Path[0] = '^';
    Path[1] = '/';
    KMemCpy(Path + 2, Name, Length + 1);
    return VFSChangeDirectory(Context, Path);
}

VFS_ERROR VFSChangeDirectory(VFS_CONTEXT *Context, CONST CHAR8 *Path)
{
    VFS_RESOLVED Resolved;
    VFS_ERROR    Error;

    if (Context == NULL || Context->Registry == NULL)
    {
        return VFS_INVALID;
    }

    if (!VFSLock(Context->Registry))
    {
        return VFS_BUSY;
    }

    Error = VFSResolve(Context, Path, TRUE, &Resolved);

    if (Error == VFS_OK && !Resolved.AdministratorRoot)
    {
        VFS_PARTITION *Partition = VFSPartition(Context, Resolved.Partition);
        VFS_NODE       Dot;
        Error = Partition->Operations->Child(Partition->Backend, &Context->Credentials, Resolved.Node.Number, (CONST CHAR8 *) ".", &Dot);
    }

    if (Error == VFS_OK)
    {
        Context->Partition = Resolved.Partition;
        Context->AdministratorRoot = Resolved.AdministratorRoot;
        KMemCpy(Context->Cwd, Resolved.Path, VFS_MAX_PATH);
    }

    return VFSUnlock(Context->Registry, Error);
}

STATIC VFS_ERROR VFSLookupMode(VFS_CONTEXT *Context, CONST CHAR8 *Path, VFS_NODE *Node, BOOLEAN Follow)
{
    VFS_RESOLVED Resolved;
    VFS_ERROR    Error;

    if (Context == NULL || Context->Registry == NULL || Node == NULL)
    {
        return VFS_INVALID;
    }

    if (!VFSLock(Context->Registry))
    {
        return VFS_BUSY;
    }

    Error = VFSResolve(Context, Path, Follow, &Resolved);

    if (Error == VFS_OK)
    {
        *Node = Resolved.Node;
        Node->Partition = Resolved.AdministratorRoot ? VFS_INVALID_PARTITION : Resolved.Partition;
    }

    return VFSUnlock(Context->Registry, Error);
}

VFS_ERROR VFSLookup(VFS_CONTEXT *Context, CONST CHAR8 *Path, VFS_NODE *Node)
{
    return VFSLookupMode(Context, Path, Node, TRUE);
}

VFS_ERROR VFSLookupNoFollow(VFS_CONTEXT *Context, CONST CHAR8 *Path, VFS_NODE *Node)
{
    return VFSLookupMode(Context, Path, Node, FALSE);
}

VFS_ERROR VFSReadDirectory(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 *Cursor, VFS_DIRECTORY_ENTRY *Entry)
{
    VFS_RESOLVED Resolved;
    VFS_ERROR    Error;

    if (Context == NULL || Context->Registry == NULL || Cursor == NULL || Entry == NULL)
    {
        return VFS_INVALID;
    }

    KMemSet(Entry, 0, sizeof(*Entry));

    if (!VFSLock(Context->Registry))
    {
        return VFS_BUSY;
    }

    Error = VFSResolve(Context, Path, TRUE, &Resolved);

    if (Error == VFS_OK && Resolved.AdministratorRoot)
    {
        while (*Cursor < VFS_MAX_PARTITIONS)
        {
            VFS_PARTITION *Partition = &Context->Registry->Partitions[(*Cursor)++];

            if (Partition->Registered)
            {
                KMemCpy(Entry->Name, Partition->Name, 32);
                Entry->Node.Mode = VFS_MODE_DIRECTORY | 0700;
                return VFSUnlock(Context->Registry, VFS_OK);
            }
        }

        Entry->End = TRUE;
    }
    else if (Error == VFS_OK)
    {
        VFS_PARTITION *Partition = VFSPartition(Context, Resolved.Partition);
        if (Partition->Operations->ReadDirectory == NULL)
        {
            Error = VFS_UNSUPPORTED;
        }
        else
        {
            do
            {
                Error = Partition->Operations->ReadDirectory(Partition->Backend, &Context->Credentials, Resolved.Node.Number, Cursor, Entry);
            } while (Error == VFS_OK && !Entry->End && Entry->Name[0] == '^');
        }
    }

    return VFSUnlock(Context->Registry, Error);
}

STATIC VFS_ERROR VFSData(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength, BOOLEAN Write)
{
    VFS_RESOLVED Resolved;
    VFS_ERROR    Error;

    if (ReadLength != NULL)
    {
        *ReadLength = 0;
    }

    if (Context == NULL || Context->Registry == NULL || (Length != 0 && Buffer == NULL) || (!Write && ReadLength == NULL))
    {
        return VFS_INVALID;
    }

    if (!VFSLock(Context->Registry))
    {
        return VFS_BUSY;
    }

    Error = VFSResolve(Context, Path, TRUE, &Resolved);

    if (Error == VFS_OK && Resolved.AdministratorRoot)
    {
        Error = VFS_INVALID;
    }
    else if (Error == VFS_OK)
    {
        VFS_PARTITION *Partition = VFSPartition(Context, Resolved.Partition);

        if (Write)
        {
            Error = Partition->Operations->Write == NULL ? VFS_UNSUPPORTED : Partition->Operations->Write(Partition->Backend, &Context->Credentials, Resolved.Node.Number, Offset, Buffer, Length);
        }
        else
        {
            Error = Partition->Operations->Read == NULL ? VFS_UNSUPPORTED : Partition->Operations->Read(Partition->Backend, &Context->Credentials, Resolved.Node.Number, Offset, Buffer, Length, ReadLength);
        }
    }

    return VFSUnlock(Context->Registry, Error);
}

VFS_ERROR VFSRead(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength)
{
    return VFSData(Context, Path, Offset, Buffer, Length, ReadLength, FALSE);
}

VFS_ERROR VFSWrite(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Offset, CONST VOID *Buffer, UINT32 Length)
{
    return VFSData(Context, Path, Offset, (VOID *) Buffer, Length, NULL, TRUE);
}

STATIC VFS_ERROR VFSParent(VFS_CONTEXT *Context, CONST CHAR8 *Path, VFS_RESOLVED *Resolved, CHAR8 *Name)
{
    UINT32    Length = VFSLength(Path, VFS_MAX_PATH);
    UINT32    Start = Length;
    CHAR8    *Parent;
    VFS_ERROR Error;

    if (Length == 0 || Length >= VFS_MAX_PATH || Path[Length - 1] == '/')
    {
        return VFS_INVALID;
    }

    while (Start != 0 && Path[Start - 1] != '/')
    {
        Start--;
    }

    if (Length - Start > 255)
    {
        return VFS_LIMIT;
    }

    KMemCpy(Name, Path + Start, Length - Start + 1);

    if (Name[0] == '^')
    {
        return VFS_NOT_FOUND;
    }

    if (VFSEqual(Name, (CONST CHAR8 *) ".") || VFSEqual(Name, (CONST CHAR8 *) ".."))
    {
        return VFS_INVALID;
    }

    Parent = KAlloc(VFS_MAX_PATH);

    if (Parent == NULL)
    {
        return VFS_LIMIT;
    }

    if (Start == 0)
    {
        Parent[0] = '.';
        Parent[1] = 0;
    }
    else
    {
        KMemCpy(Parent, Path, Start);
        Parent[Start] = 0;
    }

    Error = VFSResolve(Context, Parent, TRUE, Resolved);
    KFree(Parent);

    if (Error == VFS_OK && (Resolved->AdministratorRoot || (Resolved->Node.Mode & VFS_MODE_TYPE) != VFS_MODE_DIRECTORY))
    {
        Error = VFS_INVALID;
    }

    return Error;
}

STATIC VFS_ERROR VFSMutate(VFS_CONTEXT *Context, VFS_MUTATION *Mutation, CONST CHAR8 *Path, CONST CHAR8 *Other)
{
    VFS_RESOLVED First;
    VFS_RESOLVED Second = {0};
    CHAR8        Name[256];
    CHAR8        OtherName[256];
    VFS_ERROR    Error;
    BOOLEAN      Parent = Mutation->Type == VFS_MUTATE_CREATE || Mutation->Type == VFS_MUTATE_REMOVE || Mutation->Type == VFS_MUTATE_SYMLINK || Mutation->Type == VFS_MUTATE_RENAME;

    if (Context == NULL || Context->Registry == NULL)
    {
        return VFS_INVALID;
    }

    if (!VFSLock(Context->Registry))
    {
        return VFS_BUSY;
    }

    Error = Parent ? VFSParent(Context, Path, &First, Name) : VFSResolve(Context, Path, Mutation->Type != VFS_MUTATE_LINK, &First);

    if (Error == VFS_OK && First.AdministratorRoot)
    {
        Error = VFS_INVALID;
    }

    if (Error == VFS_OK)
    {
        Mutation->Parent = First.Node.Number;
        Mutation->Number = First.Node.Number;
        Mutation->Name = Name;

        if (Mutation->Type == VFS_MUTATE_RENAME || Mutation->Type == VFS_MUTATE_LINK)
        {
            Error = VFSParent(Context, Other, &Second, OtherName);

            if (Error == VFS_OK && First.Partition != Second.Partition)
            {
                Error = VFS_CROSS_PARTITION;
            }

            Mutation->OtherParent = Second.Node.Number;
            Mutation->OtherName = OtherName;

            if (Mutation->Type == VFS_MUTATE_LINK)
            {
                Mutation->Parent = Second.Node.Number;
                Mutation->Name = OtherName;
            }
        }
    }

    if (Error == VFS_OK)
    {
        VFS_PARTITION *Partition = VFSPartition(Context, First.Partition);
        Error = Partition->Operations->Mutate == NULL ? VFS_UNSUPPORTED : Partition->Operations->Mutate(Partition->Backend, &Context->Credentials, Mutation);
    }

    return VFSUnlock(Context->Registry, Error);
}

VFS_ERROR VFSCreate(VFS_CONTEXT *Context, CONST CHAR8 *Path, BOOLEAN Directory, UINT16 Mode)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_CREATE;
    Mutation.Directory = Directory;
    Mutation.Mode = Mode;
    return VFSMutate(Context, &Mutation, Path, NULL);
}

VFS_ERROR VFSRemove(VFS_CONTEXT *Context, CONST CHAR8 *Path)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_REMOVE;
    return VFSMutate(Context, &Mutation, Path, NULL);
}

VFS_ERROR VFSRename(VFS_CONTEXT *Context, CONST CHAR8 *OldPath, CONST CHAR8 *NewPath)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_RENAME;
    return VFSMutate(Context, &Mutation, OldPath, NewPath);
}

VFS_ERROR VFSTruncate(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT64 Size)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_TRUNCATE;
    Mutation.Size = Size;
    return VFSMutate(Context, &Mutation, Path, NULL);
}

VFS_ERROR VFSLink(VFS_CONTEXT *Context, CONST CHAR8 *Source, CONST CHAR8 *Destination)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_LINK;
    return VFSMutate(Context, &Mutation, Source, Destination);
}

VFS_ERROR VFSSymlink(VFS_CONTEXT *Context, CONST CHAR8 *Target, CONST CHAR8 *Path)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_SYMLINK;
    Mutation.Target = Target;

    if (VFSLength(Target, VFS_MAX_PATH) == 0 || VFSLength(Target, VFS_MAX_PATH) >= VFS_MAX_PATH)
    {
        return VFS_INVALID;
    }

    return VFSMutate(Context, &Mutation, Path, NULL);
}

VFS_ERROR VFSSetMode(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT16 Mode)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_MODE;
    Mutation.Mode = Mode;
    return VFSMutate(Context, &Mutation, Path, NULL);
}

VFS_ERROR VFSSetOwner(VFS_CONTEXT *Context, CONST CHAR8 *Path, UINT32 UID, UINT32 GID)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_OWNER;
    Mutation.UID = UID;
    Mutation.GID = GID;
    return VFSMutate(Context, &Mutation, Path, NULL);
}

VFS_ERROR VFSTouch(VFS_CONTEXT *Context, CONST CHAR8 *Path)
{
    VFS_MUTATION Mutation = {0};
    Mutation.Type = VFS_MUTATE_TOUCH;
    return VFSMutate(Context, &Mutation, Path, NULL);
}
