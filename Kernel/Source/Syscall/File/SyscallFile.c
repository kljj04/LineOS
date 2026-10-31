// Kernel/Source/Syscall/File/SyscallFile.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

#include <Syscall/Memory/Memory.h>

#define FILE_BUFFER_SIZE (1024U * 1024U)

STATIC INT64 FileRelease(OPEN_FILE *File)
{
    INT64 Result = File->Failed;

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

VOID SyscallFilesRelease(TASK *Task)
{
    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        OPEN_FILE *File = Task->Files[Index];

        Task->Files[Index] = NULL;

        if (File != NULL)
        {
            FileRelease(File);
        }
    }
}

BOOLEAN SyscallFilesInherit(TASK *Child, TASK *Parent)
{
    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        if (Parent != NULL && Parent->Files[Index] != NULL)
        {
            OPEN_FILE *File = Parent->Files[Index];

            ATOMIC_FETCH_ADD(&File->References, 1, ATOMIC_RELAXED);
            Child->Files[Index] = File;
        }
    }

    return TRUE;
}

INT64 SyscallOpen(UINT64 Path, UINT64 Flags, UINT64 Mode)
{
    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;
    UINTN    Index;
    INT64    Result;

    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }

    if (Flags > UINT32_MAX || (Flags & ~(VFS_OPEN_ACCMODE | VFS_OPEN_CREATE | VFS_OPEN_EXCLUSIVE | VFS_OPEN_TRUNCATE | VFS_OPEN_APPEND)) != 0 || (Flags & VFS_OPEN_ACCMODE) == 3 || ((Flags & VFS_OPEN_CREATE) != 0 && Mode > 0777))
    {
        return -EINVAL;
    }

    Result = SyscallCopyPath(Task, Path, &Name);

    if (Result < 0)
    {
        return Result;
    }

    for (Index = 0; Index < TASK_FILE_LIMIT && Task->Files[Index] != NULL; Index++)
    {
    }

    if (Index == TASK_FILE_LIMIT)
    {
        KFree(Name);

        return -EMFILE;
    }

    OPEN_FILE *File = KAlloc(sizeof(*File));

    if (File == NULL)
    {
        KFree(Name);

        return -ENOMEM;
    }

    KMemSet(File, 0, sizeof(*File));
    Result = SyscallVFSError(VFSOpen(Task->FileContext, Name, (UINT32) Flags, (UINT16) Mode, &Node));

    if (Result < 0)
    {
        goto Failed;
    }

    File->Append = (Flags & VFS_OPEN_APPEND) != 0;
    File->Access = (Flags & VFS_OPEN_ACCMODE) == 0 ? FILE_READ : (Flags & VFS_OPEN_ACCMODE) == 1 ? FILE_WRITE : FILE_READ | FILE_WRITE;

    if ((Node.Mode & VFS_MODE_TYPE) == 0x2000U)
    {
        File->Device = Node.Partition == Task->FileContext->Registry->DevicePartition ? DeviceFSGetNode(Node.Number) : NULL;

        if (File->Device == NULL)
        {
            Result = -ENXIO;

            goto Failed;
        }

        UINT32 Permission = Task->FileContext->Credentials.UID == Node.UID ? (Node.Mode >> 6) & 7 : Task->FileContext->Credentials.GID == Node.GID ? (Node.Mode >> 3) & 7 : Node.Mode & 7;

        if (Task->FileContext->Credentials.UID != 0 && (((File->Access & FILE_READ) != 0 && (Permission & 4) == 0) || ((File->Access & FILE_WRITE) != 0 && (Permission & 2) == 0)))
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

        Result = SyscallVFSError(VFSReadDirectory(Task->FileContext, Name, &Cursor, &Entry));

        if (Result < 0)
        {
            goto Failed;
        }
    }

    File->References = 1;
    File->Path = Name;
    File->OpenContext = *Task->FileContext;
    File->NamespaceGeneration = Node.NamespaceGeneration;
    File->Number = Node.Number;
    File->Partition = Node.Partition;
    Task->Files[Index] = File;

    return (INT64) Index;
Failed:
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
    VFS_NODE Node = {0};
    UINT64   Total = 0;
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

    Node.NamespaceGeneration = File->NamespaceGeneration;
    Node.Number = File->Number;
    Node.Partition = File->Partition;

    INT64 Error = 0;

    while (Total < Count)
    {
        UINT32 Length = Count - Total > BufferSize ? BufferSize : (UINT32) (Count - Total);
        UINT32 ReadLength = 0;

        Error = SyscallVFSError(VFSReadChecked(Task->FileContext, File->Path, &Node, File->Offset, Bytes, Length, &ReadLength));

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

            if (!VMMCopyToUser(Task->AddressSpace, Buffer + Total, Bytes + Copied, Chunk))
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

    if (File->Failed != 0)
    {
        return File->Failed;
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

        if (!VMMCopyFromUser(Task->AddressSpace, Bytes + Copied, Buffer + Copied, Chunk))
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

    Length = Copied;
    Node.NamespaceGeneration = File->NamespaceGeneration;
    Node.Number = File->Number;
    Node.Partition = File->Partition;

    UINT64 EndOffset = File->Offset + Length;
    INT64  Result;

    if (File->Append)
    {
        Result = SyscallVFSError(VFSAppendChecked(Task->FileContext, File->Path, &Node, Bytes, Length, &EndOffset));
    }
    else
    {
        Result = SyscallVFSError(VFSWriteChecked(Task->FileContext, File->Path, &Node, File->Offset, Bytes, Length));
    }

    KFree(Bytes);

    if (Result == 0)
    {
        File->Offset = EndOffset;

        return Length;
    }

    if (Result == -EIO || Result == -EROFS)
    {
        File->Failed = Result;
    }

    return Result;
}

STATIC INT64 FileData(UINT64 FD, UINT64 Buffer, UINT64 Count, BOOLEAN Write)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;

    if (Task == NULL || Task->AddressSpace == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL || (File->Access & (Write ? FILE_WRITE : FILE_READ)) == 0)
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
    INT64   Result = Serialized ? FileBegin(File) : 0;

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

INT64 SyscallClose(UINT64 FD)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;

    if (Task == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -EBADF;
    }

    Task->Files[FD] = NULL;

    return FileRelease(File);
}
