// Kernel/Source/Syscall/Memory/SyscallDeviceMap.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Memory/SyscallMemory.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/SyscallError.h>
#include <Scheduler/LBPWRR.h>
#include <Memory/PMM.h>

#include <Syscall/Memory/Memory.h>

INT64 SyscallMapDevice(INT64 FD, UINT64 Length, UINT64 Protection, UINT64 Flags, UINT64 Offset, UINT64 *Physical)
{
    TASK      *Task = LBPWRRGetCurrentTask();
    OPEN_FILE *File;

    if (Task == NULL || FD < 0 || FD >= TASK_FILE_LIMIT || (File = Task->Files[FD]) == NULL)
    {
        return -EBADF;
    }

    if (File->Device == NULL || File->Device->Operations->Map == NULL)
    {
        return -ENODEV;
    }

    if ((File->Access & FILE_READ) == 0 || ((Protection & PROT_WRITE) != 0 && (File->Access & FILE_WRITE) == 0))
    {
        return -EACCES;
    }

    return File->Device->Operations->Map(Task, File->DeviceState, Length, Protection, Flags, Offset, Physical);
}
