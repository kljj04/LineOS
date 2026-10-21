// Kernel/Source/Network/DHCP.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>
#include <Arch/AMD64/CPU.h>

#define NETWORK_DHCP_OFF      0
#define NETWORK_DHCP_DISCOVER 1
#define NETWORK_DHCP_REQUEST  2
#define NETWORK_DHCP_BOUND    3
#define NETWORK_DHCP_RENEW    4
#define NETWORK_DHCP_REBIND   5

typedef struct
{
    UINT32         State;
    UINT32         Transaction;
    UINT32         Server;
    NETWORK_CONFIG Offer;
    UINT64         NextSend;
    UINT64         RenewAt;
    UINT64         RebindAt;
    UINT64         Expires;
    UINT32         RetryMls;
    BOOLEAN        Enabled;
    UINT32         Attempts;
} NETWORK_DHCP_STATE;

STATIC NETWORK_DHCP_STATE DHCP = {.Enabled = TRUE};

VOID NetworkDHCPStopLocked(VOID)
{
    KMemSet(&DHCP, 0, sizeof(DHCP));
}

STATIC VOID StartDiscover(VOID)
{
    DHCP.State = NETWORK_DHCP_DISCOVER;
    DHCP.Transaction = (UINT32) RDTSC();
    DHCP.Server = 0;
    DHCP.Attempts = 0;
    KMemSet(&DHCP.Offer, 0, sizeof(DHCP.Offer));
    DHCP.NextSend = 0;
    DHCP.RetryMls = 1000;
}

STATIC VOID SendDHCP(VOID)
{
    UINT8   Packet[300] = {0};
    UINT32  Offset = 240;
    BOOLEAN Renew = DHCP.State == NETWORK_DHCP_RENEW || DHCP.State == NETWORK_DHCP_REBIND;
    UINT32  Destination = UINT32_MAX;

    Packet[0] = 1;
    Packet[1] = 1;
    Packet[2] = 6;
    NetworkWrite32(Packet + 4, DHCP.Transaction);
    NetworkWrite16(Packet + 10, 0x8000);

    if (Renew)
    {
        NetworkWrite32(Packet + 12, NetworkState.Config.Address);
    }

    KMemCpy(Packet + 28, NetworkState.Device->MAC, 6);
    NetworkWrite32(Packet + 236, 0x63825363);
    Packet[Offset++] = 53;
    Packet[Offset++] = 1;
    Packet[Offset++] = DHCP.State == NETWORK_DHCP_DISCOVER ? 1 : 3;
    Packet[Offset++] = 61;
    Packet[Offset++] = 7;
    Packet[Offset++] = 1;
    KMemCpy(Packet + Offset, NetworkState.Device->MAC, 6);
    Offset += 6;

    if (DHCP.State == NETWORK_DHCP_REQUEST)
    {
        Packet[Offset++] = 50;
        Packet[Offset++] = 4;
        NetworkWrite32(Packet + Offset, DHCP.Offer.Address);
        Offset += 4;
        Packet[Offset++] = 54;
        Packet[Offset++] = 4;
        NetworkWrite32(Packet + Offset, DHCP.Server);
        Offset += 4;
    }

    Packet[Offset++] = 55;
    Packet[Offset++] = 6;
    Packet[Offset++] = 1;
    Packet[Offset++] = 3;
    Packet[Offset++] = 6;
    Packet[Offset++] = 51;
    Packet[Offset++] = 58;
    Packet[Offset++] = 59;
    Packet[Offset] = 255;

    if (DHCP.State == NETWORK_DHCP_RENEW)
    {
        Destination = DHCP.Server;
    }

    if (!NetworkUDPSendLocked(68, Destination, 67, Packet, sizeof(Packet)) && Destination != UINT32_MAX)
    {
        NetworkUDPSendLocked(68, UINT32_MAX, 67, Packet, sizeof(Packet));
    }
}

