// Kernel/Source/Syscall/SyscallFile.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallFile.h>
#include <Syscall/SyscallTerminal.h>
#include <Scheduler/WaitQueue.h>
#include <Scheduler/LBPWRR.h>
#include <FS/VFS/VFS.h>
#include <Memory/PMM.h>

#define FILE_TERMINAL 1U
#define FILE_MASTER   2U
#define FILE_READ     1U
#define FILE_WRITE    2U

struct OPEN_FILE
{
    UINT32     References;
    UINT32     Busy;
    SPIN_LOCK  Lock;
    WAIT_QUEUE Waiters;
    UINT32     Kind;
    UINT32     Access;
    UINT64     Offset;
    UINT32     Number;
    UINT32     Partition;
    CHAR8     *Path;
};

STATIC BOOLEAN FileNameEqual(CONST CHAR8 *Left, CONST char *Right)
{
    while (*Left != 0 && *Left == *Right)
    {
        Left++;
        Right++;
    }
    return *Left == *Right;
}

VOID SyscallFilesRelease(TASK *Task)
{
    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        OPEN_FILE *File = Task->Files[Index];
        Task->Files[Index] = NULL;
        if (File != NULL && ATOMIC_FETCH_SUB(&File->References, 1, ATOMIC_ACQ_REL) == 1)
        {
            if (File->Kind == FILE_MASTER)
            {
                TerminalDetach();
            }
            if (File->Path != NULL)
            {
                KFree(File->Path);
            }
            KFree(File);
        }
    }
}

BOOLEAN SyscallFilesInherit(TASK *Child, TASK *Parent)
{
    for (UINTN Index = 0; Index < TASK_FILE_LIMIT; Index++)
    {
        if (Parent != NULL)
        {
            OPEN_FILE *File = Parent->Files[Index];
            if (File != NULL)
            {
                ATOMIC_FETCH_ADD(&File->References, 1, ATOMIC_RELAXED);
                Child->Files[Index] = File;
            }
        }
    }
    return TRUE;
}

INT64 SyscallOpen(UINT64 Path, UINT64 Flags)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    CHAR8     *Name;
    VFS_NODE   Node;
    UINTN      Index;
    UINT32     ReadLength;
    OPEN_FILE *File;

    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL || Path == 0 || Flags > 2)
    {
        return -1;
    }
    for (Index = 0; Index < TASK_FILE_LIMIT && Task->Files[Index] != NULL; Index++)
    {
    }
    if (Index == TASK_FILE_LIMIT || (Name = KAlloc(VFS_MAX_PATH)) == NULL)
    {
        return -1;
    }
    for (UINTN Position = 0; Position < VFS_MAX_PATH; Position++)
    {
        if (Path > UINT64_MAX - Position || !VMMCopyFromUser(Task->AddressSpace, &Name[Position], Path + Position, 1))
        {
            break;
        }
        if (Name[Position] == 0)
        {
            UINT32 Kind = FileNameEqual(Name, "/dev/lsh1") ? FILE_TERMINAL : (FileNameEqual(Name, "/dev/lsh1/master") ? FILE_MASTER : 0);
            if (Position == 0 || (Kind == FILE_MASTER && !Task->AssetPublisher) || (Kind == 0 && (Flags != 0 || VFSLookup(Task->FileContext, Name, &Node) != VFS_OK || (Node.Mode & VFS_MODE_TYPE) != 0x8000U || VFSRead(Task->FileContext, Name, 0, NULL, 0, &ReadLength) != VFS_OK)))
            {
                break;
            }
            File = KAlloc(sizeof(*File));
            if (File == NULL)
            {
                break;
            }
            if (Kind == FILE_MASTER && !TerminalAttach())
            {
                KFree(File);
                break;
            }
            KMemSet(File, 0, sizeof(*File));
            File->References = 1;
            File->Kind = Kind;
            File->Access = Flags == 0 ? FILE_READ : (Flags == 1 ? FILE_WRITE : FILE_READ | FILE_WRITE);
            File->Path = Name;
            if (Kind == 0)
            {
                File->Number = Node.Number;
                File->Partition = Node.Partition;
            }
            Task->Files[Index] = File;
            return (INT64) Index;
        }
    }
    KFree(Name);
    return -1;
}

