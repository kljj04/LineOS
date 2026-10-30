// Kernel/Source/Syscall/File/SyscallDirectory.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/File/SyscallDirectory.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

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

