// Kernel/Include/Network/NetworkInternal.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <Network/Network.h>
#include <Network/VirtIONetwork.h>
#include <Arch/AMD64/SpinLock.h>

#define NETWORK_TCP_BUFFER_SIZE 8192
#define NETWORK_ARP_COUNT       16

typedef struct
{
    UINT32 Address;
    UINT8  MAC[6];
    UINT64 Updated;
    UINT64 Requested;
} NETWORK_ARP_ENTRY;

typedef struct
{
    SPIN_LOCK              Lock;
    VIRTIO_NETWORK_DEVICE *Device;
    NETWORK_CONFIG         Config;
    NETWORK_STATS          Stats;
    NETWORK_ARP_ENTRY      ARP[NETWORK_ARP_COUNT];
    UINT32                 Busy;
    UINT16                 Identification;
    UINT16                 NextPort;
    BOOLEAN                Ready;
    BOOLEAN                Configured;
    BOOLEAN                PingPending;
    BOOLEAN                PingReply;
    UINT32                 PingAddress;
    UINT16                 PingID;
    UINT8                  PingData[16];
} NETWORK_STATE;

EXTERN NETWORK_STATE NetworkState;

UINT16  NetworkRead16(CONST UINT8 *Data);
UINT32  NetworkRead32(CONST UINT8 *Data);
VOID    NetworkWrite16(UINT8 *Data, UINT16 Value);
VOID    NetworkWrite32(UINT8 *Data, UINT32 Value);
UINT16  NetworkChecksum(CONST UINT8 *Data, UINT32 Length, UINT32 Sum);
UINT16  NetworkTransportChecksum(UINT32 Source, UINT32 Destination, UINT8 Protocol, CONST UINT8 *Data, UINT32 Length);
UINT64  NetworkNow(VOID);
UINT32  NetworkRemaining(UINT64 Start, UINT32 TimeoutMls);
BOOLEAN NetworkExpired(UINT64 Start, UINT32 TimeoutMls);
VOID    NetworkWait(VOID);
BOOLEAN NetworkBegin(VOID);
VOID    NetworkEnd(VOID);
BOOLEAN NetworkError(CONST CHAR16 *Error);
BOOLEAN NetworkResolve(UINT32 Address, UINT32 TimeoutMls);
BOOLEAN NetworkSendIPv4Locked(UINT32 Address, UINT8 Protocol, CONST UINT8 *Payload, UINT32 Length);
BOOLEAN NetworkUDPExchangeInternal(UINT32 Address, UINT16 Port, CONST VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls);
BOOLEAN NetworkDNSResolveInternal(CONST CHAR8 *Name, UINT32 *Address, UINT32 TimeoutMls);
VOID    NetworkTCPInputLocked(UINT32 Address, CONST UINT8 *Packet, UINT32 Length);
BOOLEAN NetworkTCPActiveLocked(VOID);

UINT16  NetworkAllocatePortLocked(VOID);
BOOLEAN NetworkUDPPortInUseLocked(UINT16 Port);
VOID    NetworkUDPInputLocked(UINT32 Address, UINT32 Destination, CONST UINT8 *Packet, UINT32 Length);
BOOLEAN NetworkUDPSendLocked(UINT16 LocalPort, UINT32 Address, UINT16 Port, CONST VOID *Buffer, UINT32 Length);
BOOLEAN NetworkTCPPortInUseLocked(UINT16 Port);
VOID    NetworkDHCPTick(VOID);
VOID    NetworkDHCPStopLocked(VOID);
BOOLEAN NetworkSetConfigLocked(CONST NETWORK_CONFIG *Config);
VOID    NetworkClearConfigLocked(VOID);

VOID NetworkDHCPInputLocked(UINT32 Source, CONST UINT8 *Packet, UINT32 Length);
VOID NetworkTCPResetLocked(VOID);
