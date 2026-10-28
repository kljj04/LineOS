// Kernel/Include/FS/DeviceFS/DeviceFS.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once
#include <FS/VFS/VFS.h>
#include <Scheduler/TaskTypes.h>

typedef struct
{
    INT64 (*Open)(TASK *Task, UINT64 Flags, VOID **State);
    INT64 (*Read)(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count);
    INT64 (*Write)(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count);
    INT64 (*Close)(VOID *State);
    INT64 (*Map)(TASK *Task, VOID *State, UINT64 Length, UINT64 Protection, UINT64 Flags, UINT64 Offset, UINT64 *Physical);
    INT64 (*Ioctl)(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access);
} DEVICE_OPERATIONS;

typedef struct
{
    CONST CHAR8             *Name;
    UINT16                   Mode;
    BOOLEAN                  Seekable;
    CONST DEVICE_OPERATIONS *Operations;
} DEVICE_NODE;

VFS_ERROR          DeviceFSMount(VFS_REGISTRY *Registry);
CONST DEVICE_NODE *DeviceFSGetNode(UINT32 Number);
