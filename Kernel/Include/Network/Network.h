// Kernel/Include/Network/Network.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

#define NETWORK_IPV4(A, B, C, D) (((UINT32) (A) << 24) | ((UINT32) (B) << 16) | ((UINT32) (C) << 8) | (UINT32) (D))
#define NETWORK_MTU              1500
#define NETWORK_UDP_MAX_PAYLOAD  1472

typedef struct
{
    UINT32 Address;
    UINT32 Netmask;
    UINT32 Gateway;
    UINT32 DNS;
} NETWORK_CONFIG;

typedef struct
{
    UINT64 Received;
    UINT64 Transmitted;
    UINT64 Dropped;
    UINT64 ARPReceived;
    UINT64 ICMPReceived;
    UINT64 UDPReceived;
    UINT64 TCPReceived;
    UINT64 Retransmitted;
} NETWORK_STATS;

BOOLEAN       NetworkInit(VOID);
BOOLEAN       NetworkConfigure(CONST NETWORK_CONFIG *Config);
BOOLEAN       NetworkGetConfig(NETWORK_CONFIG *Config);
VOID          NetworkGetStats(NETWORK_STATS *Stats);
VOID          NetworkPoll(VOID);
VOID          NetworkTask(VOID);
BOOLEAN       NetworkPing(UINT32 Address, UINT32 TimeoutMls, UINT32 *RoundTripMls);
BOOLEAN       NetworkUDPExchange(UINT32 Address, UINT16 Port, CONST VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls);
BOOLEAN       NetworkDNSResolve(CONST CHAR8 *Name, UINT32 *Address, UINT32 TimeoutMls);
BOOLEAN       NetworkTCPConnect(UINT32 Address, UINT16 Port, UINT32 TimeoutMls);
BOOLEAN       NetworkTCPSend(CONST VOID *Buffer, UINT32 Length, UINT32 TimeoutMls);
BOOLEAN       NetworkTCPReceive(VOID *Buffer, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls);
BOOLEAN       NetworkTCPClose(UINT32 TimeoutMls);
VOID          NetworkTCPAbort(VOID);
BOOLEAN       NetworkHTTPGet(CONST CHAR8 *Host, UINT16 Port, CONST CHAR8 *Path, VOID *Body, UINT32 Capacity, UINT32 *Length, UINT16 *Status, UINT32 TimeoutMls);
CONST CHAR16 *NetworkGetLastError(VOID);
