// Kernel/Source/Syscall/File/SyscallFile.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

#include <Syscall/Memory/Memory.h>

#define FILE_OPEN_LARGEFILE 0100000ULL
#define FILE_BUFFER_SIZE    (1024U * 1024U)

STATIC INT64 FileRelease(OPEN_FILE *File)
{
    INT64 Result = ATOMIC_LOAD(&File->Failed, ATOMIC_RELAXED);

    if (ATOMIC_FETCH_SUB(&File->References, 1, ATOMIC_ACQ_REL) == 1)
    {
        if (File->Device != NULL && File->Device->Operations->Close != NULL)
        {
            INT64 Error = File->Device->Operations->Close(File->DeviceState);

            if (Result == 0)
            {
                Result = Error;
            }
        }

        KFree(File->Path);
        KFree(File);
    }

    return Result;
}

OPEN_FILE *FileAcquire(TASK *Task, UINT64 FD)
{
    if (Task == NULL || FD >= TASK_FILE_LIMIT)
    {
        return NULL;
    }

    PROCESS   *Process = Task->Process;
    UINT64     Flags   = SpinLockAcquireIRQSave(&Process->FileTableLock);
    OPEN_FILE *File    = Process->Files[FD];

    if (File != NULL)
    {
        ATOMIC_FETCH_ADD(&File->References, 1, ATOMIC_RELAXED);
    }

    SpinLockReleaseIRQRestore(&Process->FileTableLock, Flags);

    return File;
}

VOID FileDrop(OPEN_FILE **File)
{
    if (*File != NULL)
    {
        FileRelease(*File);
    }
}

STATIC OPEN_FILE *FileDetach(PROCESS *Process, UINT64 FD)
{
    UINT64     Flags = SpinLockAcquireIRQSave(&Process->FileTableLock);
    OPEN_FILE *File  = Process->Files[FD];

    Process->Files[FD] = NULL;
    Process->CloseOnExecFiles &= ~(1U << FD);
    SpinLockReleaseIRQRestore(&Process->FileTableLock, Flags);

    return File;
}

VOID SyscallFilesRelease(TASK *Task)
{
    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        OPEN_FILE *File = FileDetach(Task->Process, Index);

        FileDrop(&File);
    }
}

BOOLEAN SyscallFilesInherit(TASK *Child, TASK *Parent)
{
    if (Parent == NULL)
    {
        return TRUE;
    }

    PROCESS *Process = Parent->Process;
    UINT64   Flags   = SpinLockAcquireIRQSave(&Process->FileTableLock);

    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        OPEN_FILE *File = Process->Files[Index];

        if (File != NULL)
        {
            ATOMIC_FETCH_ADD(&File->References, 1, ATOMIC_RELAXED);
            Child->Process->Files[Index] = File;
        }
    }

    Child->Process->CloseOnExecFiles = Process->CloseOnExecFiles;
    SpinLockReleaseIRQRestore(&Process->FileTableLock, Flags);

    return TRUE;
}

STATIC VOID FilePublish(PROCESS *Process, UINTN Index, OPEN_FILE *File, BOOLEAN CloseOnExec)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&Process->FileTableLock);

    Process->Files[Index]     = File;
    Process->CloseOnExecFiles = (Process->CloseOnExecFiles & ~(1U << Index)) | (CloseOnExec ? 1U << Index : 0);
    Process->ReservedFiles &= ~(1U << Index);
    SpinLockReleaseIRQRestore(&Process->FileTableLock, Flags);
}

INT64 SyscallOpen(UINT64 Path, UINT64 Flags, UINT64 Mode)
{
    return SyscallOpenat(FILE_AT_FDCWD, Path, Flags, Mode);
}

