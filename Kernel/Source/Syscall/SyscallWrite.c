// Kernel/Source/Syscall/SyscallWrite.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallWrite.h>
#include <Syscall/SyscallStream.h>
#include <Scheduler/LBPWRR.h>
#include <Arch/AMD64/SpinLock.h>
#include <Memory/VMM.h>
#include <Debug/Debug.h>

#define STREAM_CAPACITY 65536U
#define STREAM_CHUNK 256U

STATIC SPIN_LOCK StreamLock;
STATIC UINT8 StreamData[STREAM_CAPACITY];
STATIC UINT32 StreamHead;
STATIC UINT32 StreamLength;

INT64 SyscallWrite(UINT64 FD, CONST CHAR8 *Buffer, UINT64 Count)
{
    TASK *Task = LBPWRRGetCurrentTask();
    UINT8 Bytes[STREAM_CHUNK];
    UINT64 Total = 0;

    if ((FD != 1 && FD != 2) || Task == NULL || Task->AddressSpace == NULL || Count > 0x7FFFFFFFFFFFFFFFULL || (Count != 0 && (Buffer == NULL || (UINT64) Buffer > UINT64_MAX - Count)))
    {
        return -1;
    }
    while (Total < Count)
    {
        UINT32 Chunk = Count - Total > STREAM_CHUNK ? STREAM_CHUNK : (UINT32) (Count - Total);
        UINT64 Flags;
        UINT32 Available;

        if (!VMMCopyFromUser(Task->AddressSpace, Bytes, (UINT64) Buffer + Total, Chunk))
        {
            return Total != 0 ? (INT64) Total : -1;
        }
        for (UINT32 Index = 0; Index < Chunk; Index++)
        {
            char Message[2] = { (char) Bytes[Index], 0 };
            DebugWrite(Message);
        }
        Flags = SpinLockAcquireIRQSave(&StreamLock);
        Available = STREAM_CAPACITY - StreamLength;
        if (Chunk > Available)
        {
            Chunk = Available;
        }
        for (UINT32 Index = 0; Index < Chunk; Index++)
        {
            StreamData[(StreamHead + StreamLength + Index) % STREAM_CAPACITY] = Bytes[Index];
        }
        StreamLength += Chunk;
        SpinLockReleaseIRQRestore(&StreamLock, Flags);
        Total += Chunk;
        if (Chunk == 0 || Chunk < STREAM_CHUNK)
        {
            break;
        }
    }
    return Total != 0 || Count == 0 ? (INT64) Total : -1;
}

INT64 SyscallStreamRead(UINT64 Buffer, UINT64 Count)
{
    TASK *Task = LBPWRRGetCurrentTask();
    UINT8 Bytes[STREAM_CHUNK];
    UINT64 Flags;
    UINT32 Chunk;

    if (Task == NULL || Task->AddressSpace == NULL || Count > 0x7FFFFFFFFFFFFFFFULL || (Count != 0 && (Buffer == 0 || Buffer > UINT64_MAX - Count)))
    {
        return -1;
    }
    if (Count == 0)
    {
        return 0;
    }
    Flags = SpinLockAcquireIRQSave(&StreamLock);
    Chunk = Count > STREAM_CHUNK ? STREAM_CHUNK : (UINT32) Count;
    if (Chunk > StreamLength)
    {
        Chunk = StreamLength;
    }
    if (Chunk == 0)
    {
        SpinLockReleaseIRQRestore(&StreamLock, Flags);
        return -1;
    }
    for (UINT32 Index = 0; Index < Chunk; Index++)
    {
        Bytes[Index] = StreamData[(StreamHead + Index) % STREAM_CAPACITY];
    }
    if (!VMMCopyToUser(Task->AddressSpace, Buffer, Bytes, Chunk))
    {
        SpinLockReleaseIRQRestore(&StreamLock, Flags);
        return -1;
    }
    StreamHead = (StreamHead + Chunk) % STREAM_CAPACITY;
    StreamLength -= Chunk;
    SpinLockReleaseIRQRestore(&StreamLock, Flags);
    return Chunk;
}