#define SYSCALL_READ_BUFFER_SIZE (1024U * 1024U)

STATIC INT64 FileRead(TASK *Task, OPEN_FILE *File, UINT64 Buffer, UINT64 Count)
{
    VFS_NODE Node;
    UINT8   *Bytes;
    UINT32   BufferSize;
    UINT64   Total = 0;

    if (Count == 0)
    {
        return 0;
    }

    if (Buffer == 0 || Buffer > UINT64_MAX - Count)
    {
        return -1;
    }

    Node.Number = File->Number;
    Node.Partition = File->Partition;
    BufferSize = Count > SYSCALL_READ_BUFFER_SIZE ? SYSCALL_READ_BUFFER_SIZE : (UINT32) Count;
    Bytes = KAlloc(BufferSize);

    if (Bytes == NULL)
    {
        return -1;
    }

    while (Total < Count)
    {
        UINT32 Length = Count - Total > BufferSize ? BufferSize : (UINT32) (Count - Total);
        UINT32 ReadLength = 0;

        if (VFSReadChecked(Task->FileContext, File->Path, &Node, File->Offset, Bytes, Length, &ReadLength) != VFS_OK || ReadLength > Length)
        {
            KFree(Bytes);
            return Total != 0 ? (INT64) Total : -1;
        }

        if (ReadLength == 0)
        {
            break;
        }

        if (!VMMCopyToUser(Task->AddressSpace, Buffer + Total, Bytes, ReadLength))
        {
            KFree(Bytes);
            return Total != 0 ? (INT64) Total : -1;
        }

        File->Offset += ReadLength;
        Total += ReadLength;

        if (ReadLength < Length)
        {
            break;
        }
    }

    KFree(Bytes);

    return (INT64) Total;
}

INT64 SyscallRead(UINT64 FD, UINT64 Buffer, UINT64 Count)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    INT64      Result;
    if (Task == NULL || Task->AddressSpace == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL || (File->Access & FILE_READ) == 0 || Count > 0x7FFFFFFFFFFFFFFFULL || (Count != 0 && (Buffer == 0 || Buffer > UINT64_MAX - Count)))
    {
        return -1;
    }
    if (File->Kind != 0)
    {
        return TerminalRead(Task, File->Kind == FILE_MASTER, Buffer, Count);
    }
    if (Task->FileContext == NULL)
    {
        return -1;
    }
    UINT64 Flags = SpinLockAcquireIRQSave(&File->Lock);
    while (File->Busy != 0)
    {
        if (!WaitQueueSleep(&File->Waiters, &File->Lock))
        {
            SpinLockReleaseIRQRestore(&File->Lock, Flags);
            return -1;
        }
    }
    File->Busy = 1;
    SpinLockReleaseIRQRestore(&File->Lock, Flags);
    Result = FileRead(Task, File, Buffer, Count);
    Flags = SpinLockAcquireIRQSave(&File->Lock);
    File->Busy = 0;
    WaitQueueWakeAll(&File->Waiters);
    SpinLockReleaseIRQRestore(&File->Lock, Flags);
    return Result;
}

INT64 SyscallFileWrite(UINT64 FD, UINT64 Buffer, UINT64 Count)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    if (Task == NULL || Task->AddressSpace == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL || (File->Access & FILE_WRITE) == 0 || File->Kind == 0 || Count > 0x7FFFFFFFFFFFFFFFULL || (Count != 0 && (Buffer == 0 || Buffer > UINT64_MAX - Count)))
    {
        return -1;
    }
    return TerminalWrite(Task, File->Kind == FILE_MASTER, Buffer, Count);
}

INT64 SyscallClose(UINT64 FD)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;
    if (Task == NULL || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -1;
    }
    Task->Files[FD] = NULL;
    if (ATOMIC_FETCH_SUB(&File->References, 1, ATOMIC_ACQ_REL) == 1)
    {
        if (File->Kind == FILE_MASTER)
        {
            TerminalDetach();
        }
        if (File->Path != NULL)
        {
            KFree(File->Path);
        }
        KFree(File);
    }
    return 0;
}