VOID NetworkDHCPTick(VOID)
{
    UINT64 Flags;
    UINT64 Now;

    if (!NetworkState.Ready)
    {
        return;
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    Now = NetworkNow();

    if (!DHCP.Enabled)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        return;
    }

    if (DHCP.State >= NETWORK_DHCP_BOUND && Now >= DHCP.Expires)
    {
        NetworkClearConfigLocked();
        StartDiscover();
    }

    if (DHCP.State == NETWORK_DHCP_OFF)
    {
        StartDiscover();
    }
    else if (DHCP.State == NETWORK_DHCP_BOUND && Now >= DHCP.RenewAt)
    {
        DHCP.State = NETWORK_DHCP_RENEW;
        DHCP.Transaction = (UINT32) RDTSC();
        DHCP.NextSend = 0;
        DHCP.RetryMls = 1000;
    }
    else if (DHCP.State == NETWORK_DHCP_RENEW && Now >= DHCP.RebindAt)
    {
        DHCP.State = NETWORK_DHCP_REBIND;
        DHCP.NextSend = 0;
    }

    if (DHCP.State != NETWORK_DHCP_BOUND && Now >= DHCP.NextSend)
    {
        if (DHCP.State == NETWORK_DHCP_REQUEST && DHCP.Attempts >= 3)
        {
            StartDiscover();
        }

        DHCP.Attempts++;
        SendDHCP();
        DHCP.NextSend = Now + DHCP.RetryMls;
        DHCP.RetryMls = DHCP.RetryMls < 16000 ? DHCP.RetryMls * 2 : 16000;
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
}

VOID NetworkDHCPInputLocked(UINT32 Source, CONST UINT8 *Packet, UINT32 Length)
{
    UINT32         Offset = 240;
    UINT32         Type = 0;
    UINT32         Server = 0;
    UINT32         Lease = 0;
    UINT32         T1 = 0;
    UINT32         T2 = 0;
    BOOLEAN        End = FALSE;
    NETWORK_CONFIG Config = DHCP.Offer;

    if (!DHCP.Enabled || Length < 240 || Packet[0] != 2 || Packet[1] != 1 || Packet[2] != 6 || NetworkRead32(Packet + 4) != DHCP.Transaction || NetworkRead32(Packet + 236) != 0x63825363)
    {
        return;
    }

    for (UINT32 Index = 0; Index < 6; Index++)
    {
        if (Packet[28 + Index] != NetworkState.Device->MAC[Index])
        {
            return;
        }
    }

    while (Offset < Length)
    {
        UINT8 Option = Packet[Offset++];

        if (Option == 255)
        {
            End = TRUE;
            break;
        }

        if (Option == 0)
        {
            continue;
        }

        if (Offset >= Length)
        {
            return;
        }

        UINT32 Size = Packet[Offset++];

        if (Size > Length - Offset)
        {
            return;
        }

        if (Option == 53)
        {
            if (Size != 1 || Type != 0)
                return;
            Type = Packet[Offset];
        }
        else if (Option == 1 || Option == 54 || Option == 51 || Option == 58 || Option == 59)
        {
            if (Size != 4)
                return;
            UINT32 Value = NetworkRead32(Packet + Offset);
            if (Option == 1)
                Config.Netmask = Value;
            if (Option == 54)
                Server = Value;
            if (Option == 51)
                Lease = Value;
            if (Option == 58)
                T1 = Value;
            if (Option == 59)
                T2 = Value;
        }
        else if (Option == 3 || Option == 6)
        {
            if (Size < 4 || Size % 4 != 0)
                return;
            if (Option == 3)
                Config.Gateway = NetworkRead32(Packet + Offset);
            else
                Config.DNS = NetworkRead32(Packet + Offset);
        }

        Offset += Size;
    }

    if (!End || Server == 0 || (Server >> 24) >= 224 || Source == 0)
    {
        return;
    }

    if (Type == 6 && DHCP.State >= NETWORK_DHCP_REQUEST && (DHCP.State == NETWORK_DHCP_REBIND || DHCP.Server == Server))
    {
        NetworkClearConfigLocked();
        StartDiscover();
        return;
    }

    Config.Address = NetworkRead32(Packet + 16);

    if (Type == 2 && DHCP.State == NETWORK_DHCP_DISCOVER && Config.Address != 0 && Config.Netmask != 0)
    {
        DHCP.Offer = Config;
        DHCP.Server = Server;
        DHCP.State = NETWORK_DHCP_REQUEST;
        DHCP.Attempts = 0;
        DHCP.NextSend = 0;
        DHCP.RetryMls = 1000;
    }
    else if (Type == 5 && DHCP.State >= NETWORK_DHCP_REQUEST && (DHCP.State == NETWORK_DHCP_REBIND || Server == DHCP.Server) && Lease >= 4)
    {
        if (Config.Address == 0 && NetworkState.Configured)
        {
            Config.Address = NetworkState.Config.Address;
        }

        if (DHCP.State == NETWORK_DHCP_REQUEST && Config.Address != DHCP.Offer.Address)
        {
            return;
        }

        if (!NetworkSetConfigLocked(&Config))
        {
            return;
        }

        if (T1 == 0 || T1 >= Lease)
            T1 = Lease / 2;
        if (T2 <= T1 || T2 >= Lease)
            T2 = (UINT32) ((UINT64) Lease * 7 / 8);
        DHCP.Offer = Config;
        DHCP.Server = Server;
        DHCP.State = NETWORK_DHCP_BOUND;
        UINT64 Now = NetworkNow();
        DHCP.RenewAt = Now + (UINT64) T1 * 1000;
        DHCP.RebindAt = Now + (UINT64) T2 * 1000;
        DHCP.Expires = Now + (UINT64) Lease * 1000;
    }
}

BOOLEAN NetworkDHCPConfigure(UINT32 TimeoutMls)
{
    UINT64 Start = NetworkNow();

    if (!NetworkBegin())
    {
        return FALSE;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    DHCP.Enabled = TRUE;

    if (DHCP.State == NETWORK_DHCP_OFF)
    {
        NetworkClearConfigLocked();
        StartDiscover();
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    do
    {
        NetworkDHCPTick();
        NetworkWait();
        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
        BOOLEAN Configured = NetworkState.Configured && DHCP.State == NETWORK_DHCP_BOUND;
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        if (Configured)
        {
            NetworkEnd();
            return TRUE;
        }
    } while (!NetworkExpired(Start, TimeoutMls));

    NetworkEnd();
    return NetworkError(L"DHCP timeout; background discovery remains enabled");
}
