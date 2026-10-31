// LOSLibC/Include/IPC.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <Syscall/IPC/IPC.h>

INT64 LOSIPCCreate(CONST char *Name);
INT64 LOSIPCLookup(CONST char *Name);
INT64 LOSIPCBind(UINT64 Endpoint, UINT64 Peer);
INT64 LOSIPCSend(UINT64 Endpoint, UINT64 Peer, CONST VOID *Data, UINT32 Size);
INT64 LOSIPCReceive(UINT64 Endpoint, VOID *Data, UINT32 Capacity, LOS_IPC_REQUEST *Sender);
INT64 LOSIPCClose(UINT64 Endpoint);
INT64 LOSIPCAlive(UINT64 Endpoint, UINT64 Peer);
