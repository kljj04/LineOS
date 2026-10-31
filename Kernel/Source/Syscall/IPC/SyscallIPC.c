// Kernel/Source/Syscall/IPC/SyscallIPC.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/IPC/SyscallIPC.h>
#include <Syscall/IPC/IPC.h>
#include <Scheduler/LBPWRR.h>
#include <Arch/AMD64/SpinLock.h>
#include <Memory/PMM.h>
#include <Syscall/Errno.h>

#define IPC_ENDPOINT_LIMIT 64U
#define IPC_QUEUE_LIMIT    16U
#define IPC_MEMORY_LIMIT   (16U * 1024U * 1024U)

typedef struct IPC_MESSAGE
{
    struct IPC_MESSAGE *Next;
    UINT64              Source;
    UINT32              Size;
    UINT32              UID;
    UINT32              PID;
    UINT8               Data[];
} IPC_MESSAGE;

typedef struct
{
    TASK        *Owner;
    UINT64       ID;
    UINT64       Peer;
    UINT32       PID;
    UINT32       UID;
    UINT32       Count;
    CHAR8        Name[LOS_IPC_NAME_SIZE];
    IPC_MESSAGE *Head;
    IPC_MESSAGE *Tail;
} IPC_ENDPOINT;

STATIC IPC_ENDPOINT Endpoints[IPC_ENDPOINT_LIMIT];
STATIC SPIN_LOCK    IPCLock;
STATIC UINT64       NextID = 1;
STATIC UINT64       QueuedBytes;

