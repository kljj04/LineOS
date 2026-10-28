// Kernel/Source/Syscall/SyscallTerminal.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallTerminal.h>
#include <Scheduler/WaitQueue.h>
#include <Arch/AMD64/SpinLock.h>
#include <Debug/Debug.h>
#include <LineOS/Errno.h>

#define TERMINAL_CAPACITY 65536U
#define TERMINAL_CHUNK    256U

typedef struct
{
    SPIN_LOCK  Lock;
    WAIT_QUEUE Readers;
    WAIT_QUEUE Writers;
    UINT8      Data[TERMINAL_CAPACITY];
    UINT32     Head;
    UINT32     Length;
    BOOLEAN    Closed;
} TERMINAL_QUEUE;

STATIC TERMINAL_QUEUE TerminalInput;
STATIC TERMINAL_QUEUE TerminalOutput;
STATIC SPIN_LOCK      TerminalControlLock;
STATIC BOOLEAN        TerminalMasterActive;

BOOLEAN TerminalAttach(VOID)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&TerminalControlLock);
    if (TerminalMasterActive)
    {
        SpinLockReleaseIRQRestore(&TerminalControlLock, Flags);
        return FALSE;
    }
    TerminalMasterActive = TRUE;
    SpinLockAcquire(&TerminalInput.Lock);
    TerminalInput.Closed = FALSE;
    SpinLockRelease(&TerminalInput.Lock);
    SpinLockAcquire(&TerminalOutput.Lock);
    TerminalOutput.Closed = FALSE;
    SpinLockRelease(&TerminalOutput.Lock);
    SpinLockReleaseIRQRestore(&TerminalControlLock, Flags);
    return TRUE;
}

VOID TerminalDetach(VOID)
{
    UINT64          Flags = SpinLockAcquireIRQSave(&TerminalControlLock);
    TERMINAL_QUEUE *Queues[2] = {&TerminalInput, &TerminalOutput};
    for (UINTN Index = 0; Index < 2; Index++)
    {
        TERMINAL_QUEUE *Queue = Queues[Index];
        SpinLockAcquire(&Queue->Lock);
        Queue->Closed = TRUE;
        WaitQueueWakeAll(&Queue->Readers);
        WaitQueueWakeAll(&Queue->Writers);
        SpinLockRelease(&Queue->Lock);
    }
    TerminalMasterActive = FALSE;
    SpinLockReleaseIRQRestore(&TerminalControlLock, Flags);
}

INT64 TerminalRead(TASK *Task, BOOLEAN Master, UINT64 Buffer, UINT64 Count)
{
    TERMINAL_QUEUE *Queue = Master ? &TerminalOutput : &TerminalInput;
    UINT8           Bytes[TERMINAL_CHUNK];
    UINT64          Flags;
    UINT32          Length;

    if (Count == 0)
    {
        return 0;
    }
    Flags = SpinLockAcquireIRQSave(&Queue->Lock);
    while (Queue->Length == 0 && !Queue->Closed)
    {
        if (!WaitQueueSleep(&Queue->Readers, &Queue->Lock))
        {
            SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
            return -EINTR;
        }
    }
    Length = Count > TERMINAL_CHUNK ? TERMINAL_CHUNK : (UINT32) Count;
    if (Length > Queue->Length)
    {
        Length = Queue->Length;
    }
    if (Length == 0)
    {
        SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
        return 0;
    }
    for (UINT32 Index = 0; Index < Length; Index++)
    {
        Bytes[Index] = Queue->Data[(Queue->Head + Index) % TERMINAL_CAPACITY];
    }
    if (!VMMCopyToUser(Task->AddressSpace, Buffer, Bytes, Length))
    {
        SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
        return -EFAULT;
    }
    Queue->Head = (Queue->Head + Length) % TERMINAL_CAPACITY;
    Queue->Length -= Length;
    WaitQueueWakeAll(&Queue->Writers);
    SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
    return Length;
}