INT64 SyscallOpenat(INT32 FD, UINT64 Path, UINT64 Flags, UINT64 Mode)
{
    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;
    UINTN    Index;
    INT64    Result;

    if (Task == NULL || Task->Process->AddressSpace == NULL || Task->Process->FileContext == NULL)
    {
        return -ESRCH;
    }

    BOOLEAN CloseOnExec = (Flags & FILE_OPEN_CLOEXEC) != 0;
    Flags &= ~(FILE_OPEN_LARGEFILE | (UINT64) FILE_OPEN_CLOEXEC);
    Flags = (UINT32) Flags;
    Mode  = (UINT32) Mode & 0777U;
    if ((Flags & (VFS_OPEN_CREATE | VFS_OPEN_DIRECTORY)) == (VFS_OPEN_CREATE | VFS_OPEN_DIRECTORY))
    {
        return -EINVAL;
    }

    if ((Flags & ~(VFS_OPEN_ACCMODE | VFS_OPEN_CREATE | VFS_OPEN_EXCLUSIVE | VFS_OPEN_TRUNCATE | VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_NOFOLLOW)) != 0 || (Flags & VFS_OPEN_ACCMODE) == 3)
    {
        return -EINVAL;
    }

    Result = FileResolvePath(Task, FD, Path, FALSE, &Name);

    if (Result < 0)
    {
        return Result;
    }

    OPEN_FILE *File = KAlloc(sizeof(*File));

    if (File == NULL)
    {
        KFree(Name);

        return -ENOMEM;
    }

    PROCESS *Process  = Task->Process;
    UINT64   IRQFlags = SpinLockAcquireIRQSave(&Process->FileTableLock);

    for (Index = 0; Index < TASK_FILE_LIMIT && (Process->Files[Index] != NULL || (Process->ReservedFiles & (1U << Index)) != 0); Index++)
    {
    }

    if (Index == TASK_FILE_LIMIT)
    {
        SpinLockReleaseIRQRestore(&Process->FileTableLock, IRQFlags);
        KFree(File);
        KFree(Name);

        return -EMFILE;
    }

    Process->ReservedFiles |= 1U << Index;
    SpinLockReleaseIRQRestore(&Process->FileTableLock, IRQFlags);
    KMemSet(File, 0, sizeof(*File));
    CHAR8 *Canonical = KAlloc(VFS_MAX_PATH);
    if (Canonical == NULL)
    {
        Result = -ENOMEM;
        goto Failed;
    }
    Result = SyscallVFSError(VFSOpenPath(Task->Process->FileContext, Name, (UINT32) Flags, (UINT16) Mode, &Node, Canonical));
    KFree(Name);
    Name        = Canonical;
    File->Flags = (UINT32) Flags | FILE_OPEN_LARGEFILE;

    if (Result < 0)
    {
        goto Failed;
    }

    File->Append = (Flags & VFS_OPEN_APPEND) != 0;
    File->Access = (Flags & VFS_OPEN_ACCMODE) == 0 ? FILE_READ : (Flags & VFS_OPEN_ACCMODE) == 1 ? FILE_WRITE : FILE_READ | FILE_WRITE;

    if ((Node.Mode & VFS_MODE_TYPE) == 0x2000U)
    {
        File->Device = Node.Partition == Task->Process->FileContext->Registry->DevicePartition ? DeviceFSGetNode(Node.Number) : NULL;

        if (File->Device == NULL)
        {
            Result = -ENXIO;

            goto Failed;
        }

        UINT32 Permission = Task->Process->FileContext->Credentials.UID == Node.UID ? (Node.Mode >> 6) & 7 : Task->Process->FileContext->Credentials.GID == Node.GID ? (Node.Mode >> 3) & 7 : Node.Mode & 7;

        if (Task->Process->FileContext->Credentials.UID != 0 && (((File->Access & FILE_READ) != 0 && (Permission & 4) == 0) || ((File->Access & FILE_WRITE) != 0 && (Permission & 2) == 0)))
        {
            Result = -EACCES;

            goto Failed;
        }

        Result = File->Device->Operations->Open == NULL ? 0 : File->Device->Operations->Open(Task, Flags, &File->DeviceState);

        if (Result < 0)
        {
            goto Failed;
        }
    }

    File->Directory = (Node.Mode & VFS_MODE_TYPE) == VFS_MODE_DIRECTORY;

    if (File->Directory)
    {
        UINT64              Cursor = 0;
        VFS_DIRECTORY_ENTRY Entry;

        Result = SyscallVFSError(VFSReadDirectory(Task->Process->FileContext, Name, &Cursor, &Entry));

        if (Result < 0)
        {
            goto Failed;
        }
    }

    File->References  = 1;
    File->Path        = Name;
    File->OpenContext = *Task->Process->FileContext;
    File->Generation  = Node.Generation;
    File->Number      = Node.Number;
    File->Partition   = Node.Partition;
    FilePublish(Process, Index, File, CloseOnExec);

    return (INT64) Index;
Failed:
    FilePublish(Process, Index, NULL, FALSE);
    KFree(File);
    KFree(Name);

    return Result;
}

