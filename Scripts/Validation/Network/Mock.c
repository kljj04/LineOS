// Scripts/Validation/Network/Mock.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Scheduler/WaitQueue.h>
#include <string.h>

NETWORK_STATE NetworkState;
UINT64        NetworkTestTime = 1;
UINT8         NetworkTestPacket[1500];
UINT32        NetworkTestLength;
UINT32        NetworkTestPackets;

UINT16 NetworkRead16(CONST UINT8 *Data)
{
    return (UINT16) ((Data[0] << 8) | Data[1]);
}
UINT32 NetworkRead32(CONST UINT8 *Data)
{
    return ((UINT32) NetworkRead16(Data) << 16) | NetworkRead16(Data + 2);
}
VOID NetworkWrite16(UINT8 *Data, UINT16 Value)
{
    Data[0] = Value >> 8;
    Data[1] = (UINT8) Value;
}
VOID NetworkWrite32(UINT8 *Data, UINT32 Value)
{
    NetworkWrite16(Data, Value >> 16);
    NetworkWrite16(Data + 2, (UINT16) Value);
}
UINT16 NetworkChecksum(CONST UINT8 *Data, UINT32 Length, UINT32 Sum)
{
    for (UINT32 Index = 0; Index + 1 < Length; Index += 2)
        Sum += NetworkRead16(Data + Index);
    if ((Length & 1) != 0)
        Sum += (UINT32) Data[Length - 1] << 8;
    while ((Sum >> 16) != 0)
        Sum = (Sum & 0xFFFF) + (Sum >> 16);
    return (UINT16) ~Sum;
}
UINT16 NetworkTransportChecksum(UINT32 Source, UINT32 Destination, UINT8 Protocol, CONST UINT8 *Data, UINT32 Length)
{
    return NetworkChecksum(Data, Length, (Source >> 16) + (Source & 0xFFFF) + (Destination >> 16) + (Destination & 0xFFFF) + Protocol + Length);
}
UINT64 NetworkNow(VOID)
{
    return NetworkTestTime;
}
UINT32 NetworkRemaining(UINT64 Start, UINT32 Timeout)
{
    return NetworkTestTime - Start >= Timeout ? 0 : Timeout - (UINT32) (NetworkTestTime - Start);
}
BOOLEAN NetworkExpired(UINT64 Start, UINT32 Timeout)
{
    return NetworkTestTime - Start >= Timeout;
}
VOID NetworkPoll(VOID)
{
    NetworkTCPTickLocked();
}
VOID NetworkWait(VOID)
{
    NetworkTestTime++;
    NetworkPoll();
}
BOOLEAN NetworkError(CONST CHAR16 *Error)
{
    (VOID) Error;
    return FALSE;
}
BOOLEAN NetworkResolveReadyLocked(UINT32 Address)
{
    (VOID) Address;
    return TRUE;
}
BOOLEAN NetworkResolve(UINT32 Address, UINT32 Timeout)
{
    (VOID) Address;
    (VOID) Timeout;
    return TRUE;
}
BOOLEAN NetworkSendIPv4Locked(UINT32 Address, UINT8 Protocol, CONST UINT8 *Data, UINT32 Length)
{
    (VOID) Address;
    (VOID) Protocol;
    if (Length > sizeof(NetworkTestPacket))
        return FALSE;
    memcpy(NetworkTestPacket, Data, Length);
    NetworkTestLength = Length;
    NetworkTestPackets++;
    return TRUE;
}
UINT16 NetworkAllocatePortLocked(VOID)
{
    for (UINT32 Port = 49152; Port < 65536; Port++)
        if (!NetworkTCPPortInUseLocked((UINT16) Port))
            return (UINT16) Port;
    return 0;
}
VOID WaitQueueInit(WAIT_QUEUE *Queue)
{
    memset(Queue, 0, sizeof(*Queue));
}
UINT64 RDTSC(VOID)
{
    STATIC UINT64 Counter = 12345;
    return ++Counter;
}