STATIC BOOLEAN IPCNameEqual(CONST CHAR8 *Left, CONST char *Right)
{
    for (UINT32 Index = 0; Index < LOS_IPC_NAME_SIZE; Index++)
    {
        if (Left[Index] != (UINT8) Right[Index])
        {
            return FALSE;
        }

        if (Left[Index] == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC VOID IPCClear(IPC_ENDPOINT *Endpoint)
{
    IPC_MESSAGE *Message = Endpoint->Head;

    while (Message != NULL)
    {
        IPC_MESSAGE *Next = Message->Next;

        QueuedBytes -= Message->Size;
        KFree(Message);
        Message = Next;
    }

    KMemSet(Endpoint, 0, sizeof(*Endpoint));
}

STATIC VOID IPCCollect(VOID)
{
    for (UINT32 Index = 0; Index < IPC_ENDPOINT_LIMIT; Index++)
    {
        IPC_ENDPOINT *Endpoint = &Endpoints[Index];

        if (Endpoint->Owner != NULL && (Endpoint->Owner->PID != Endpoint->PID || Endpoint->Owner->State == TASK_TERMINATED || Endpoint->Owner->State == TASK_KILLED))
        {
            IPCClear(Endpoint);
        }
    }
}

STATIC IPC_ENDPOINT *IPCFind(UINT64 ID)
{
    for (UINT32 Index = 0; Index < IPC_ENDPOINT_LIMIT; Index++)
    {
        if (Endpoints[Index].Owner != NULL && Endpoints[Index].ID == ID)
        {
            return &Endpoints[Index];
        }
    }

    return NULL;
}

STATIC BOOLEAN IPCAllowed(IPC_ENDPOINT *Source, IPC_ENDPOINT *Target)
{
    return Source->UID == Target->UID || Source->UID == 0 || Target->Peer == Source->ID;
}

BOOLEAN SyscallIPCOwns(TASK *Task, UINT64 ID)
{
    UINT64        Flags = SpinLockAcquireIRQSave(&IPCLock);
    IPC_ENDPOINT *Endpoint = IPCFind(ID);
    BOOLEAN       Owned = Endpoint != NULL && Endpoint->Owner == Task && Endpoint->PID == Task->PID;

    SpinLockReleaseIRQRestore(&IPCLock, Flags);

    return Owned;
}

VOID SyscallIPCRelease(TASK *Task)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&IPCLock);

    for (UINT32 Index = 0; Index < IPC_ENDPOINT_LIMIT; Index++)
    {
        if (Endpoints[Index].Owner == Task)
        {
            IPCClear(&Endpoints[Index]);
        }
    }

    SpinLockReleaseIRQRestore(&IPCLock, Flags);
}

INT64 SyscallIPC(UINT64 Operation, UINT64 UserRequest, UINT64 Size)
{
    TASK           *Task = LBPWRRGetCurrentTask();
    LOS_IPC_REQUEST Request;

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    if (Size != sizeof(Request))
    {
        return -EINVAL;
    }

    if (!VMMCopyFromUser(Task->AddressSpace, &Request, UserRequest, sizeof(Request)))
    {
        return -EFAULT;
    }

    if (Request.Name[LOS_IPC_NAME_SIZE - 1] != 0 || Request.Reserved != 0)
    {
        return -EINVAL;
    }

    IPC_MESSAGE *Outgoing = NULL;

    if (Operation == LOS_IPC_SEND)
    {
        if (Request.Size == 0 || Request.Size > LOS_IPC_MESSAGE_SIZE)
        {
            return -EMSGSIZE;
        }

        Outgoing = KAlloc(sizeof(*Outgoing) + Request.Size);

        if (Outgoing == NULL)
        {
            return -ENOMEM;
        }

        if (!VMMCopyFromUser(Task->AddressSpace, Outgoing->Data, Request.Data, Request.Size))
        {
            KFree(Outgoing);

            return -EFAULT;
        }

        Outgoing->Next = NULL;
        Outgoing->Size = Request.Size;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&IPCLock);
    INT64  Result = -EINVAL;

    IPCCollect();

    if (Operation == LOS_IPC_CREATE || Operation == LOS_IPC_LOOKUP)
    {
        IPC_ENDPOINT *Free = NULL;

        Result = -ENOENT;

        for (UINT32 Index = 0; Index < IPC_ENDPOINT_LIMIT; Index++)
        {
            IPC_ENDPOINT *Endpoint = &Endpoints[Index];

            if (Endpoint->Owner == NULL)
            {
                if (Free == NULL)
                {
                    Free = Endpoint;
                }
            }
            else if (Request.Name[0] != 0 && Endpoint->UID == Task->RealUID && IPCNameEqual(Endpoint->Name, Request.Name))
            {
                Result = Operation == LOS_IPC_LOOKUP ? (INT64) Endpoint->ID : -EEXIST;
                goto Done;
            }
        }

        if (Operation == LOS_IPC_CREATE)
        {
            if (Free == NULL || NextID >= 0x7FFFFFFFFFFFFFFFULL)
            {
                Result = -ENOSPC;
                goto Done;
            }

            Free->Owner = Task;
            Free->PID = Task->PID;
            Free->UID = Task->RealUID;
            Free->ID = NextID++;
            KMemCpy(Free->Name, Request.Name, sizeof(Free->Name));
            Result = (INT64) Free->ID;
        }

        goto Done;
    }

    IPC_ENDPOINT *Endpoint = IPCFind(Request.Endpoint);

    if (Endpoint == NULL)
    {
        Result = -ENOENT;
        goto Done;
    }

    if (Endpoint->Owner != Task || Endpoint->PID != Task->PID)
    {
        Result = -EPERM;
        goto Done;
    }

    if (Operation == LOS_IPC_CLOSE)
    {
        IPCClear(Endpoint);
        Result = 0;
    }
    else if (Operation == LOS_IPC_RECEIVE)
    {
        IPC_MESSAGE *Message = Endpoint->Head;

        if (Message == NULL)
        {
            Result = -EAGAIN;
            goto Done;
        }

        if (Request.Size < Message->Size)
        {
            Result = -EMSGSIZE;
            goto Done;
        }

        Request.Size = Message->Size;
        Request.Peer = Message->Source;
        Request.UID = Message->UID;
        Request.PID = Message->PID;

        if (!VMMCopyToUser(Task->AddressSpace, Request.Data, Message->Data, Message->Size) || !VMMCopyToUser(Task->AddressSpace, UserRequest, &Request, sizeof(Request)))
        {
            Result = -EFAULT;
            goto Done;
        }

        Endpoint->Head = Message->Next;

        if (Endpoint->Head == NULL)
        {
            Endpoint->Tail = NULL;
        }

        Endpoint->Count--;
        QueuedBytes -= Message->Size;
        Result = Message->Size;
        KFree(Message);
    }
    else
    {
        IPC_ENDPOINT *Peer = IPCFind(Request.Peer);

        if (Peer == NULL)
        {
            Result = -ENOENT;
            goto Done;
        }

        if (Operation == LOS_IPC_BIND)
        {
            if (Endpoint->UID != 0 && Endpoint->UID != Peer->UID)
            {
                Result = -EACCES;
                goto Done;
            }

            Endpoint->Peer = Peer->ID;
            Result = 0;
        }
        else if (!IPCAllowed(Endpoint, Peer))
        {
            Result = -EACCES;
        }
        else if (Operation == LOS_IPC_ALIVE)
        {
            Result = Peer->PID;
        }
        else if (Operation == LOS_IPC_SEND)
        {
            if (Peer->Count == IPC_QUEUE_LIMIT || Request.Size > IPC_MEMORY_LIMIT - QueuedBytes)
            {
                Result = -EAGAIN;
                goto Done;
            }

            Outgoing->Source = Endpoint->ID;
            Outgoing->UID = Endpoint->UID;
            Outgoing->PID = Endpoint->PID;

            if (Peer->Tail != NULL)
            {
                Peer->Tail->Next = Outgoing;
            }
            else
            {
                Peer->Head = Outgoing;
            }

            Peer->Tail = Outgoing;
            Peer->Count++;
            QueuedBytes += Request.Size;
            Outgoing = NULL;
            Result = 0;
        }
    }

Done:
    SpinLockReleaseIRQRestore(&IPCLock, Flags);

    if (Outgoing != NULL)
    {
        KFree(Outgoing);
    }

    return Result;
}
