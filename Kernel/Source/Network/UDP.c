// Kernel/Source/Network/UDP.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>

typedef struct
{
    UINT32 Address;
    UINT16 Port;
    UINT16 Length;
    UINT8  Data[NETWORK_UDP_MAX_PAYLOAD];
} NETWORK_UDP_DATAGRAM;

typedef struct
{
    UINT32               Generation;
    UINT16               Port;
    UINT16               Head;
    UINT16               Count;
    BOOLEAN              Open;
    BOOLEAN              Reading;
    NETWORK_UDP_DATAGRAM Queue[NETWORK_UDP_QUEUE_SIZE];
} NETWORK_UDP_SOCKET;

STATIC NETWORK_UDP_SOCKET Sockets[NETWORK_UDP_SOCKET_COUNT];

STATIC NETWORK_UDP_SOCKET *FindSocket(UINT32 Socket)
{
    UINT32 Index = Socket & 255;

    if (Index >= NETWORK_UDP_SOCKET_COUNT || !Sockets[Index].Open || Sockets[Index].Generation != (Socket >> 8))
    {
        return NULL;
    }

    return &Sockets[Index];
}

BOOLEAN NetworkUDPPortInUseLocked(UINT16 Port)
{
    if (Port == 68)
    {
        return TRUE;
    }
    for (UINT32 Index = 0; Index < NETWORK_UDP_SOCKET_COUNT; Index++)
    {
        if (Sockets[Index].Open && Sockets[Index].Port == Port)
        {
            return TRUE;
        }
    }

    return FALSE;
}

UINT16 NetworkAllocatePortLocked(VOID)
{
    for (UINT32 Attempt = 0; Attempt < 16384; Attempt++)
    {
        NetworkState.NextPort = NetworkState.NextPort == 65535 ? 49152 : NetworkState.NextPort + 1;

        if (!NetworkUDPPortInUseLocked(NetworkState.NextPort) && !NetworkTCPPortInUseLocked(NetworkState.NextPort))
        {
            return NetworkState.NextPort;
        }
    }

    return 0;
}

BOOLEAN NetworkUDPOpen(UINT16 Port, UINT32 *Socket)
{
    UINT64 Flags;

    if (Socket == NULL || !NetworkState.Ready)
    {
        return NetworkError(L"UDP unavailable or invalid handle output");
    }

    *Socket = UINT32_MAX;
    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

    if (Port == 0)
    {
        Port = NetworkAllocatePortLocked();
    }

    if (Port == 0 || NetworkUDPPortInUseLocked(Port))
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        return NetworkError(L"UDP port in use or exhausted");
    }

    for (UINT32 Index = 0; Index < NETWORK_UDP_SOCKET_COUNT; Index++)
    {
        NETWORK_UDP_SOCKET *Entry = &Sockets[Index];

        if (!Entry->Open)
        {
            UINT32 Generation = (Entry->Generation + 1) & 0xFFFFFF;
            KMemSet(Entry, 0, sizeof(*Entry));
            Entry->Generation = Generation == 0 ? 1 : Generation;
            Entry->Open = TRUE;
            Entry->Port = Port;
            *Socket = (Entry->Generation << 8) | Index;
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            return TRUE;
        }
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return NetworkError(L"UDP socket table full");
}

BOOLEAN NetworkUDPClose(UINT32 Socket)
{
    UINT64              Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_UDP_SOCKET *Entry = FindSocket(Socket);

    if (Entry == NULL)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        return NetworkError(L"UDP handle invalid");
    }

    Entry->Open = FALSE;
    Entry->Count = 0;
    Entry->Reading = FALSE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return TRUE;
}

