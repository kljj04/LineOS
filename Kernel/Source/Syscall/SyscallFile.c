// Kernel/Source/Syscall/SyscallFile.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallFile.h>
#include <Syscall/SyscallError.h>
#include <FS/DeviceFS/DeviceFS.h>
#include <Scheduler/WaitQueue.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>
#include <LineOS/Memory.h>
#include <LineOS/Stat.h>

_Static_assert(sizeof(STAT_INFO) == 144, "stat ABI");

#define FILE_READ        1U
#define FILE_WRITE       2U
#define FILE_BUFFER_SIZE (1024U * 1024U)

struct OPEN_FILE
{
    UINT32             References;
    UINT32             Busy;
    INT64              Failed;
    SPIN_LOCK          Lock;
    WAIT_QUEUE         Waiters;
    UINT32             Access;
    BOOLEAN            Directory;
    UINT64             Offset;
    UINT64             NamespaceGeneration;
    UINT32             Number;
    UINT32             Partition;
    CHAR8             *Path;
    VFS_CONTEXT        OpenContext;
    CONST DEVICE_NODE *Device;
    VOID              *DeviceState;
};

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
    if (Flags > UINT32_MAX || (Flags & ~(VFS_OPEN_ACCMODE | VFS_OPEN_CREATE | VFS_OPEN_EXCLUSIVE | VFS_OPEN_TRUNCATE)) != 0 || (Flags & VFS_OPEN_ACCMODE) == 3 || ((Flags & VFS_OPEN_CREATE) != 0 && Mode > 0777))
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

STATIC INT64 FileBegin(OPEN_FILE *File)
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

STATIC VOID FileEnd(OPEN_FILE *File)
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
    if (File->Offset > UINT64_MAX - Length)
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
    INT64 Result = SyscallVFSError(VFSWriteChecked(Task->FileContext, File->Path, &Node, File->Offset, Bytes, Length));
    KFree(Bytes);
    if (Result == 0)
    {
        File->Offset += Length;
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

INT64 SyscallIoctl(UINT64 FD, UINT64 Request, UINT64 Argument)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    if (Task == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -EBADF;
    }
    if (File->Device == NULL || File->Device->Operations->Ioctl == NULL)
    {
        return -ENOTTY;
    }
    return File->Device->Operations->Ioctl(Task, File->DeviceState, Request, Argument, File->Access);
}

INT64 SyscallRename(UINT64 OldPath, UINT64 NewPath)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *OldName;
    CHAR8 *NewName;
    INT64  Result = SyscallCopyPath(Task, OldPath, &OldName);
    if (Result < 0)
    {
        return Result;
    }
    Result = SyscallCopyPath(Task, NewPath, &NewName);
    if (Result == 0)
    {
        Result = SyscallVFSError(VFSRename(Task->FileContext, OldName, NewName));
        KFree(NewName);
    }
    KFree(OldName);
    return Result;
}

INT64 SyscallRemove(UINT64 Path)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;
    INT64  Result = SyscallCopyPath(Task, Path, &Name);
    if (Result == 0)
    {
        Result = SyscallVFSError(VFSRemove(Task->FileContext, Name));
        KFree(Name);
    }
    return Result;
}

INT64 SyscallMkdir(UINT64 Path, UINT64 Mode)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;
    if ((Mode & ~07777ULL) != 0)
    {
        return -EINVAL;
    }
    INT64 Result = SyscallCopyPath(Task, Path, &Name);
    if (Result == 0)
    {
        Result = SyscallVFSError(VFSCreate(Task->FileContext, Name, TRUE, (UINT16) (Mode & 0777)));
        KFree(Name);
    }
    return Result;
}

INT64 SyscallMapDevice(INT64 FD, UINT64 Length, UINT64 Protection, UINT64 Flags, UINT64 Offset, UINT64 *Physical)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    if (Task == NULL || FD < 0 || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -EBADF;
    }
    if (File->Device == NULL || File->Device->Operations->Map == NULL)
    {
        return -ENODEV;
    }
    if ((File->Access & FILE_READ) == 0 || ((Protection & PROT_WRITE) != 0 && (File->Access & FILE_WRITE) == 0))
    {
        return -EACCES;
    }
    return File->Device->Operations->Map(Task, File->DeviceState, Length, Protection, Flags, Offset, Physical);
}

