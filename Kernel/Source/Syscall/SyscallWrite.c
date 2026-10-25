// Kernel/Source/Syscall/SyscallWrite.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/Console/Console.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Syscall/Syscall.h>
#include <Memory/VMM.h>

INT64 SyscallWrite(UINT64 FD, CONST CHAR8 *Buffer, UINT64 Count)
{
    TASK  *Task;
    CHAR8  Bytes[255];
    UINT64 Total;
    UINT64 Chunk;
    UINT64 Written;

    if (FD != 1 && FD != 2)
    {
        return -1;
    }

    if (Count == 0)
    {
        return 0;
    }

    if (Buffer == NULL || Count > 0x7FFFFFFFFFFFFFFFULL)
    {
        return -1;
    }

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -1;
    }

    Total = 0;

    while (Total < Count)
    {
        Chunk = Count - Total;

        if (Chunk > sizeof(Bytes))
        {
            Chunk = sizeof(Bytes);
        }

        if ((UINT64) Buffer > UINT64_MAX - Total)
        {
            break;
        }

        if (!VMMCopyFromUser(
            Task->AddressSpace,
            Bytes,
            (UINT64) Buffer + Total,
            Chunk
        ))
        {
            break;
        }

        Written = ConsoleWrite(Bytes, Chunk);

        if (Written > Chunk)
        {
            break;
        }

        Total += Written;

        if (Written < Chunk)
        {
            break;
        }
    }

    return Total > 0 ? (INT64) Total : -1;
}