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

    Info.Device     = Node->Partition + 1;
    Info.Inode      = Node->Number;
    Info.Links      = 1;
    Info.Mode       = Node->Mode;
    Info.UID        = Node->UID;
    Info.GID        = Node->GID;
    Info.RDevice    = (Node->Mode & VFS_MODE_TYPE) == 0x2000U ? Node->Number : 0;
    Info.Size       = Node->Size;
    Info.BlockSize  = 4096;
    Info.Blocks     = (Node->Size + 511) / 512;
    Info.AccessTime = Node->AccessTime;
    Info.ModifyTime = Node->ModifyTime;
    Info.ChangeTime = Node->ChangeTime;

    return VMMCopyToUser(Task->Process->AddressSpace, Buffer, &Info, sizeof(Info)) ? 0 : -EFAULT;
}

INT64 SyscallStat(SYSCALL_CONTEXT *Context)
{
    UINT64  Path   = Context->Args.RDI;
    UINT64  Buffer = Context->Args.RSI;
    BOOLEAN Follow = Context->Number == SYSCALL_STAT;

    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;

    if (Task == NULL || Task->Process->AddressSpace == NULL || Task->Process->FileContext == NULL)
    {
        return -ESRCH;
    }

    INT64 Result = SyscallCopyPath(Task, Path, &Name);

    if (Result < 0)
    {
        return Result;
    }

    Result = SyscallVFSError(Follow ? VFSLookup(Task->Process->FileContext, Name, &Node) : VFSLookupNoFollow(Task->Process->FileContext, Name, &Node));
    KFree(Name);

    return Result < 0 ? Result : FileStat(Task, &Node, Buffer);
}

INT64 SyscallFstat(SYSCALL_CONTEXT *Context)
{
    UINT64 FD     = Context->Args.RDI;
    UINT64 Buffer = Context->Args.RSI;

    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, (UINT64) FD);
    VFS_NODE   Node;

    if (Task == NULL || File == NULL)
    {
        return -EBADF;
    }

    INT64 Result = SyscallVFSError(VFSLookup(Task->Process->FileContext, File->Path, &Node));

    if (Result < 0)
    {
        return Result;
    }

    if (Node.Partition != File->Partition || Node.Number != File->Number || (File->Device == NULL && Node.Generation != File->Generation))
    {
        return -ESTALE;
    }

    return FileStat(Task, &Node, Buffer);
}

INT64 SyscallNewfstatat(SYSCALL_CONTEXT *Context)
{
    INT32  FD     = (INT32) Context->Args.RDI;
    UINT64 Path   = Context->Args.RSI;
    UINT64 Buffer = Context->Args.RDX;
    UINT64 Flags  = Context->Args.R10;

    TASK    *Task = LBPWRRGetCurrentTask();
    CHAR8   *Name;
    VFS_NODE Node;
    if (Task == NULL || Task->Process->AddressSpace == NULL || Task->Process->FileContext == NULL)
    {
        return -ESRCH;
    }
    Flags = (UINT32) Flags;
    if ((Flags & ~((UINT64) FILE_AT_SYMLINK_NOFOLLOW | FILE_AT_NO_AUTOMOUNT | FILE_AT_EMPTY_PATH)) != 0)
    {
        return -EINVAL;
    }
    if ((Flags & FILE_AT_EMPTY_PATH) != 0)
    {
        CHAR8 First = 0;
        if (Path != 0 && !VMMCopyFromUser(Task->Process->AddressSpace, &First, Path, 1))
        {
            return -EFAULT;
        }
        if (First == 0 && FD != FILE_AT_FDCWD)
        {
            return SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) ((UINT32) FD), .Args.RSI = (UINT64) (Buffer)});
        }
    }
    INT64 Result = FileResolvePath(Task, FD, Path, (Flags & FILE_AT_EMPTY_PATH) != 0, &Name);
    if (Result < 0)
    {
        return Result;
    }
    Result = SyscallVFSError((Flags & FILE_AT_SYMLINK_NOFOLLOW) != 0 ? VFSLookupNoFollow(Task->Process->FileContext, Name, &Node) : VFSLookup(Task->Process->FileContext, Name, &Node));
    KFree(Name);
    return Result < 0 ? Result : FileStat(Task, &Node, Buffer);
}
