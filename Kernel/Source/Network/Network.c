// Kernel/Source/Network/Network.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>
#include <Timer/HPET.h>
#include <Timer/TSC.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Arch/AMD64/CPU.h>

NETWORK_STATE        NetworkState;
STATIC CONST CHAR16 *LastError = L"not initialized";

UINT16 NetworkRead16(CONST UINT8 *Data)
{
    return ((UINT16) Data[0] << 8) | Data[1];
}

UINT32 NetworkRead32(CONST UINT8 *Data)
{
    return ((UINT32) NetworkRead16(Data) << 16) | NetworkRead16(Data + 2);
}

VOID NetworkWrite16(UINT8 *Data, UINT16 Value)
{
    Data[0] = Value >> 8;
    Data[1] = Value;
}

VOID NetworkWrite32(UINT8 *Data, UINT32 Value)
{
    NetworkWrite16(Data, Value >> 16);
    NetworkWrite16(Data + 2, Value);
}

UINT16 NetworkChecksum(CONST UINT8 *Data, UINT32 Length, UINT32 Sum)
{
    while (Length >= 2)
    {
        Sum += NetworkRead16(Data);
        Data += 2;
        Length -= 2;
    }

    if (Length != 0)
    {
        Sum += (UINT16) Data[0] << 8;
    }

    while (Sum >> 16)
    {
        Sum = (Sum & 0xFFFF) + (Sum >> 16);
    }

    return (UINT16) ~Sum;
}

UINT16 NetworkTransportChecksum(UINT32 Source, UINT32 Destination, UINT8 Protocol, CONST UINT8 *Data, UINT32 Length)
{
    UINT32 Sum = (Source >> 16) + (Source & 0xFFFF) + (Destination >> 16) + (Destination & 0xFFFF) + Protocol + Length;
    return NetworkChecksum(Data, Length, Sum);
}

UINT64 NetworkNow(VOID)
{
    UINT64 Frequency = HPETGetFrequency();
    return Frequency == 0 ? 0 : (UINT64) ((UINT128) HPETReadCounter() * 1000 / Frequency);
}

UINT32 NetworkRemaining(UINT64 Start, UINT32 TimeoutMls)
{
    UINT64 Elapsed = NetworkNow() - Start;
    return Elapsed >= TimeoutMls ? 0 : TimeoutMls - (UINT32) Elapsed;
}

BOOLEAN NetworkExpired(UINT64 Start, UINT32 TimeoutMls)
{
    return NetworkNow() - Start >= TimeoutMls;
}

BOOLEAN NetworkError(CONST CHAR16 *Error)
{
    __atomic_store_n(&LastError, Error, __ATOMIC_RELEASE);
    return FALSE;
}

CONST CHAR16 *NetworkGetLastError(VOID)
{
    return __atomic_load_n(&LastError, __ATOMIC_ACQUIRE);
}