INT64 SyscallGetDents64(UINT64 FD, UINT64 Buffer, UINT64 Count)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    if (Task == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -EBADF;
    }
    if (!File->Directory)
    {
        return -ENOTDIR;
    }
    if (Count > 0x7FFFFFFFULL)
    {
        Count = 0x7FFFFFFFULL;
    }
    if (Buffer >= (1ULL << 47) || Count > (1ULL << 47) - Buffer)
    {
        return -EFAULT;
    }
    INT64 Error = FileBegin(File);
    if (Error < 0)
    {
        return Error;
    }
    UINT64 Total = 0;
    for (;;)
    {
        VFS_DIRECTORY_ENTRY Entry;
        UINT64              Cursor = File->Offset;
        Error = SyscallVFSError(VFSReadDirectory(Task->FileContext, File->Path, &Cursor, &Entry));
        if (Error < 0 || Entry.End)
        {
            break;
        }
        UINT32 Length = 0;
        while (Length < 255 && Entry.Name[Length] != 0)
        {
            Length++;
        }
        UINT16 RecordLength = (UINT16) ((19 + Length + 1 + 7) & ~7U);
        if (RecordLength > Count - Total)
        {
            Error = -EINVAL;
            break;
        }
        UINT8  Record[280] = {0};
        UINT64 Number = Entry.Node.Number;
        KMemCpy(Record, &Number, 8);
        KMemCpy(Record + 8, &Cursor, 8);
        KMemCpy(Record + 16, &RecordLength, 2);
        Record[18] = (UINT8) ((Entry.Node.Mode & VFS_MODE_TYPE) >> 12);
        KMemCpy(Record + 19, Entry.Name, Length + 1);
        if (!VMMCopyToUser(Task->AddressSpace, Buffer + Total, Record, RecordLength))
        {
            Error = -EFAULT;
            break;
        }
        File->Offset = Cursor;
        Total += RecordLength;
    }
    FileEnd(File);
    return Total != 0 ? (INT64) Total : Error;
}

STATIC INT64 FileStat(TASK *Task, CONST VFS_NODE *Node, UINT64 Buffer)
{
    STAT_INFO Info = {0};
    Info.Device = Node->Partition + 1;
    Info.Inode = Node->Number;
    Info.Links = 1;
    Info.Mode = Node->Mode;
    Info.UID = Node->UID;
    Info.GID = Node->GID;
    Info.RDevice = (Node->Mode & VFS_MODE_TYPE) == 0x2000U ? Node->Number : 0;
    Info.Size = Node->Size;
    Info.BlockSize = 4096;
    Info.Blocks = (Node->Size + 511) / 512;
    Info.AccessTime = Node->AccessTime;
    Info.ModifyTime = Node->ModifyTime;
    Info.ChangeTime = Node->ChangeTime;
    return VMMCopyToUser(Task->AddressSpace, Buffer, &Info, sizeof(Info)) ? 0 : -EFAULT;
}

INT64 SyscallStat(UINT64 Path, UINT64 Buffer, BOOLEAN Follow)
{
    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;
    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }
    INT64 Result = SyscallCopyPath(Task, Path, &Name);
    if (Result < 0)
    {
        return Result;
    }
    Result = SyscallVFSError(Follow ? VFSLookup(Task->FileContext, Name, &Node) : VFSLookupNoFollow(Task->FileContext, Name, &Node));
    KFree(Name);
    return Result < 0 ? Result : FileStat(Task, &Node, Buffer);
}

INT64 SyscallFstat(UINT64 FD, UINT64 Buffer)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    VFS_NODE   Node;
    if (Task == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -EBADF;
    }
    INT64 Result = SyscallVFSError(VFSLookup(Task->FileContext, File->Path, &Node));
    if (Result < 0)
    {
        return Result;
    }
    if (Node.Partition != File->Partition || Node.Number != File->Number || (File->Device == NULL && Node.NamespaceGeneration != File->NamespaceGeneration))
    {
        return -ESTALE;
    }
    return FileStat(Task, &Node, Buffer);
}