INT64 FileBegin(OPEN_FILE *File)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&File->Lock);

    while (File->Busy != 0)
    {
        if (!WaitQueueSleep(&File->Waiters, &File->Lock))
        {
            SpinLockReleaseIRQRestore(&File->Lock, Flags);

            return -EINTR;
        }
    }

    File->Busy = 1;
    SpinLockReleaseIRQRestore(&File->Lock, Flags);

    return 0;
}

VOID FileEnd(OPEN_FILE *File)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&File->Lock);

    File->Busy = 0;
    WaitQueueWakeAll(&File->Waiters);
    SpinLockReleaseIRQRestore(&File->Lock, Flags);
}

STATIC INT64 FileRead(TASK *Task, OPEN_FILE *File, UINT64 Buffer, UINT64 Count)
{
    VFS_NODE Node       = {0};
    UINT64   Total      = 0;
    UINT32   BufferSize = Count > FILE_BUFFER_SIZE ? FILE_BUFFER_SIZE : (UINT32) Count;

    if (Count == 0)
    {
        return 0;
    }

    UINT8 *Bytes = KAlloc(BufferSize);

    if (Bytes == NULL)
    {
        return -ENOMEM;
    }

    Node.Generation = File->Generation;
    Node.Number     = File->Number;
    Node.Partition  = File->Partition;

    INT64 Error = 0;

    while (Total < Count)
    {
        UINT32 Length     = Count - Total > BufferSize ? BufferSize : (UINT32) (Count - Total);
        UINT32 ReadLength = 0;

        Error = SyscallVFSError(VFSReadChecked(Task->Process->FileContext, File->Path, &Node, File->Offset, Bytes, Length, &ReadLength));

        if (Error < 0 || ReadLength > Length)
        {
            Error = Error < 0 ? Error : -EIO;
            break;
        }

        if (ReadLength == 0)
        {
            break;
        }

        UINT32 Copied = 0;

        while (Copied < ReadLength)
        {
            UINT32 Chunk = (UINT32) (4096 - ((Buffer + Total) & 4095));

            if (Chunk > ReadLength - Copied)
            {
                Chunk = ReadLength - Copied;
            }

            if (!VMMCopyToUser(Task->Process->AddressSpace, Buffer + Total, Bytes + Copied, Chunk))
            {
                Error = -EFAULT;
                break;
            }

            File->Offset += Chunk;
            Total += Chunk;
            Copied += Chunk;
        }

        if (Copied != ReadLength)
        {
            break;
        }

        if (ReadLength < Length)
        {
            break;
        }
    }

    KFree(Bytes);

    return Total != 0 ? (INT64) Total : Error;
}

STATIC INT64 FileWrite(TASK *Task, OPEN_FILE *File, UINT64 Buffer, UINT64 Count)
{
    VFS_NODE Node = {0};

    INT64 Failed = ATOMIC_LOAD(&File->Failed, ATOMIC_RELAXED);

    if (Failed != 0)
    {
        return Failed;
    }

    if (Count == 0)
    {
        return 0;
    }

    UINT32 Length = Count > FILE_BUFFER_SIZE ? FILE_BUFFER_SIZE : (UINT32) Count;

    if (!File->Append && File->Offset > UINT64_MAX - Length)
    {
        return -EFBIG;
    }

    UINT8 *Bytes = KAlloc(Length);

    if (Bytes == NULL)
    {
        return -ENOMEM;
    }

    UINT32 Copied = 0;

    while (Copied < Length)
    {
        UINT32 Chunk = (UINT32) (4096 - ((Buffer + Copied) & 4095));

        if (Chunk > Length - Copied)
        {
            Chunk = Length - Copied;
        }

        if (!VMMCopyFromUser(Task->Process->AddressSpace, Bytes + Copied, Buffer + Copied, Chunk))
        {
            break;
        }

        Copied += Chunk;
    }

    if (Copied == 0)
    {
        KFree(Bytes);

        return -EFAULT;
    }

    Length          = Copied;
    Node.Generation = File->Generation;
    Node.Number     = File->Number;
    Node.Partition  = File->Partition;

    UINT64 EndOffset = File->Offset + Length;
    INT64  Result;

    if (File->Append)
    {
        Result = SyscallVFSError(VFSAppendChecked(Task->Process->FileContext, File->Path, &Node, Bytes, Length, &EndOffset));
    }
    else
    {
        Result = SyscallVFSError(VFSWriteChecked(Task->Process->FileContext, File->Path, &Node, File->Offset, Bytes, Length));
    }

    KFree(Bytes);

    if (Result == 0)
    {
        File->Offset = EndOffset;

        return Length;
    }

    if (Result == -EIO || Result == -EROFS)
    {
        ATOMIC_STORE(&File->Failed, Result, ATOMIC_RELAXED);
    }

    return Result;
}