BOOLEAN NetworkBegin(VOID)
{
    UINT32 Expected = 0;

    if (!NetworkState.Ready)
    {
        return NetworkError(L"network unavailable");
    }

    if (!__atomic_compare_exchange_n(&NetworkState.Busy, &Expected, 1, FALSE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    {
        return NetworkError(L"network operation busy");
    }

    return TRUE;
}

VOID NetworkEnd(VOID)
{
    __atomic_store_n(&NetworkState.Busy, 0, __ATOMIC_RELEASE);
}

STATIC BOOLEAN IsUnicast(UINT32 Address)
{
    UINT8 First = Address >> 24;
    return First != 0 && First != 127 && First < 224 && Address != UINT32_MAX;
}

STATIC BOOLEAN EqualMAC(CONST UINT8 *A, CONST UINT8 *B)
{
    for (UINT32 Index = 0; Index < 6; Index++)
    {
        if (A[Index] != B[Index])
        {
            return FALSE;
        }
    }

    return TRUE;
}

STATIC BOOLEAN IsValidMAC(CONST UINT8 *MAC)
{
    UINT8 Combined = 0;

    for (UINT32 Index = 0; Index < 6; Index++)
    {
        Combined |= MAC[Index];
    }

    return Combined != 0 && (MAC[0] & 1) == 0;
}

STATIC BOOLEAN IsValidDestination(UINT32 Address)
{
    UINT32 HostMask = ~NetworkState.Config.Netmask;

    if (!IsUnicast(Address) || Address == NetworkState.Config.Address)
    {
        return FALSE;
    }

    return (Address & NetworkState.Config.Netmask) != (NetworkState.Config.Address & NetworkState.Config.Netmask) || ((Address & HostMask) != 0 && (Address & HostMask) != HostMask);
}

STATIC BOOLEAN IsBroadcastMAC(CONST UINT8 *MAC)
{
    STATIC CONST UINT8 Broadcast[6] = {255, 255, 255, 255, 255, 255};
    return EqualMAC(MAC, Broadcast);
}

STATIC UINT32 NextHop(UINT32 Address)
{
    return (Address & NetworkState.Config.Netmask) == (NetworkState.Config.Address & NetworkState.Config.Netmask) ? Address : NetworkState.Config.Gateway;
}

STATIC NETWORK_ARP_ENTRY *FindARP(UINT32 Address)
{
    for (UINT32 Index = 0; Index < NETWORK_ARP_COUNT; Index++)
    {
        NETWORK_ARP_ENTRY *Entry = &NetworkState.ARP[Index];

        if (Entry->Address == Address && NetworkNow() - Entry->Updated < 60000)
        {
            return Entry;
        }
    }

    return NULL;
}

STATIC BOOLEAN SendFrame(CONST UINT8 *MAC, UINT16 Type, CONST UINT8 *Payload, UINT32 Length)
{
    UINT8 Frame[VIRTIO_NETWORK_MAX_FRAME_SIZE];

    if (Length > NETWORK_MTU)
    {
        return NetworkError(L"network MTU exceeded");
    }

    KMemCpy(Frame, MAC, 6);
    KMemCpy(Frame + 6, NetworkState.Device->MAC, 6);
    NetworkWrite16(Frame + 12, Type);
    KMemCpy(Frame + 14, Payload, Length);

    if (!VirtIONetworkSend(NetworkState.Device, Frame, Length + 14))
    {
        return NetworkError(VirtIONetworkGetLastError());
    }

    NetworkState.Stats.Transmitted++;
    return TRUE;
}

STATIC BOOLEAN SendARP(UINT16 Operation, UINT32 Address, CONST UINT8 *MAC)
{
    UINT8              Packet[28] = {0};
    STATIC CONST UINT8 Broadcast[6] = {255, 255, 255, 255, 255, 255};

    NetworkWrite16(Packet, 1);
    NetworkWrite16(Packet + 2, 0x0800);
    Packet[4] = 6;
    Packet[5] = 4;
    NetworkWrite16(Packet + 6, Operation);
    KMemCpy(Packet + 8, NetworkState.Device->MAC, 6);
    NetworkWrite32(Packet + 14, NetworkState.Config.Address);

    if (MAC != NULL)
    {
        KMemCpy(Packet + 18, MAC, 6);
    }

    NetworkWrite32(Packet + 24, Address);
    return SendFrame(MAC == NULL ? Broadcast : MAC, 0x0806, Packet, sizeof(Packet));
}

STATIC VOID HandleARP(CONST UINT8 *SourceMAC, CONST UINT8 *Packet, UINT32 Length)
{
    UINT32             Address;
    UINT16             Operation;
    NETWORK_ARP_ENTRY *Entry;

    if (Length < 28 || NetworkRead16(Packet) != 1 || NetworkRead16(Packet + 2) != 0x0800 || Packet[4] != 6 || Packet[5] != 4 || !EqualMAC(Packet + 8, SourceMAC) || !IsValidMAC(SourceMAC) || NetworkRead32(Packet + 24) != NetworkState.Config.Address)
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    Operation = NetworkRead16(Packet + 6);
    Address = NetworkRead32(Packet + 14);

    if ((Operation != 1 && Operation != 2) || !IsUnicast(Address) || Address == NetworkState.Config.Address || (Address & NetworkState.Config.Netmask) != (NetworkState.Config.Address & NetworkState.Config.Netmask) || (Operation == 2 && !EqualMAC(Packet + 18, NetworkState.Device->MAC)))
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    Entry = FindARP(Address);

    if (Entry == NULL)
    {
        Entry = &NetworkState.ARP[0];

        for (UINT32 Index = 0; Index < NETWORK_ARP_COUNT; Index++)
        {
            if (NetworkState.ARP[Index].Address == 0 || NetworkState.ARP[Index].Updated < Entry->Updated)
            {
                Entry = &NetworkState.ARP[Index];
            }
        }
    }

    Entry->Address = Address;
    Entry->Updated = NetworkNow();
    KMemCpy(Entry->MAC, SourceMAC, 6);
    NetworkState.Stats.ARPReceived++;

    if (Operation == 1)
    {
        SendARP(2, Address, SourceMAC);
    }
}

BOOLEAN NetworkSendIPv4Locked(UINT32 Address, UINT8 Protocol, CONST UINT8 *Payload, UINT32 Length)
{
    UINT8              Packet[NETWORK_MTU] = {0};
    NETWORK_ARP_ENTRY *Entry = FindARP(NextHop(Address));

    if (!IsValidDestination(Address) || Entry == NULL || Length > NETWORK_MTU - 20)
    {
        return NetworkError(L"IPv4 route unresolved or packet too large");
    }

    Packet[0] = 0x45;
    NetworkWrite16(Packet + 2, Length + 20);
    NetworkWrite16(Packet + 4, ++NetworkState.Identification);
    NetworkWrite16(Packet + 6, 0x4000);
    Packet[8] = 64;
    Packet[9] = Protocol;
    NetworkWrite32(Packet + 12, NetworkState.Config.Address);
    NetworkWrite32(Packet + 16, Address);
    NetworkWrite16(Packet + 10, NetworkChecksum(Packet, 20, 0));
    KMemCpy(Packet + 20, Payload, Length);
    return SendFrame(Entry->MAC, 0x0800, Packet, Length + 20);
}

STATIC VOID HandleICMP(UINT32 Address, CONST UINT8 *Packet, UINT32 Length)
{
    if (Length < 8 || Packet[1] != 0 || NetworkChecksum(Packet, Length, 0) != 0)
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    NetworkState.Stats.ICMPReceived++;

    if (Packet[0] == 8)
    {
        UINT8 Reply[NETWORK_MTU - 20];

        KMemCpy(Reply, Packet, Length);
        Reply[0] = 0;
        Reply[2] = 0;
        Reply[3] = 0;
        NetworkWrite16(Reply + 2, NetworkChecksum(Reply, Length, 0));
        NetworkSendIPv4Locked(Address, 1, Reply, Length);
    }
    else if (Packet[0] == 0 && NetworkState.PingPending && Address == NetworkState.PingAddress && Length == 24 && NetworkRead16(Packet + 4) == NetworkState.PingID && NetworkRead16(Packet + 6) == 1)
    {
        for (UINT32 Index = 0; Index < 16; Index++)
        {
            if (Packet[8 + Index] != NetworkState.PingData[Index])
            {
                return;
            }
        }

        NetworkState.PingReply = TRUE;
    }
}

STATIC VOID HandleUDP(UINT32 Address, CONST UINT8 *Packet, UINT32 Length)
{
    if (Length < 8 || NetworkRead16(Packet + 4) != Length || (NetworkRead16(Packet + 6) != 0 && NetworkTransportChecksum(Address, NetworkState.Config.Address, 17, Packet, Length) != 0))
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    NetworkState.Stats.UDPReceived++;

    if (NetworkState.UDPPending && !NetworkState.UDPReply && NetworkState.UDPAddress == Address && NetworkState.UDPPort == NetworkRead16(Packet) && NetworkState.UDPLocalPort == NetworkRead16(Packet + 2))
    {
        NetworkState.UDPLength = Length - 8;
        KMemCpy(NetworkState.UDPData, Packet + 8, Length - 8);
        NetworkState.UDPReply = TRUE;
    }
}

STATIC VOID HandleIPv4(CONST UINT8 *Packet, UINT32 Length)
{
    UINT32 HeaderLength;
    UINT32 TotalLength;
    UINT32 Source;

    if (Length < 20)
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    HeaderLength = (Packet[0] & 15) * 4;
    TotalLength = NetworkRead16(Packet + 2);
    Source = NetworkRead32(Packet + 12);

    if ((Packet[0] >> 4) != 4 || HeaderLength < 20 || HeaderLength > Length || TotalLength < HeaderLength || TotalLength > Length || TotalLength > NETWORK_MTU || Packet[8] == 0 || (NetworkRead16(Packet + 6) & 0xBFFF) != 0 || NetworkChecksum(Packet, HeaderLength, 0) != 0 || NetworkRead32(Packet + 16) != NetworkState.Config.Address || !IsValidDestination(Source))
    {
        NetworkState.Stats.Dropped++;
        return;
    }

    if (Packet[9] == 1)
    {
        HandleICMP(Source, Packet + HeaderLength, TotalLength - HeaderLength);
    }
    else if (Packet[9] == 17)
    {
        HandleUDP(Source, Packet + HeaderLength, TotalLength - HeaderLength);
    }
    else if (Packet[9] == 6)
    {
        NetworkTCPInputLocked(Source, Packet + HeaderLength, TotalLength - HeaderLength);
    }
}

VOID NetworkPoll(VOID)
{
    UINT8  Frame[VIRTIO_NETWORK_MAX_FRAME_SIZE];
    UINT32 Length;
    UINT64 Flags;

    if (!NetworkState.Ready)
    {
        return;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

    for (UINT32 Count = 0; Count < VIRTIO_NETWORK_QUEUE_SIZE && VirtIONetworkReceive(NetworkState.Device, Frame, sizeof(Frame), &Length); Count++)
    {
        NetworkState.Stats.Received++;

        if (Length < 14 || (!EqualMAC(Frame, NetworkState.Device->MAC) && !IsBroadcastMAC(Frame)) || !IsValidMAC(Frame + 6))
        {
            NetworkState.Stats.Dropped++;
            continue;
        }

        if (NetworkRead16(Frame + 12) == 0x0806)
        {
            HandleARP(Frame + 6, Frame + 14, Length - 14);
        }
        else if (NetworkRead16(Frame + 12) == 0x0800 && EqualMAC(Frame, NetworkState.Device->MAC))
        {
            HandleIPv4(Frame + 14, Length - 14);
        }
    }

    for (UINT32 Index = 0; Index < NETWORK_ARP_COUNT; Index++)
    {
        NETWORK_ARP_ENTRY *Entry = &NetworkState.ARP[Index];
        UINT64             Now = NetworkNow();

        if (Entry->Address != 0 && Now - Entry->Updated >= 30000 && Now - Entry->Updated < 60000 && Now - Entry->Requested >= 1000)
        {
            SendARP(1, Entry->Address, NULL);
            Entry->Requested = Now;
        }
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
}

VOID NetworkWait(VOID)
{
    NetworkPoll();

    if (LBPWRRGetCurrentTask() != NULL)
    {
        TaskSleepMls(1);
    }
    else
    {
        PAUSE();
    }
}

VOID NetworkTask(VOID)
{
    for (;;)
    {
        NetworkPoll();
        TaskSleepMls(1);
    }
}

BOOLEAN NetworkResolve(UINT32 Address, UINT32 TimeoutMls)
{
    UINT64 Start = NetworkNow();
    UINT64 Last = Start - 1000;
    UINT64 Flags;
    UINT32 Hop;

    if (!IsValidDestination(Address) || TimeoutMls == 0)
    {
        return NetworkError(L"invalid destination or timeout");
    }

    Hop = NextHop(Address);

    do
    {
        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

        if (FindARP(Hop) != NULL)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            return TRUE;
        }

        if (NetworkNow() - Last >= 1000)
        {
            SendARP(1, Hop, NULL);
            Last = NetworkNow();
        }

        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        NetworkWait();
    } while (!NetworkExpired(Start, TimeoutMls));

    return NetworkError(L"ARP resolution timeout");
}

BOOLEAN NetworkConfigure(CONST NETWORK_CONFIG *Config)
{
    UINT64 Flags;
    UINT32 HostMask;

    if (Config == NULL || !IsUnicast(Config->Address) || !IsUnicast(Config->Gateway) || !IsUnicast(Config->DNS))
    {
        return NetworkError(L"invalid IPv4 configuration");
    }

    HostMask = ~Config->Netmask;

    if (HostMask < 3 || (HostMask & (HostMask + 1)) != 0 || Config->Netmask == 0 || (Config->Address & HostMask) == 0 || (Config->Address & HostMask) == HostMask || (Config->Gateway & Config->Netmask) != (Config->Address & Config->Netmask) || Config->Gateway == Config->Address || (Config->Gateway & HostMask) == 0 || (Config->Gateway & HostMask) == HostMask)
    {
        return NetworkError(L"invalid netmask or gateway");
    }

    if (!NetworkBegin())
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

    if (NetworkTCPActiveLocked())
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        NetworkEnd();
        return NetworkError(L"TCP connection active");
    }

    NetworkState.Config = *Config;
    KMemSet(NetworkState.ARP, 0, sizeof(NetworkState.ARP));
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    NetworkEnd();
    NetworkError(L"ok");
    return TRUE;
}

BOOLEAN NetworkInit(VOID)
{
    if (NetworkState.Ready)
    {
        return TRUE;
    }

    if (HPETGetFrequency() == 0 || VirtIONetworkGetDevice() == NULL)
    {
        return NetworkError(L"network device or monotonic clock unavailable");
    }

    SpinLockInit(&NetworkState.Lock);
    NetworkState.Device = VirtIONetworkGetDevice();
    NetworkState.NextPort = 49152 + (RDTSC() % 10000);
    NetworkState.Config.Address = NETWORK_IPV4(10, 0, 2, 15);
    NetworkState.Config.Netmask = NETWORK_IPV4(255, 255, 255, 0);
    NetworkState.Config.Gateway = NETWORK_IPV4(10, 0, 2, 2);
    NetworkState.Config.DNS = NETWORK_IPV4(10, 0, 2, 3);
    NetworkState.Ready = TRUE;
    NetworkError(L"ok");
    return TRUE;
}

BOOLEAN NetworkGetConfig(NETWORK_CONFIG *Config)
{
    UINT64 Flags;

    if (Config == NULL || !NetworkState.Ready)
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    *Config = NetworkState.Config;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return TRUE;
}

VOID NetworkGetStats(NETWORK_STATS *Stats)
{
    UINT64 Flags;

    if (Stats == NULL)
    {
        return;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    *Stats = NetworkState.Stats;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
}

BOOLEAN NetworkPing(UINT32 Address, UINT32 TimeoutMls, UINT32 *RoundTripMls)
{
    UINT64  DeadlineStart;
    UINT8   Packet[24] = {8, 0};
    UINT64  Start;
    UINT64  Flags;
    BOOLEAN Result = FALSE;

    if (RoundTripMls == NULL || TimeoutMls == 0 || !NetworkBegin())
    {
        return NetworkError(L"invalid ping arguments or busy");
    }

    *RoundTripMls = 0;
    DeadlineStart = NetworkNow();

    if (!NetworkResolve(Address, TimeoutMls))
    {
        NetworkEnd();
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NetworkState.PingID++;
    NetworkState.PingAddress = Address;
    NetworkState.PingReply = FALSE;
    NetworkState.PingPending = TRUE;
    NetworkWrite16(Packet + 4, NetworkState.PingID);
    NetworkWrite16(Packet + 6, 1);

    for (UINT32 Index = 0; Index < 16; Index++)
    {
        Packet[8 + Index] = (UINT8) (RDTSC() >> (Index % 8));
        NetworkState.PingData[Index] = Packet[8 + Index];
    }

    NetworkWrite16(Packet + 2, NetworkChecksum(Packet, sizeof(Packet), 0));
    Start = NetworkNow();

    if (NetworkSendIPv4Locked(Address, 1, Packet, sizeof(Packet)))
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        do
        {
            NetworkWait();
            Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
            Result = NetworkState.PingReply;
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        } while (!Result && !NetworkExpired(DeadlineStart, TimeoutMls));

        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    }

    NetworkState.PingPending = FALSE;
    *RoundTripMls = (UINT32) (NetworkNow() - Start);
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    NetworkEnd();
    NetworkError(Result ? L"ok" : L"ICMP echo timeout");
    return Result;
}

BOOLEAN NetworkUDPExchangeInternal(UINT32 Address, UINT16 Port, CONST VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls)
{
    UINT8   Packet[NETWORK_MTU - 20] = {0};
    UINT64  Flags;
    UINT64  Start;
    UINT16  Checksum;
    BOOLEAN Result = FALSE;

    if (Port == 0 || RequestLength > NETWORK_UDP_MAX_PAYLOAD || (RequestLength != 0 && Request == NULL) || Response == NULL || Length == NULL || TimeoutMls == 0)
    {
        return NetworkError(L"invalid UDP arguments");
    }

    *Length = 0;
    Start = NetworkNow();

    if (!NetworkResolve(Address, TimeoutMls))
    {
        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NetworkState.NextPort = NetworkState.NextPort == 65535 ? 49152 : NetworkState.NextPort + 1;
    NetworkState.UDPLocalPort = NetworkState.NextPort;
    NetworkState.UDPAddress = Address;
    NetworkState.UDPPort = Port;
    NetworkState.UDPReply = FALSE;
    NetworkState.UDPPending = TRUE;
    NetworkWrite16(Packet, NetworkState.UDPLocalPort);
    NetworkWrite16(Packet + 2, Port);
    NetworkWrite16(Packet + 4, RequestLength + 8);

    if (RequestLength != 0)
    {
        KMemCpy(Packet + 8, Request, RequestLength);
    }

    Checksum = NetworkTransportChecksum(NetworkState.Config.Address, Address, 17, Packet, RequestLength + 8);
    NetworkWrite16(Packet + 6, Checksum == 0 ? 0xFFFF : Checksum);

    if (NetworkSendIPv4Locked(Address, 17, Packet, RequestLength + 8))
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        do
        {
            NetworkWait();
            Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
            Result = NetworkState.UDPReply;
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        } while (!Result && !NetworkExpired(Start, TimeoutMls));

        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    }

    NetworkState.UDPPending = FALSE;

    if (Result)
    {
        *Length = NetworkState.UDPLength;
        Result = Capacity >= *Length;

        if (Result)
        {
            KMemCpy(Response, NetworkState.UDPData, *Length);
        }
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    NetworkError(Result ? L"ok" : (*Length > Capacity ? L"UDP response buffer too small" : L"UDP response timeout"));
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
