// Kernel/Source/Syscall/SyscallFile.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallFile.h>
#include <Scheduler/LBPWRR.h>
#include <FS/VFS/VFS.h>
#include <Memory/PMM.h>

INT64 SyscallOpen(UINT64 Path, UINT64 Flags)
{
    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;
    UINTN    Index;
    UINT32   ReadLength;

    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL || Path == 0 || Flags != 0)
    {
        return -1;
    }
    for (Index = 0; Index < TASK_FILE_LIMIT && Task->Files[Index].Path != NULL; Index++)
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
            KFree(Name);
            return -1;
        }
        if (Name[Position] == 0)
        {
            if (Position == 0 || VFSLookup(Task->FileContext, Name, &Node) != VFS_OK || (Node.Mode & VFS_MODE_TYPE) != 0x8000U)
            {
                KFree(Name);
                return -1;
            }
            if (VFSRead(Task->FileContext, Name, 0, NULL, 0, &ReadLength) != VFS_OK)
            {
                KFree(Name);
                return -1;
            }
            Task->Files[Index].Path = Name;
            Task->Files[Index].Offset = 0;
            Task->Files[Index].Number = Node.Number;
            Task->Files[Index].Partition = Node.Partition;
            return (INT64) Index + 3;
        }
    }
    KFree(Name);
    return -1;
}

#define SYSCALL_READ_BUFFER_SIZE (1024U * 1024U)

INT64 SyscallRead(UINT64 FD, UINT64 Buffer, UINT64 Count)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    TASK_FILE *File;
    VFS_NODE   Node;
    UINT8     *Bytes;
    UINT32     BufferSize;
    UINT64     Total = 0;

    if (Task == NULL || Task->AddressSpace == NULL || Task->FileContext == NULL || FD < 3 || FD - 3 >= TASK_FILE_LIMIT || Count > 0x7FFFFFFFFFFFFFFFULL)
    {
        return -1;
    }

    File = &Task->Files[FD - 3];

    if (File->Path == NULL)
    {
        return -1;
    }

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

INT64 SyscallClose(UINT64 FD)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    TASK_FILE *File;

    if (Task == NULL || FD < 3 || FD - 3 >= TASK_FILE_LIMIT)
    {
        return -1;
    }
    File = &Task->Files[FD - 3];
    if (File->Path == NULL)
    {
        return -1;
    }
    KFree(File->Path);
    KMemSet(File, 0, sizeof(*File));
    return 0;
}
