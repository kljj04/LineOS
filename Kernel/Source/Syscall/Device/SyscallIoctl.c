// Kernel/Source/Syscall/Device/SyscallIoctl.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Device/SyscallIoctl.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

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

