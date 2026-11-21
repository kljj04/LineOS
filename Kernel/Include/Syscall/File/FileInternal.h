// Kernel/Include/Syscall/File/FileInternal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/File/SyscallFile.h>
#include <FS/DeviceFS/DeviceFS.h>
#include <Scheduler/WaitQueue.h>

#define FILE_READ  1U
#define FILE_WRITE 2U

struct OPEN_FILE
{
    UINT32             References;
    UINT32             Busy;
    INT64              Failed;
    SPIN_LOCK          Lock;
    WAIT_QUEUE         Waiters;
    UINT32             Access;
    BOOLEAN            Directory;
    BOOLEAN            Socket;
    BOOLEAN            Append;
    UINT32             Flags;
    UINT64             Offset;
    UINT64             Generation;
    UINT32             Number;
    UINT32             Partition;
    CHAR8             *Path;
    VFS_CONTEXT        OpenContext;
    CONST DEVICE_NODE *Device;
    VOID              *DeviceState;
};

INT64 FileBegin(OPEN_FILE *File);
VOID  FileEnd(OPEN_FILE *File);

OPEN_FILE *FileAcquire(TASK *Task, UINT64 FD);
VOID       FileDrop(OPEN_FILE **File);

INT64 FileResolvePath(TASK *Task, INT32 FD, UINT64 Path, BOOLEAN Empty, CHAR8 **Name);

#define FILE_AT_FDCWD            (-100)
#define FILE_AT_SYMLINK_NOFOLLOW 0x100U
#define FILE_AT_NO_AUTOMOUNT     0x800U
#define FILE_AT_EMPTY_PATH       0x1000U
#define FILE_OPEN_CLOEXEC        0x80000U

INT64 FileInstall(TASK *Task, OPEN_FILE *File, BOOLEAN CloseOnExec);