BOOLEAN NetworkUDPSendLocked(UINT16 LocalPort, UINT32 Address, UINT16 Port, CONST VOID *Buffer, UINT32 Length)
{
    UINT8  Packet[NETWORK_MTU - 20] = {0};
    UINT16 Checksum;

    if (Port == 0 || LocalPort == 0 || Length > NETWORK_UDP_MAX_PAYLOAD || (Length != 0 && Buffer == NULL) || (!NetworkState.Configured && !(LocalPort == 68 && Port == 67 && Address == UINT32_MAX)))
    {
        return NetworkError(L"UDP send arguments or address configuration invalid");
    }

    NetworkWrite16(Packet, LocalPort);
    NetworkWrite16(Packet + 2, Port);
    NetworkWrite16(Packet + 4, Length + 8);

    if (Length != 0)
    {
        KMemCpy(Packet + 8, Buffer, Length);
    }

    Checksum = NetworkTransportChecksum(NetworkState.Config.Address, Address, 17, Packet, Length + 8);
    NetworkWrite16(Packet + 6, Checksum == 0 ? 0xFFFF : Checksum);
    return NetworkSendIPv4Locked(Address, 17, Packet, Length + 8);
}

VOID NetworkUDPInputLocked(UINT32 Address, UINT32 Destination, CONST UINT8 *Packet, UINT32 Length)
{
    if (Length < 8 || Length > NETWORK_MTU - 20 || NetworkRead16(Packet + 4) != Length || NetworkRead16(Packet + 2) == 0 || (NetworkRead16(Packet + 6) != 0 && NetworkTransportChecksum(Address, Destination, 17, Packet, Length) != 0))
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    NetworkState.Stats.UDPReceived++;

    if (NetworkRead16(Packet) == 67 && NetworkRead16(Packet + 2) == 68)
    {
        NetworkDHCPInputLocked(Address, Packet + 8, Length - 8);
        return;
    }

    for (UINT32 Index = 0; Index < NETWORK_UDP_SOCKET_COUNT; Index++)
    {
        NETWORK_UDP_SOCKET *Entry = &Sockets[Index];

        if (!Entry->Open || Entry->Port != NetworkRead16(Packet + 2))
        {
            continue;
        }

        if (Entry->Count == NETWORK_UDP_QUEUE_SIZE)
        {
            NetworkState.Stats.Dropped++;
            return;
        }

        NETWORK_UDP_DATAGRAM *Datagram = &Entry->Queue[(Entry->Head + Entry->Count) % NETWORK_UDP_QUEUE_SIZE];
        Datagram->Address = Address;
        Datagram->Port = NetworkRead16(Packet);
        Datagram->Length = Length - 8;
        KMemCpy(Datagram->Data, Packet + 8, Length - 8);
        Entry->Count++;
        return;
    }
}