STATIC INT64 FileData(UINT64 FD, UINT64 Buffer, UINT64 Count, BOOLEAN Write)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, FD);

    if (Task == NULL || Task->Process->AddressSpace == NULL || File == NULL || (File->Access & (Write ? FILE_WRITE : FILE_READ)) == 0)
    {
        return -EBADF;
    }

    if (File->Directory)
    {
        return -EISDIR;
    }

    if (Count != 0 && (Buffer >= (1ULL << 47) || Count > (1ULL << 47) - Buffer))
    {
        return -EFAULT;
    }

    if (Count > 0x7FFFF000ULL)
    {
        Count = 0x7FFFF000ULL;
    }

    BOOLEAN Serialized = File->Device == NULL || File->Device->Seekable;
    INT64   Result     = Serialized ? FileBegin(File) : 0;

    if (Result < 0)
    {
        return Result;
    }

    if (File->Device != NULL)
    {
        CONST DEVICE_OPERATIONS *Operations = File->Device->Operations;

        Result = Write ? (Operations->Write == NULL ? -EINVAL : Operations->Write(Task, File->DeviceState, &File->Offset, Buffer, Count)) : (Operations->Read == NULL ? -EINVAL : Operations->Read(Task, File->DeviceState, &File->Offset, Buffer, Count));
    }
    else
    {
        Result = Write ? FileWrite(Task, File, Buffer, Count) : FileRead(Task, File, Buffer, Count);
    }

    if (Serialized)
    {
        FileEnd(File);
    }

    return Result;
}

INT64 SyscallRead(UINT64 FD, UINT64 Buffer, UINT64 Count)
{
    return FileData(FD, Buffer, Count, FALSE);
}

INT64 SyscallFileWrite(UINT64 FD, UINT64 Buffer, UINT64 Count)
{
    return FileData(FD, Buffer, Count, TRUE);
}

