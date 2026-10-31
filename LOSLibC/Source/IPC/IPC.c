// LOSLibC/Source/IPC/IPC.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <IPC.h>
#include <syscall.h>
#include <string.h>
#include <errno.h>

STATIC INT64 IPCCall(UINT32 Operation, LOS_IPC_REQUEST *Request)
{
    return LineOSSyscallResult(LineOSSyscall3(SYSCALL_IPC, Operation, (UINT64) Request, sizeof(*Request)));
}

STATIC INT64 IPCName(UINT32 Operation, CONST char *Name)
{
    LOS_IPC_REQUEST Request = {0};

    if (Name != NULL)
    {
        if (strlen(Name) >= sizeof(Request.Name))
        {
            errno = ENAMETOOLONG;

            return -1;
        }

        strcpy(Request.Name, Name);
    }

    return IPCCall(Operation, &Request);
}

INT64 LOSIPCCreate(CONST char *Name)
{
    return IPCName(LOS_IPC_CREATE, Name);
}

INT64 LOSIPCLookup(CONST char *Name)
{
    return IPCName(LOS_IPC_LOOKUP, Name);
}

INT64 LOSIPCBind(UINT64 Endpoint, UINT64 Peer)
{
    LOS_IPC_REQUEST Request = {.Endpoint = Endpoint, .Peer = Peer};

    return IPCCall(LOS_IPC_BIND, &Request);
}

INT64 LOSIPCSend(UINT64 Endpoint, UINT64 Peer, CONST VOID *Data, UINT32 Size)
{
    LOS_IPC_REQUEST Request = {.Endpoint = Endpoint, .Peer = Peer, .Data = (UINT64) Data, .Size = Size};

    return IPCCall(LOS_IPC_SEND, &Request);
}

INT64 LOSIPCReceive(UINT64 Endpoint, VOID *Data, UINT32 Capacity, LOS_IPC_REQUEST *Sender)
{
    LOS_IPC_REQUEST Request = {.Endpoint = Endpoint, .Data = (UINT64) Data, .Size = Capacity};
    INT64           Result = IPCCall(LOS_IPC_RECEIVE, &Request);

    if (Result >= 0 && Sender != NULL)
    {
        *Sender = Request;
    }

    return Result;
}

INT64 LOSIPCClose(UINT64 Endpoint)
{
    LOS_IPC_REQUEST Request = {.Endpoint = Endpoint};

    return IPCCall(LOS_IPC_CLOSE, &Request);
}

INT64 LOSIPCAlive(UINT64 Endpoint, UINT64 Peer)
{
    LOS_IPC_REQUEST Request = {.Endpoint = Endpoint, .Peer = Peer};

    return IPCCall(LOS_IPC_ALIVE, &Request);
}