INT64 TerminalWrite(TASK *Task, BOOLEAN Master, UINT64 Buffer, UINT64 Count)
{
    TERMINAL_QUEUE *Queue = Master ? &TerminalInput : &TerminalOutput;
    UINT8           Bytes[TERMINAL_CHUNK];
    UINT64          Total = 0;

    while (Total < Count)
    {
        UINT32 Length = Count - Total > TERMINAL_CHUNK ? TERMINAL_CHUNK : (UINT32) (Count - Total);
        UINT32 Done = 0;
        if (!VMMCopyFromUser(Task->AddressSpace, Bytes, Buffer + Total, Length))
        {
            return Total != 0 ? (INT64) Total : -EFAULT;
        }
        while (Done < Length)
        {
            UINT64 Flags = SpinLockAcquireIRQSave(&Queue->Lock);
            while (Queue->Length == TERMINAL_CAPACITY && !Queue->Closed)
            {
                if (!WaitQueueSleep(&Queue->Writers, &Queue->Lock))
                {
                    SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
                    return Total != 0 ? (INT64) Total : -EINTR;
                }
            }
            if (Queue->Closed)
            {
                SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
                return Total != 0 ? (INT64) Total : -EPIPE;
            }
            UINT32 Chunk = Length - Done;
            if (Chunk > TERMINAL_CAPACITY - Queue->Length)
            {
                Chunk = TERMINAL_CAPACITY - Queue->Length;
            }
            for (UINT32 Index = 0; Index < Chunk; Index++)
            {
                UINT8 Byte = Bytes[Done + Index];
                Queue->Data[(Queue->Head + Queue->Length + Index) % TERMINAL_CAPACITY] = Byte;
                if (!Master)
                {
                    char Message[2] = {(char) Byte, 0};
                    DebugWrite(Message);
                }
            }
            Queue->Length += Chunk;
            Done += Chunk;
            Total += Chunk;
            WaitQueueWakeAll(&Queue->Readers);
            SpinLockReleaseIRQRestore(&Queue->Lock, Flags);
        }
    }
    return (INT64) Total;
}

VOID TerminalKey(UINT16 Code, BOOLEAN Pressed, UINT8 Modifiers)
{
    STATIC CONST CHAR8 Plain[] = "\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
    STATIC CONST CHAR8 Shift[] = "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
    STATIC BOOLEAN     TogglePressed[2];
    UINT8              Byte;
    UINT64             Flags;

    if (Code == 100 || Code == 122)
    {
        UINTN   Index = Code == 100 ? 0 : 1;
        BOOLEAN Previous = TogglePressed[Index];
        TogglePressed[Index] = Pressed;
        if (!Pressed || Previous)
        {
            return;
        }
        Byte = 14;
    }
    else
    {
        if (!Pressed || Code >= sizeof(Plain) - 1)
        {
            return;
        }
        Byte = (Modifiers & 1) != 0 ? Shift[Code] : Plain[Code];
        if ((Modifiers & 2) != 0 && ((Byte >= 'a' && Byte <= 'z') || (Byte >= 'A' && Byte <= 'Z')))
        {
            Byte ^= 32;
        }
        if ((Modifiers & 4) != 0 && ((Byte >= 'a' && Byte <= 'z') || (Byte >= 'A' && Byte <= 'Z')))
        {
            Byte &= 31;
        }
    }
    if (Byte == 0)
    {
        return;
    }
    Flags = SpinLockAcquireIRQSave(&TerminalInput.Lock);
    if (!TerminalInput.Closed && TerminalInput.Length < TERMINAL_CAPACITY)
    {
        TerminalInput.Data[(TerminalInput.Head + TerminalInput.Length) % TERMINAL_CAPACITY] = Byte;
        TerminalInput.Length++;
        WaitQueueWakeAll(&TerminalInput.Readers);
    }
    SpinLockReleaseIRQRestore(&TerminalInput.Lock, Flags);
}