STATIC BOOLEAN SendTo(UINT32 Socket, UINT32 Address, UINT16 Port, CONST VOID *Buffer, UINT32 Length, UINT32 TimeoutMls)
{
    UINT64  Start = NetworkNow();
    UINT64  Flags;
    BOOLEAN Broadcast = Address == UINT32_MAX || (NetworkState.Configured && Address == (NetworkState.Config.Address | ~NetworkState.Config.Netmask));

    if (TimeoutMls == 0 || Length > NETWORK_UDP_MAX_PAYLOAD || (Length != 0 && Buffer == NULL) || Port == 0)
    {
        return NetworkError(L"UDP send arguments invalid");
    }

    if (!Broadcast && !NetworkResolve(Address, TimeoutMls))
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_UDP_SOCKET *Entry = FindSocket(Socket);
    BOOLEAN             Result = Entry != NULL && !NetworkExpired(Start, TimeoutMls) && NetworkUDPSendLocked(Entry->Port, Address, Port, Buffer, Length);
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

BOOLEAN NetworkUDPSendTo(UINT32 Socket, UINT32 Address, UINT16 Port, CONST VOID *Buffer, UINT32 Length, UINT32 TimeoutMls)
{
    BOOLEAN Result;

    if (!NetworkBegin())
    {
        return FALSE;
    }

    Result = SendTo(Socket, Address, Port, Buffer, Length, TimeoutMls);
    NetworkEnd();
    return Result;
}

BOOLEAN NetworkUDPReceiveFrom(UINT32 Socket, VOID *Buffer, UINT32 Capacity, UINT32 *Length, UINT32 *Address, UINT16 *Port, UINT32 TimeoutMls)
{
    UINT64  Start = NetworkNow();
    UINT64  Flags;
    BOOLEAN Result = FALSE;

    if (Length != NULL)
    {
        *Length = 0;
    }

    if (Length == NULL || Address == NULL || Port == NULL || (Capacity != 0 && Buffer == NULL))
    {
        return NetworkError(L"UDP receive arguments invalid");
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_UDP_SOCKET *Entry = FindSocket(Socket);

    if (Entry == NULL || Entry->Reading)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        return NetworkError(L"UDP handle invalid or already being read");
    }

    Entry->Reading = TRUE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    do
    {
        NetworkPoll();
        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
        Entry = FindSocket(Socket);

        if (Entry == NULL)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            return NetworkError(L"UDP socket closed during receive");
        }

        if (Entry->Count != 0)
        {
            NETWORK_UDP_DATAGRAM *Datagram = &Entry->Queue[Entry->Head];
            *Length = Datagram->Length;
            *Address = Datagram->Address;
            *Port = Datagram->Port;
            Result = Capacity >= *Length;

            if (Result)
            {
                if (*Length != 0)
                {
                    KMemCpy(Buffer, Datagram->Data, *Length);
                }

                Entry->Head = (Entry->Head + 1) % NETWORK_UDP_QUEUE_SIZE;
                Entry->Count--;
            }

            Entry->Reading = FALSE;
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            NetworkError(Result ? L"ok" : L"UDP buffer too small; datagram retained");
            return Result;
        }

        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        if (TimeoutMls != 0)
        {
            NetworkWait();
        }
    } while (!NetworkExpired(Start, TimeoutMls));

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    Entry = FindSocket(Socket);

    if (Entry != NULL)
    {
        Entry->Reading = FALSE;
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return NetworkError(L"UDP receive timeout");
}

BOOLEAN NetworkUDPExchangeInternal(UINT32 Address, UINT16 Port, CONST VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls)
{
    UINT32  Socket;
    UINT64  Start = NetworkNow();
    BOOLEAN Result = FALSE;
    UINT8   Packet[NETWORK_UDP_MAX_PAYLOAD];

    if (Length == NULL || Response == NULL || TimeoutMls == 0)
    {
        return NetworkError(L"UDP exchange arguments invalid");
    }

    *Length = 0;

    if (!NetworkUDPOpen(0, &Socket))
    {
        return FALSE;
    }

    if (SendTo(Socket, Address, Port, Request, RequestLength, TimeoutMls))
    {
        do
        {
            UINT32 ReceivedAddress;
            UINT16 ReceivedPort;
            UINT32 Size;

            if (NetworkUDPReceiveFrom(Socket, Packet, sizeof(Packet), &Size, &ReceivedAddress, &ReceivedPort, NetworkRemaining(Start, TimeoutMls)))
            {
                if (ReceivedAddress != Address || ReceivedPort != Port)
                {
                    continue;
                }

                *Length = Size;
                Result = Capacity >= Size;

                if (Result)
                {
                    KMemCpy(Response, Packet, Size);
                }
                else
                {
                    NetworkError(L"UDP exchange response too large");
                }

                break;
            }
        } while (!NetworkExpired(Start, TimeoutMls));
    }

    NetworkUDPClose(Socket);
    return Result;
}

BOOLEAN NetworkUDPExchange(UINT32 Address, UINT16 Port, CONST VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls)
{
    BOOLEAN Result;

    if (!NetworkBegin())
    {
        return FALSE;
    }

    Result = NetworkUDPExchangeInternal(Address, Port, Request, RequestLength, Response, Capacity, Length, TimeoutMls);
    NetworkEnd();
    return Result;
}
