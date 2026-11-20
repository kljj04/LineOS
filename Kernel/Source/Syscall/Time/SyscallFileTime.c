// Kernel/Source/Syscall/Time/SyscallFileTime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Time/SyscallFileTime.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

#include <Syscall/Time/Time.h>

INT64 SyscallUtimensat(SYSCALL_CONTEXT *Context)
{
    INT64  DirFD        = (INT64) Context->Args.RDI;
    UINT64 Path         = Context->Args.RSI;
    UINT64 TimesAddress = Context->Args.RDX;
    UINT64 Flags        = Context->Args.R10;

    TASK     *Task     = LBPWRRGetCurrentTask();
    TIME_SPEC Times[2] = {
        {0, LOS_UTIME_NOW},
        {0, LOS_UTIME_NOW}
    };
    CHAR8    *Name            = NULL;
    VFS_NODE  Expected        = {0};
    VFS_NODE *ExpectedPointer = NULL;

    if (Task == NULL || Task->Process->FileContext == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if ((Flags & ~0x100ULL) != 0 || (Path == 0 && Flags != 0))
    {
        return -EINVAL;
    }

    if (TimesAddress != 0 && !VMMCopyFromUser(Task->Process->AddressSpace, Times, TimesAddress, sizeof(Times)))
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

    VFS_CONTEXT FileContext = *Task->Process->FileContext;
    INT64       Result      = 0;

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
        OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, (UINT64) DirFD);

        if (File == NULL)
        {
            KFree(Name);

            return -EBADF;
        }

        FileContext             = File->OpenContext;
        FileContext.Credentials = Task->Process->FileContext->Credentials;
        Expected.Generation     = File->Generation;
        Expected.Partition      = File->Partition;
        Expected.Number         = File->Number;

        if (Path == 0)
        {
            ExpectedPointer = &Expected;

            return SyscallVFSError(VFSSetTimes(&FileContext, File->Path, ExpectedPointer, TRUE, Times));
        }

        if (!File->Directory)
        {
            KFree(Name);

            return -ENOTDIR;
        }

        VFS_NODE Directory;

        Result = SyscallVFSError(VFSLookup(&FileContext, File->Path, &Directory));

        if (Result == 0 && (Directory.Number != Expected.Number || Directory.Partition != Expected.Partition || Directory.Generation != Expected.Generation))
        {
            Result = -ESTALE;
        }

        if (Result == 0)
        {
            Result = SyscallVFSError(VFSChangeDirectory(&FileContext, File->Path));
        }
    }

    if (Result == 0)
    {
        Result = SyscallVFSError(VFSSetTimes(&FileContext, Name, NULL, (Flags & 0x100) == 0, Times));
    }

    KFree(Name);

    return Result;
}