INT64 SyscallChdir(UINT64 Path)
{
    TASK  *Task = LBPWRRGetCurrentTask();
    CHAR8 *Name;
    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }
    INT64 Result = SyscallCopyPath(Task, Path, &Name);
    if (Result < 0)
    {
        return Result;
    }
    Result = SyscallVFSError(VFSChangeDirectory(Task->FileContext, Name));
    KFree(Name);
    return Result;
}

INT64 SyscallGetCwd(UINT64 Buffer, UINT64 Size)
{
    TASK *Task = LBPWRRGetCurrentTask();
    CHAR8 Path[VFS_MAX_PATH];
    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL)
    {
        return -ESRCH;
    }
    if (Size == 0)
    {
        return -EINVAL;
    }
    INT64 Result = SyscallVFSError(VFSGetCwd(Task->FileContext, Path, sizeof(Path)));
    if (Result < 0)
    {
        return Result;
    }
    UINT64 Length = 0;
    while (Path[Length] != 0)
    {
        Length++;
    }
    Length++;
    if (Length > Size)
    {
        return -ERANGE;
    }
    return VMMCopyToUser(Task->AddressSpace, Buffer, Path, Length) ? (INT64) Length : -EFAULT;
}

#include <LineOS/Time.h>

INT64 SyscallUtimensat(INT64 DirFD, UINT64 Path, UINT64 TimesAddress, UINT64 Flags)
{
    TASK *Task = LBPWRRGetCurrentTask();
    TIME_SPEC Times[2] = {{0, LOS_UTIME_NOW}, {0, LOS_UTIME_NOW}};
    CHAR8 *Name = NULL;
    VFS_NODE Expected = {0};
    VFS_NODE *ExpectedPointer = NULL;
    if (Task == NULL || Task->FileContext == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }
    if ((Flags & ~0x100ULL) != 0 || (Path == 0 && Flags != 0))
    {
        return -EINVAL;
    }
    if (TimesAddress != 0 && !VMMCopyFromUser(Task->AddressSpace, Times, TimesAddress, sizeof(Times)))
    {
        return -EFAULT;
    }
    for (UINTN Index = 0; Index < 2; Index++)
    {
        if (Times[Index].Nanoseconds == LOS_UTIME_NOW || Times[Index].Nanoseconds == LOS_UTIME_OMIT)
        {
            continue;
        }
        if (Times[Index].Nanoseconds < 0 || Times[Index].Nanoseconds >= 1000000000)
        {
            return -EINVAL;
        }
        if (Times[Index].Seconds < 0 || Times[Index].Seconds > UINT32_MAX)
        {
            return -EOVERFLOW;
        }
    }
    VFS_CONTEXT Context = *Task->FileContext;
    INT64 Result = 0;
    if (Path != 0)
    {
        Result = SyscallCopyPath(Task, Path, &Name);
        if (Result < 0)
        {
            return Result;
        }
    }
    if (Path == 0 || (Name[0] != '/' && DirFD != -100))
    {
        if (DirFD < 0 || DirFD >= TASK_FILE_LIMIT || Task->Files[DirFD] == NULL)
        {
            KFree(Name);
            return -EBADF;
        }
        OPEN_FILE *File = Task->Files[DirFD];
        Context = File->OpenContext;
        Context.Credentials = Task->FileContext->Credentials;
        Expected.NamespaceGeneration = File->NamespaceGeneration;
        Expected.Partition = File->Partition;
        Expected.Number = File->Number;
        if (Path == 0)
        {
            ExpectedPointer = &Expected;
            return SyscallVFSError(VFSSetTimes(&Context, File->Path, ExpectedPointer, TRUE, Times));
        }
        if (!File->Directory)
        {
            KFree(Name);
            return -ENOTDIR;
        }
        VFS_NODE Directory;
        Result = SyscallVFSError(VFSLookup(&Context, File->Path, &Directory));
        if (Result == 0 && (Directory.Number != Expected.Number || Directory.Partition != Expected.Partition || Directory.NamespaceGeneration != Expected.NamespaceGeneration))
        {
            Result = -ESTALE;
        }
        if (Result == 0)
        {
            Result = SyscallVFSError(VFSChangeDirectory(&Context, File->Path));
        }
    }
    if (Result == 0)
    {
        Result = SyscallVFSError(VFSSetTimes(&Context, Name, NULL, (Flags & 0x100) == 0, Times));
    }
    KFree(Name);
    return Result;
}
