// Kernel/Source/Syscall/SyscallTerminal.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/SyscallTerminal.h>
#include <Scheduler/WaitQueue.h>
#include <Arch/AMD64/SpinLock.h>
#include <Debug/Debug.h>
#include <LineOS/Errno.h>
#include <Timer/Clock.h>
#include <Scheduler/Task.h>

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

typedef enum
{
    TERMINAL_DEBUG_TEXT,
    TERMINAL_DEBUG_ESCAPE,
    TERMINAL_DEBUG_CSI,
    TERMINAL_DEBUG_STRING,
    TERMINAL_DEBUG_STRING_ESCAPE,
    TERMINAL_DEBUG_INTERMEDIATE
} TERMINAL_DEBUG_STATE;

STATIC TERMINAL_DEBUG_STATE TerminalDebugState;
STATIC char TerminalDebugLine[4096];
STATIC UINTN TerminalDebugLength;
STATIC UINTN TerminalDebugCursor;
STATIC UINT32 TerminalDebugParameter;

STATIC VOID TerminalDebugText(UINT8 Byte)
{
    if (Byte == '\n')
    {
        TerminalDebugLine[TerminalDebugLength] = 0;
        DebugWrite(TerminalDebugLine);
        DebugWrite("\n");
        TerminalDebugLength = 0;
        TerminalDebugCursor = 0;
        return;
    }
    if (Byte == '\r')
    {
        TerminalDebugCursor = 0;
        return;
    }
    if (Byte == '\b' || Byte == 0x7F)
    {
        if (TerminalDebugCursor != 0)
        {
            do
            {
                TerminalDebugCursor--;
            } while (TerminalDebugCursor != 0 && ((UINT8) TerminalDebugLine[TerminalDebugCursor] & 0xC0) == 0x80);
        }
        return;
    }
    if (Byte < 0x20 && Byte != '\t')
    {
        return;
    }
    if ((Byte & 0xC0) != 0x80 && TerminalDebugCursor < TerminalDebugLength)
    {
        UINTN End = TerminalDebugCursor + 1;
        while (End < TerminalDebugLength && ((UINT8) TerminalDebugLine[End] & 0xC0) == 0x80)
        {
            End++;
        }
        UINTN Removed = End - TerminalDebugCursor;
        for (UINTN Index = End; Index < TerminalDebugLength; Index++)
        {
            TerminalDebugLine[Index - Removed] = TerminalDebugLine[Index];
        }
        TerminalDebugLength -= Removed;
    }
    if (TerminalDebugLength + 1 >= sizeof(TerminalDebugLine))
    {
        return;
    }
    for (UINTN Index = TerminalDebugLength; Index > TerminalDebugCursor; Index--)
    {
        TerminalDebugLine[Index] = TerminalDebugLine[Index - 1];
    }
    TerminalDebugLine[TerminalDebugCursor++] = (char) Byte;
    TerminalDebugLength++;
}