INT64 SyscallFileWritev(UINT64 FD, UINT64 Vectors, UINT64 Count)
{
    typedef struct
    {
        UINT64 Buffer;
        UINT64 Length;
    } FILE_VECTOR;

    TASK        *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE   *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, FD);
    FILE_VECTOR *Items;
    UINT64       Length = 0;
    INT64        Total  = 0;

    if (Task == NULL || Task->Process->AddressSpace == NULL || File == NULL || (File->Access & FILE_WRITE) == 0)
    {
        return -EBADF;
    }
    if (File->Directory)
    {
        return -EISDIR;
    }
    if (Count > 1024)
    {
        return -EINVAL;
    }
    if (Count == 0)
    {
        return 0;
    }
    Items = KAlloc(Count * sizeof(*Items));
    if (Items == NULL)
    {
        return -ENOMEM;
    }
    if (!VMMCopyFromUser(Task->Process->AddressSpace, Items, Vectors, Count * sizeof(*Items)))
    {
        KFree(Items);
        return -EFAULT;
    }
    for (UINT64 Index = 0; Index < Count; Index++)
    {
        if (Items[Index].Length > INT64_MAX - Length)
        {
            KFree(Items);
            return -EINVAL;
        }
        Length += Items[Index].Length;
    }
    BOOLEAN Serialized = File->Device == NULL || File->Device->Seekable;
    INT64   Result     = Serialized ? FileBegin(File) : 0;
    if (Result < 0)
    {
        KFree(Items);
        return Result;
    }
    for (UINT64 Index = 0; Index < Count && Total < 0x7FFFF000; Index++)
    {
        UINT64 Size = Items[Index].Length;
        if (Size == 0)
        {
            continue;
        }
        if (Size > 0x7FFFF000ULL - (UINT64) Total)
        {
            Size = 0x7FFFF000ULL - (UINT64) Total;
        }
        if (Items[Index].Buffer >= (1ULL << 47) || Size > (1ULL << 47) - Items[Index].Buffer)
        {
            Result = -EFAULT;
        }
        else if (File->Device != NULL)
        {
            Result = File->Device->Operations->Write == NULL ? -EINVAL : File->Device->Operations->Write(Task, File->DeviceState, &File->Offset, Items[Index].Buffer, Size);
        }
        else
        {
            Result = FileWrite(Task, File, Items[Index].Buffer, Size);
        }
        if (Result < 0)
        {
            if (Total == 0)
            {
                Total = Result;
            }
            break;
        }
        Total += Result;
        if ((UINT64) Result < Size)
        {
            break;
        }
    }
    if (Serialized)
    {
        FileEnd(File);
    }
    KFree(Items);
    return Total;
}

INT64 SyscallClose(UINT64 FD)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;

    if (Task == NULL || FD >= TASK_FILE_LIMIT || (File = FileDetach(Task->Process, FD)) == NULL)
    {
        return -EBADF;
    }

    return FileRelease(File);
}

VOID SyscallFilesExec(TASK *Task)
{
    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        OPEN_FILE *File = Task->Process->Files[Index];
        if (File != NULL && (Task->Process->CloseOnExecFiles & (1U << Index)) != 0)
        {
            File = FileDetach(Task->Process, Index);
            FileDrop(&File);
        }
    }
}

INT64 FileResolvePath(TASK *Task, INT32 FD, UINT64 Path, BOOLEAN Empty, CHAR8 **Name)
{
    CHAR8 First = 0;
    *Name       = NULL;
    if (Task == NULL || Task->Process->AddressSpace == NULL || Task->Process->FileContext == NULL)
    {
        return -ESRCH;
    }
    if (!(Empty && Path == 0) && !VMMCopyFromUser(Task->Process->AddressSpace, &First, Path, 1))
    {
        return -EFAULT;
    }
    if (First == 0 && !Empty)
    {
        return -ENOENT;
    }
    CHAR8 *Relative = NULL;
    INT64  Result   = First == 0 ? 0 : SyscallCopyPath(Task, Path, &Relative);
    if (Result < 0)
    {
        return Result;
    }
    if (First == '/')
    {
        *Name = Relative;
        return 0;
    }
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FD == FILE_AT_FDCWD ? NULL : FileAcquire(Task, (UINT32) FD);
    if (FD != FILE_AT_FDCWD && File == NULL)
    {
        KFree(Relative);
        return -EBADF;
    }
    if (File != NULL && First != 0 && !File->Directory)
    {
        KFree(Relative);
        return -ENOTDIR;
    }
    CONST CHAR8 *Base = File == NULL ? Task->Process->FileContext->Cwd : File->Path;
    if (File != NULL)
    {
        VFS_NODE Node;
        Result = SyscallVFSError(VFSLookup(Task->Process->FileContext, Base, &Node));
        if (Result == 0 && (Node.Number != File->Number || Node.Partition != File->Partition || Node.Generation != File->Generation))
        {
            Result = -ESTALE;
        }
        if (Result < 0)
        {
            KFree(Relative);
            return Result;
        }
    }
    UINTN Prefix = 0;
    UINTN Length = 0;
    while (Prefix < VFS_MAX_PATH && Base[Prefix] != 0)
    {
        Prefix++;
    }
    while (Relative != NULL && Relative[Length] != 0)
    {
        Length++;
    }
    if (Prefix + Length + 2 > VFS_MAX_PATH)
    {
        KFree(Relative);
        return -ENAMETOOLONG;
    }
    *Name = KAlloc(VFS_MAX_PATH);
    if (*Name == NULL)
    {
        KFree(Relative);
        return -ENOMEM;
    }
    KMemCpy(*Name, Base, Prefix);
    if (First != 0)
    {
        (*Name)[Prefix++] = '/';
        KMemCpy(*Name + Prefix, Relative, Length);
    }
    (*Name)[Prefix + Length] = 0;
    KFree(Relative);
    return 0;
}

