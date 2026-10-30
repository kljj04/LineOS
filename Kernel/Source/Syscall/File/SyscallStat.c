// Kernel/Source/Syscall/File/SyscallStat.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/SyscallStat.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

#include <Syscall/File/Stat.h>

_Static_assert(sizeof(STAT_INFO) == 144, "stat ABI");

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

