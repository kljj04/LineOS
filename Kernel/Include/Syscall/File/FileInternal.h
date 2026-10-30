// Kernel/Include/Syscall/File/FileInternal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Syscall/File/SyscallFile.h>
#include <FS/DeviceFS/DeviceFS.h>
#include <Scheduler/WaitQueue.h>

#define FILE_READ 1U
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
    BOOLEAN            Append;
    UINT64             Offset;
    UINT64             NamespaceGeneration;
    UINT32             Number;
    UINT32             Partition;
    CHAR8             *Path;
    VFS_CONTEXT        OpenContext;
    CONST DEVICE_NODE *Device;
    VOID              *DeviceState;
};


INT64 FileBegin(OPEN_FILE *File);
VOID FileEnd(OPEN_FILE *File);