INT64 SyscallAccess(UINT64 Path, UINT64 Mode)
{
    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;
    Mode = (UINT32) Mode;
    if ((Mode & ~7ULL) != 0)
    {
        return -EINVAL;
    }
    INT64 Result = SyscallCopyPath(Task, Path, &Name);
    if (Result < 0)
    {
        return Result;
    }
    VFS_CONTEXT *Context = KAlloc(sizeof(*Context));
    if (Context == NULL)
    {
        KFree(Name);
        return -ENOMEM;
    }
    *Context                           = *Task->Process->FileContext;
    Context->Credentials.UID           = Task->Process->RealUID;
    Context->Credentials.GID           = Task->Process->RealGID;
    Context->Credentials.Administrator = Context->Credentials.UID == 0;
    Result                             = SyscallVFSError(VFSLookup(Context, Name, &Node));
    if (Result == 0)
    {
        UINT32 Permission = Context->Credentials.UID == Node.UID ? (Node.Mode >> 6) & 7 : Context->Credentials.GID == Node.GID ? (Node.Mode >> 3) & 7 : Node.Mode & 7;
        if (Context->Credentials.UID == 0)
        {
            Permission = 6 | (((Node.Mode & 0111) != 0 || (Node.Mode & VFS_MODE_TYPE) == VFS_MODE_DIRECTORY) ? 1 : 0);
        }
        if ((Permission & Mode) != Mode)
        {
            Result = -EACCES;
        }
    }
    KFree(Context);
    KFree(Name);
    return Result;
}

INT64 SyscallPread64(UINT64 FD, UINT64 Buffer, UINT64 Count, INT64 Offset)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, FD);
    if (Offset < 0)
    {
        return -EINVAL;
    }
    if (Task == NULL || Task->Process->AddressSpace == NULL || File == NULL || (File->Access & FILE_READ) == 0)
    {
        return -EBADF;
    }
    if (File->Directory)
    {
        return -EISDIR;
    }
    if (File->Device != NULL)
    {
        return -ESPIPE;
    }
    if (Count != 0 && (Buffer >= (1ULL << 47) || Count > (1ULL << 47) - Buffer))
    {
        return -EFAULT;
    }
    if (Count > 0x7FFFF000ULL)
    {
        Count = 0x7FFFF000ULL;
    }
    INT64 Result = FileBegin(File);
    if (Result < 0)
    {
        return Result;
    }
    UINT64 Previous = File->Offset;
    File->Offset    = (UINT64) Offset;
    Result          = FileRead(Task, File, Buffer, Count);
    File->Offset    = Previous;
    FileEnd(File);
    return Result;
}

#define FILE_F_DUPFD         0
#define FILE_F_GETFD         1
#define FILE_F_SETFD         2
#define FILE_F_GETFL         3
#define FILE_F_SETFL         4
#define FILE_F_DUPFD_CLOEXEC 1030
#define FILE_FD_CLOEXEC      1U
#define FILE_OPEN_NONBLOCK   0x800U
#define FILE_OPEN_ASYNC      0x2000U
#define FILE_OPEN_DIRECT     0x4000U