STATIC VOID TerminalDebugByte(UINT8 Byte)
{
    switch (TerminalDebugState)
    {
        case TERMINAL_DEBUG_TEXT:
        {
            if (Byte == 0x1B)
            {
                TerminalDebugState = TERMINAL_DEBUG_ESCAPE;
                return;
            }
            TerminalDebugText(Byte);
            return;
        }
        case TERMINAL_DEBUG_ESCAPE:
        {
            if (Byte == '[')
            {
                TerminalDebugParameter = 0;
                TerminalDebugState = TERMINAL_DEBUG_CSI;
            }
            else if (Byte == ']' || Byte == 'P' || Byte == 'X' || Byte == '^' || Byte == '_')
            {
                TerminalDebugState = TERMINAL_DEBUG_STRING;
            }
            else if (Byte >= 0x20 && Byte <= 0x2F)
            {
                TerminalDebugState = TERMINAL_DEBUG_INTERMEDIATE;
            }
            else if (Byte != 0x1B)
            {
                TerminalDebugState = TERMINAL_DEBUG_TEXT;
            }
            return;
        }
        case TERMINAL_DEBUG_CSI:
        {
            if (Byte >= '0' && Byte <= '9')
            {
                if (TerminalDebugParameter < 100000)
                {
                    TerminalDebugParameter = TerminalDebugParameter * 10 + Byte - '0';
                }
            }
            else if (Byte == ';')
            {
                TerminalDebugParameter = 0;
            }
            else if (Byte == 0x1B)
            {
                TerminalDebugState = TERMINAL_DEBUG_ESCAPE;
            }
            else if ((Byte >= 0x40 && Byte <= 0x7E) || Byte == 0x18 || Byte == 0x1A)
            {
                if (Byte == 'K' && TerminalDebugParameter == 2)
                {
                    for (UINTN Index = 0; Index < TerminalDebugCursor; Index++)
                    {
                        TerminalDebugLine[Index] = ' ';
                    }
                    TerminalDebugLength = TerminalDebugCursor;
                }
                TerminalDebugState = TERMINAL_DEBUG_TEXT;
            }
            return;
        }
        case TERMINAL_DEBUG_INTERMEDIATE:
        {
            if (Byte == 0x1B)
            {
                TerminalDebugState = TERMINAL_DEBUG_ESCAPE;
            }
            else if ((Byte >= 0x40 && Byte <= 0x7E) || Byte == 0x18 || Byte == 0x1A)
            {
                TerminalDebugState = TERMINAL_DEBUG_TEXT;
            }
            return;
        }
        case TERMINAL_DEBUG_STRING:
        {
            if (Byte == 0x07 || Byte == 0x18 || Byte == 0x1A)
            {
                TerminalDebugState = TERMINAL_DEBUG_TEXT;
            }
            else if (Byte == 0x1B)
            {
                TerminalDebugState = TERMINAL_DEBUG_STRING_ESCAPE;
            }
            return;
        }
        case TERMINAL_DEBUG_STRING_ESCAPE:
        {
            if (Byte == '\\' || Byte == 0x07 || Byte == 0x18 || Byte == 0x1A)
            {
                TerminalDebugState = TERMINAL_DEBUG_TEXT;
            }
            else if (Byte != 0x1B)
            {
                TerminalDebugState = TERMINAL_DEBUG_STRING;
            }
            return;
        }
    }
}

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
    TerminalDebugState = TERMINAL_DEBUG_TEXT;
    TerminalDebugLength = 0;
    TerminalDebugCursor = 0;
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
                    TerminalDebugByte(Byte);
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

    if (Code == 103 || Code == 108)
    {
        if (!Pressed)
        {
            return;
        }
        Byte = 27;
    }
    else if (Code == 100 || Code == 122)
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
    UINT32 Length = Code == 103 || Code == 108 ? 3U : 1U;
    if (!TerminalInput.Closed && Length <= TERMINAL_CAPACITY - TerminalInput.Length)
    {
        TerminalInput.Data[(TerminalInput.Head + TerminalInput.Length) % TERMINAL_CAPACITY] = Byte;
        TerminalInput.Length++;
        if (Length == 3)
        {
            TerminalInput.Data[(TerminalInput.Head + TerminalInput.Length++) % TERMINAL_CAPACITY] = '[';
            TerminalInput.Data[(TerminalInput.Head + TerminalInput.Length++) % TERMINAL_CAPACITY] = Code == 103 ? 'A' : 'B';
        }
        WaitQueueWakeAll(&TerminalInput.Readers);
    }
    SpinLockReleaseIRQRestore(&TerminalInput.Lock, Flags);
}

INT64 TerminalWaitOutput(UINT32 TimeoutMls)
{
    UINT64 Started = ClockGetUptimeMls();

    for (;;)
    {
        UINT64 Flags = SpinLockAcquireIRQSave(&TerminalOutput.Lock);
        BOOLEAN Ready = TerminalOutput.Length != 0;
        BOOLEAN Closed = TerminalOutput.Closed;
        SpinLockReleaseIRQRestore(&TerminalOutput.Lock, Flags);

        if (Ready)
        {
            return 1;
        }
        if (Closed)
        {
            return -EPIPE;
        }

        UINT64 Elapsed = ClockGetUptimeMls() - Started;
        if (Elapsed >= TimeoutMls)
        {
            return 0;
        }

        UINT64 Remaining = TimeoutMls - Elapsed;
        TaskSleepMls(Remaining < 20 ? Remaining : 20);
    }
}