INT64 SyscallFcntl(UINT64 FD, INT32 Command, UINT64 Argument)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, FD);
    if (File == NULL)
    {
        return -EBADF;
    }
    PROCESS *Process = Task->Process;
    if (Command == FILE_F_GETFD || Command == FILE_F_SETFD)
    {
        UINT64 Flags  = SpinLockAcquireIRQSave(&Process->FileTableLock);
        INT64  Result = -EBADF;
        if (Process->Files[FD] == File)
        {
            if (Command == FILE_F_SETFD)
            {
                Process->CloseOnExecFiles = (Process->CloseOnExecFiles & ~(1U << FD)) | ((Argument & FILE_FD_CLOEXEC) != 0 ? 1U << FD : 0);
                Result                    = 0;
            }
            else
            {
                Result = (Process->CloseOnExecFiles & (1U << FD)) != 0 ? FILE_FD_CLOEXEC : 0;
            }
        }
        SpinLockReleaseIRQRestore(&Process->FileTableLock, Flags);
        return Result;
    }
    if (Command == FILE_F_DUPFD || Command == FILE_F_DUPFD_CLOEXEC)
    {
        INT32 Minimum = (INT32) Argument;
        if (Minimum < 0 || Minimum >= TASK_FILE_LIMIT)
        {
            return -EINVAL;
        }
        UINT64 Flags = SpinLockAcquireIRQSave(&Process->FileTableLock);
        UINTN  Index;
        for (Index = (UINTN) Minimum; Index < TASK_FILE_LIMIT; Index++)
        {
            if (Process->Files[Index] == NULL && (Process->ReservedFiles & (1U << Index)) == 0)
            {
                ATOMIC_FETCH_ADD(&File->References, 1, ATOMIC_RELAXED);
                Process->Files[Index]     = File;
                Process->CloseOnExecFiles = (Process->CloseOnExecFiles & ~(1U << Index)) | (Command == FILE_F_DUPFD_CLOEXEC ? 1U << Index : 0);
                break;
            }
        }
        SpinLockReleaseIRQRestore(&Process->FileTableLock, Flags);
        return Index == TASK_FILE_LIMIT ? -EMFILE : (INT64) Index;
    }
    if (Command == FILE_F_GETFL || Command == FILE_F_SETFL)
    {
        INT64 Result = FileBegin(File);
        if (Result < 0)
        {
            return Result;
        }
        Result = File->Flags & (VFS_OPEN_ACCMODE | VFS_OPEN_APPEND | FILE_OPEN_NONBLOCK | FILE_OPEN_LARGEFILE);
        if (Command == FILE_F_SETFL)
        {
            if ((Argument & (FILE_OPEN_ASYNC | FILE_OPEN_DIRECT)) != 0 || (File->Device != NULL && (Argument & FILE_OPEN_NONBLOCK) != 0))
            {
                Result = -EOPNOTSUPP;
            }
            else
            {
                File->Flags  = (File->Flags & ~(VFS_OPEN_APPEND | FILE_OPEN_NONBLOCK)) | ((UINT32) Argument & (VFS_OPEN_APPEND | FILE_OPEN_NONBLOCK));
                File->Append = (File->Flags & VFS_OPEN_APPEND) != 0;
                Result       = 0;
            }
        }
        FileEnd(File);
        return Result;
    }
    return -EINVAL;
}

INT64 SyscallPoll(UINT64 Descriptors, UINT64 Count, INT32 Timeout)
{
    typedef struct
    {
        INT32 FD;
        INT16 Events;
        INT16 Returned;
    } FILE_POLL;
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }
    if (Count > TASK_FILE_LIMIT)
    {
        return -EINVAL;
    }
    if (Timeout != 0)
    {
        return -EOPNOTSUPP;
    }
    if (Count == 0)
    {
        return 0;
    }
    FILE_POLL *Items = KAlloc(Count * sizeof(*Items));
    if (Items == NULL)
    {
        return -ENOMEM;
    }
    INT64 Result = -EFAULT;
    if (!VMMCopyFromUser(Task->Process->AddressSpace, Items, Descriptors, Count * sizeof(*Items)))
    {
        goto Done;
    }
    Result = 0;
    for (UINT64 Index = 0; Index < Count; Index++)
    {
        if (Items[Index].Events != 0)
        {
            Result = -EOPNOTSUPP;
            goto Done;
        }
        OPEN_FILE *File __attribute__((cleanup(FileDrop))) = Items[Index].FD >= 0 ? FileAcquire(Task, (UINT32) Items[Index].FD) : NULL;
        Items[Index].Returned                              = Items[Index].FD >= 0 && File == NULL ? 0x20 : 0;
        Result += Items[Index].Returned != 0;
    }
    if (!VMMCopyToUser(Task->Process->AddressSpace, Descriptors, Items, Count * sizeof(*Items)))
    {
        Result = -EFAULT;
    }
Done:
    KFree(Items);
    return Result;
}
