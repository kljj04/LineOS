// Kernel/Source/Network/TCP.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>
#include <Arch/AMD64/CPU.h>
#include <Syscall/Errno.h>

#define NETWORK_TCP_FIN 0x01
#define NETWORK_TCP_SYN 0x02
#define NETWORK_TCP_RST 0x04
#define NETWORK_TCP_PSH 0x08
#define NETWORK_TCP_ACK 0x10
#define NETWORK_TCP_MSS 536

typedef struct
{
    BOOLEAN SocketAPI;
    BOOLEAN Listening;
    BOOLEAN Connecting;
    BOOLEAN SynReceived;
    BOOLEAN PendingAccept;
    BOOLEAN Detached;
    BOOLEAN ReadClosed;
    BOOLEAN WriteClosed;
    UINT32  Parent;
    UINT32  Backlog;
    INT32   Error;
    UINT64  Started;
    UINT64  SentAt;
    UINT32  RetryMls;
    UINT32  TxSequence;
    UINT32  TxLength;
    UINT8   TxData[NETWORK_TCP_MSS];
    UINT32  Generation;
    BOOLEAN Writing;
    BOOLEAN Reading;
    BOOLEAN Closing;
    UINT32  BoundAddress;
    UINT32  Address;
    UINT16  Port;
    UINT16  LocalPort;
    UINT32  SendUnacknowledged;
    UINT32  SendNext;
    UINT32  ReceiveNext;
    UINT16  PeerWindow;
    UINT16  PeerMSS;
    UINT32  WindowSequence;
    UINT32  WindowACK;
    UINT8   LastByte;
    UINT8   Buffer[NETWORK_TCP_BUFFER_SIZE];
    UINT32  Head;
    UINT32  Count;
    BOOLEAN Active;
    BOOLEAN Connected;
    BOOLEAN PeerFIN;
    BOOLEAN Reset;
    BOOLEAN FINSent;
} NETWORK_TCP_CONNECTION;

STATIC NETWORK_TCP_CONNECTION Connections[NETWORK_TCP_SOCKET_COUNT];

STATIC NETWORK_TCP_CONNECTION *PassiveOpen(UINT32 Address, CONST UINT8 *Packet, UINT32 Length);
STATIC VOID                    SocketTickLocked(VOID);

#define NETWORK_TCP_TIME_WAIT_COUNT 64

typedef struct
{
    UINT32 Address;
    UINT16 Port;
    UINT16 LocalPort;
    UINT32 SendNext;
    UINT32 ReceiveNext;
    UINT64 Expires;
} NETWORK_TCP_TIME_WAIT;

STATIC NETWORK_TCP_TIME_WAIT TimeWait[NETWORK_TCP_TIME_WAIT_COUNT];

STATIC NETWORK_TCP_CONNECTION *FindConnection(UINT32 Socket)
{
    UINT32 Index = Socket & 255;

    if (Index >= NETWORK_TCP_SOCKET_COUNT || !Connections[Index].Active || Connections[Index].Generation != (Socket >> 8))
    {
        return NULL;
    }

    return &Connections[Index];
}

STATIC VOID ResetConnection(NETWORK_TCP_CONNECTION *Connection)
{
    Connection->Active    = FALSE;
    Connection->Connected = FALSE;
    Connection->Reset     = TRUE;
    Connection->Count     = 0;
}

VOID NetworkTCPResetLocked(VOID)
{
    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        NETWORK_TCP_CONNECTION *Entry = &Connections[Index];
        if (Entry->Active && Entry->SocketAPI && !Entry->Connected && !Entry->Connecting && !Entry->SynReceived && Entry->BoundAddress == 0)
        {
            continue;
        }
        if (Entry->Active && Entry->SocketAPI && !Entry->PendingAccept && !Entry->Detached)
        {
            Entry->Reset     = TRUE;
            Entry->Error     = ENETRESET;
            Entry->Listening = FALSE;
            continue;
        }
        ResetConnection(Entry);
    }

    KMemSet(TimeWait, 0, sizeof(TimeWait));
}

BOOLEAN NetworkTCPPortInUseLocked(UINT16 Port)
{
    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        if (Connections[Index].Active && Connections[Index].LocalPort == Port)
        {
            return TRUE;
        }
    }

    for (UINT32 Index = 0; Index < NETWORK_TCP_TIME_WAIT_COUNT; Index++)
    {
        if (TimeWait[Index].LocalPort == Port && NetworkNow() < TimeWait[Index].Expires)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC BOOLEAN FinishClose(NETWORK_TCP_CONNECTION *Connection)
{
    for (UINT32 Index = 0; Index < NETWORK_TCP_TIME_WAIT_COUNT; Index++)
    {
        NETWORK_TCP_TIME_WAIT *Entry = &TimeWait[Index];

        if (NetworkNow() >= Entry->Expires)
        {
            Entry->Address     = Connection->Address;
            Entry->Port        = Connection->Port;
            Entry->LocalPort   = Connection->LocalPort;
            Entry->SendNext    = Connection->SendNext;
            Entry->ReceiveNext = Connection->ReceiveNext;
            Entry->Expires     = NetworkNow() + 120000;
            ResetConnection(Connection);

            return TRUE;
        }
    }

    return NetworkError(L"TCP TIME_WAIT capacity exhausted");
}

STATIC VOID HandleTimeWait(UINT32 Address, CONST UINT8 *Packet, UINT32 Length)
{
    for (UINT32 Index = 0; Index < NETWORK_TCP_TIME_WAIT_COUNT; Index++)
    {
        NETWORK_TCP_TIME_WAIT *Entry = &TimeWait[Index];

        if (NetworkNow() >= Entry->Expires || Entry->Address != Address || Entry->Port != NetworkRead16(Packet) || Entry->LocalPort != NetworkRead16(Packet + 2) || (Packet[13] & (NETWORK_TCP_FIN | NETWORK_TCP_ACK | NETWORK_TCP_SYN | NETWORK_TCP_RST)) != (NETWORK_TCP_FIN | NETWORK_TCP_ACK) || NetworkRead32(Packet + 8) != Entry->SendNext || NetworkRead32(Packet + 4) + Length - (Packet[12] >> 4) * 4 + 1 != Entry->ReceiveNext)
        {
            continue;
        }

        UINT8 Reply[20] = {0};

        NetworkWrite16(Reply, Entry->LocalPort);
        NetworkWrite16(Reply + 2, Entry->Port);
        NetworkWrite32(Reply + 4, Entry->SendNext);
        NetworkWrite32(Reply + 8, Entry->ReceiveNext);
        Reply[12] = 0x50;
        Reply[13] = NETWORK_TCP_ACK;
        NetworkWrite16(Reply + 16, NetworkTransportChecksum(NetworkState.Config.Address, Address, 6, Reply, sizeof(Reply)));
        NetworkSendIPv4Locked(Address, 6, Reply, sizeof(Reply));
        Entry->Expires = NetworkNow() + 120000;

        return;
    }
}

STATIC BOOLEAN SequenceBefore(UINT32 A, UINT32 B)
{
    return (INT32) (A - B) < 0;
}

STATIC BOOLEAN SendSegment(NETWORK_TCP_CONNECTION *Connection, UINT32 Sequence, UINT8 Flags, CONST UINT8 *Data, UINT32 Length)
{
    UINT8  Packet[NETWORK_MTU - 20] = {0};
    UINT32 HeaderLength             = (Flags & NETWORK_TCP_SYN) != 0 ? 24 : 20;

    NetworkWrite16(Packet, Connection->LocalPort);
    NetworkWrite16(Packet + 2, Connection->Port);
    NetworkWrite32(Packet + 4, Sequence);
    NetworkWrite32(Packet + 8, (Flags & NETWORK_TCP_ACK) != 0 ? Connection->ReceiveNext : 0);
    Packet[12] = (HeaderLength / 4) << 4;
    Packet[13] = Flags;
    NetworkWrite16(Packet + 14, NETWORK_TCP_BUFFER_SIZE - Connection->Count);

    if (HeaderLength == 24)
    {
        Packet[20] = 2;
        Packet[21] = 4;
        NetworkWrite16(Packet + 22, NETWORK_TCP_MSS);
    }

    if (Length != 0)
    {
        KMemCpy(Packet + HeaderLength, Data, Length);
    }

    NetworkWrite16(Packet + 16, NetworkTransportChecksum(NetworkState.Config.Address, Connection->Address, 6, Packet, Length + HeaderLength));

    return NetworkSendIPv4Locked(Connection->Address, 6, Packet, Length + HeaderLength);
}

BOOLEAN NetworkTCPActiveLocked(VOID)
{
    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        if (Connections[Index].Active && (!Connections[Index].SocketAPI || Connections[Index].Connected || Connections[Index].Connecting || Connections[Index].SynReceived))
        {
            return TRUE;
        }
    }

    return FALSE;
}

VOID NetworkTCPInputLocked(UINT32 Address, CONST UINT8 *Packet, UINT32 Length)
{
    NETWORK_TCP_CONNECTION *Connection = NULL;
    UINT32                  HeaderLength;
    UINT32                  Sequence;
    UINT32                  Acknowledgment;
    UINT32                  DataLength;
    UINT8                   Flags;

    if (Length < 20 || NetworkTransportChecksum(Address, NetworkState.Config.Address, 6, Packet, Length) != 0)
    {
        NetworkState.Stats.Dropped++;

        return;
    }

    HeaderLength = (Packet[12] >> 4) * 4;

    if (HeaderLength < 20 || HeaderLength > Length || (Packet[12] & 15) != 0)
    {
        NetworkState.Stats.Dropped++;

        return;
    }

    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        NETWORK_TCP_CONNECTION *Entry = &Connections[Index];

        if (Entry->Active && Entry->Address == Address && Entry->Port == NetworkRead16(Packet) && Entry->LocalPort == NetworkRead16(Packet + 2))
        {
            Connection = Entry;
            break;
        }
    }

    if (Connection == NULL)
    {
        Connection = PassiveOpen(Address, Packet, Length);
        if (Connection != NULL)
        {
            return;
        }
        HandleTimeWait(Address, Packet, Length);

        return;
    }

    NetworkState.Stats.TCPReceived++;
    Sequence       = NetworkRead32(Packet + 4);
    Acknowledgment = NetworkRead32(Packet + 8);
    Flags          = Packet[13];
    DataLength     = Length - HeaderLength;

    if (Connection->SocketAPI && Connection->SynReceived)
    {
        if ((Flags & NETWORK_TCP_RST) != 0)
        {
            if (Sequence != Connection->ReceiveNext)
            {
                return;
            }
            ResetConnection(Connection);
            return;
        }
        if ((Flags & NETWORK_TCP_SYN) != 0 && Sequence + 1 == Connection->ReceiveNext)
        {
            SendSegment(Connection, Connection->SendNext - 1, NETWORK_TCP_SYN | NETWORK_TCP_ACK, NULL, 0);
            return;
        }
        if (Flags != NETWORK_TCP_ACK && Flags != (NETWORK_TCP_ACK | NETWORK_TCP_PSH))
        {
            return;
        }
        if (Sequence != Connection->ReceiveNext || Acknowledgment != Connection->SendNext)
        {
            return;
        }
        Connection->SynReceived        = FALSE;
        Connection->Connected          = TRUE;
        Connection->SendUnacknowledged = Acknowledgment;
        Connection->PeerWindow         = NetworkRead16(Packet + 14);
        Connection->WindowSequence     = Sequence;
        Connection->WindowACK          = Acknowledgment;
    }
    if (!Connection->Connected)
    {
        if ((Flags & NETWORK_TCP_ACK) == 0 || Acknowledgment != Connection->SendNext)
        {
            return;
        }

        if ((Flags & NETWORK_TCP_RST) != 0)
        {
            Connection->Reset = TRUE;
            Connection->Error = ECONNREFUSED;

            return;
        }

        if ((Flags & (NETWORK_TCP_SYN | NETWORK_TCP_FIN)) != NETWORK_TCP_SYN || DataLength != 0)
        {
            return;
        }

        Connection->PeerMSS = NETWORK_TCP_MSS;

        for (UINT32 Offset = 20; Offset < HeaderLength;)
        {
            UINT8 Kind = Packet[Offset];
            UINT8 OptionLength;

            if (Kind == 0)
            {
                break;
            }

            if (Kind == 1)
            {
                Offset++;
                continue;
            }

            if (Offset + 2 > HeaderLength || (OptionLength = Packet[Offset + 1]) < 2 || Offset + OptionLength > HeaderLength)
            {
                return;
            }

            if (Kind == 2)
            {
                if (OptionLength != 4 || NetworkRead16(Packet + Offset + 2) == 0)
                {
                    return;
                }

                Connection->PeerMSS = NetworkRead16(Packet + Offset + 2);

                if (Connection->PeerMSS > NETWORK_TCP_MSS)
                {
                    Connection->PeerMSS = NETWORK_TCP_MSS;
                }
            }

            Offset += OptionLength;
        }

        Connection->ReceiveNext        = Sequence + 1;
        Connection->SendUnacknowledged = Acknowledgment;
        Connection->PeerWindow         = NetworkRead16(Packet + 14);
        Connection->WindowSequence     = Sequence;
        Connection->WindowACK          = Acknowledgment;
        Connection->Connected          = TRUE;
        Connection->Connecting         = FALSE;
        SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);

        return;
    }

    if ((Flags & NETWORK_TCP_RST) != 0)
    {
        if (Sequence == Connection->ReceiveNext)
        {
            Connection->Reset = TRUE;
            Connection->Error = ECONNRESET;
        }
        else
        {
            SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);
        }

        return;
    }

    if ((Flags & NETWORK_TCP_SYN) != 0)
    {
        SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);

        return;
    }

    if ((Flags & NETWORK_TCP_ACK) == 0 || SequenceBefore(Connection->SendNext, Acknowledgment))
    {
        return;
    }

    if (SequenceBefore(Connection->ReceiveNext, Sequence) || (SequenceBefore(Sequence, Connection->ReceiveNext) && Connection->ReceiveNext - Sequence > DataLength + ((Flags & NETWORK_TCP_FIN) != 0)))
    {
        SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);

        return;
    }

    if (!SequenceBefore(Acknowledgment, Connection->SendUnacknowledged))
    {
        Connection->SendUnacknowledged = Acknowledgment;

        if (SequenceBefore(Connection->WindowSequence, Sequence) || (Connection->WindowSequence == Sequence && !SequenceBefore(Acknowledgment, Connection->WindowACK)))
        {
            Connection->PeerWindow     = NetworkRead16(Packet + 14);
            Connection->WindowSequence = Sequence;
            Connection->WindowACK      = Acknowledgment;
        }
    }

    if (DataLength != 0 || (Flags & NETWORK_TCP_FIN) != 0)
    {
        UINT32 Skip = Connection->ReceiveNext - Sequence;

        if (Skip < DataLength)
        {
            UINT32 NewLength = DataLength - Skip;

            if (Connection->PeerFIN || Connection->Count == NETWORK_TCP_BUFFER_SIZE)
            {
                SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);

                return;
            }

            if (NewLength > NETWORK_TCP_BUFFER_SIZE - Connection->Count)
            {
                NewLength = NETWORK_TCP_BUFFER_SIZE - Connection->Count;
            }

            for (UINT32 Index = 0; Index < NewLength; Index++)
            {
                Connection->Buffer[(Connection->Head + Connection->Count + Index) % NETWORK_TCP_BUFFER_SIZE] = Packet[HeaderLength + Skip + Index];
            }

            Connection->Count += Connection->ReadClosed ? 0 : NewLength;
            Connection->ReceiveNext += NewLength;
        }

        if ((Flags & NETWORK_TCP_FIN) != 0 && Sequence + DataLength == Connection->ReceiveNext)
        {
            Connection->ReceiveNext++;
            Connection->PeerFIN = TRUE;
        }

        SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);
    }
}

STATIC BOOLEAN SendReliable(NETWORK_TCP_CONNECTION *Connection, UINT8 SegmentFlags, CONST UINT8 *Data, UINT32 Length, UINT32 TimeoutMls)
{
    UINT64 Start    = NetworkNow();
    UINT64 Sent     = Start;
    UINT32 RetryMls = 1000;
    UINT32 Sequence;
    UINT32 End;
    UINT64 Flags;

    if (TimeoutMls == 0)
    {
        return NetworkError(L"TCP send deadline expired");
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

    if (!Connection->Active || Connection->Reset)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP connection reset");
    }

    Sequence             = Connection->SendNext;
    End                  = Sequence + Length + ((SegmentFlags & (NETWORK_TCP_SYN | NETWORK_TCP_FIN)) != 0);
    Connection->SendNext = End;

    if (!SendSegment(Connection, Sequence, SegmentFlags, Data, Length))
    {
        Connection->SendNext = Sequence;
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return FALSE;
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    do
    {
        NetworkWait();
        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

        if (!Connection->Active || Connection->Reset)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

            return NetworkError(L"TCP connection reset");
        }

        if (Connection->SendUnacknowledged == End)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

            return TRUE;
        }

        if (NetworkNow() - Sent >= RetryMls)
        {
            UINT32 Skip = 0;

            if (Length != 0 && SequenceBefore(Sequence, Connection->SendUnacknowledged))
            {
                Skip = Connection->SendUnacknowledged - Sequence;
                Skip = Skip > Length ? Length : Skip;
            }

            UINT32 RetryLength = Length - Skip;

            if (Connection->PeerWindow == 0 && RetryLength != 0)
            {
                RetryLength = 1;
            }

            SendSegment(Connection, Sequence + Skip, SegmentFlags, Data == NULL ? NULL : Data + Skip, RetryLength);
            NetworkState.Stats.Retransmitted++;
            Sent     = NetworkNow();
            RetryMls = RetryMls < 8000 ? RetryMls * 2 : 8000;
        }

        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    } while (!NetworkExpired(Start, TimeoutMls));

    return NetworkError(L"TCP acknowledgment timeout");
}

STATIC VOID AbortConnectionLocked(NETWORK_TCP_CONNECTION *Connection)
{
    if (Connection->Active && Connection->Connected && !Connection->Reset)
    {
        SendSegment(Connection, Connection->SendNext, NETWORK_TCP_RST | NETWORK_TCP_ACK, NULL, 0);
    }

    ResetConnection(Connection);
}

STATIC VOID AbortConnection(NETWORK_TCP_CONNECTION *Connection)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

    AbortConnectionLocked(Connection);
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
}

STATIC BOOLEAN Connect(NETWORK_TCP_CONNECTION *Connection, UINT32 TimeoutMls)
{
    UINT64 Start = NetworkNow();

    if (!NetworkResolve(Connection->Address, TimeoutMls) || !SendReliable(Connection, NETWORK_TCP_SYN, NULL, 0, NetworkRemaining(Start, TimeoutMls)))
    {
        AbortConnection(Connection);

        return FALSE;
    }

    NetworkError(L"ok");

    return TRUE;
}

STATIC BOOLEAN Send(NETWORK_TCP_CONNECTION *Connection, CONST VOID *Buffer, UINT32 Length, UINT32 TimeoutMls)
{
    CONST UINT8 *Data     = Buffer;
    UINT32       Offset   = 0;
    UINT64       ProbeAt  = 0;
    UINT32       ProbeMls = 1000;
    UINT64       Start    = NetworkNow();

    if ((Length != 0 && Buffer == NULL) || TimeoutMls == 0)
    {
        return NetworkError(L"invalid TCP send arguments");
    }

    {
        UINT64  Flags    = SpinLockAcquireIRQSave(&NetworkState.Lock);
        BOOLEAN Writable = Connection->Active && Connection->Connected && !Connection->Reset && !Connection->FINSent;

        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        if (!Writable)
        {
            return NetworkError(L"TCP connection not writable");
        }
    }

    while (Offset < Length)
    {
        UINT64 Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
        UINT32 Count;

        if (!Connection->Active || !Connection->Connected || Connection->Reset || Connection->FINSent)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

            return NetworkError(L"TCP connection not writable");
        }

        Count = Length - Offset;

        if (Count > Connection->PeerMSS)
        {
            Count = Connection->PeerMSS;
        }

        if (Count > Connection->PeerWindow)
        {
            Count = Connection->PeerWindow;
        }

        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        if (NetworkExpired(Start, TimeoutMls))
        {
            AbortConnection(Connection);

            return NetworkError(L"TCP send window or total timeout");
        }

        if (Count == 0)
        {
            if (ProbeAt == 0)
            {
                ProbeAt = NetworkNow() + ProbeMls;
            }
            else if (NetworkNow() >= ProbeAt)
            {
                Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

                if (!Connection->Active || Connection->Reset)
                {
                    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

                    return NetworkError(L"TCP connection reset during probe");
                }

                SendSegment(Connection, Connection->SendNext - 1, NETWORK_TCP_ACK, &Connection->LastByte, 1);
                NetworkState.Stats.Retransmitted++;
                SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
                ProbeMls = ProbeMls < 8000 ? ProbeMls * 2 : 8000;
                ProbeAt  = NetworkNow() + ProbeMls;
            }

            NetworkWait();
            continue;
        }

        UINT64 Elapsed = NetworkNow() - Start;

        if (Elapsed >= TimeoutMls || !SendReliable(Connection, NETWORK_TCP_ACK | NETWORK_TCP_PSH, Data + Offset, Count, TimeoutMls - (UINT32) Elapsed))
        {
            AbortConnection(Connection);

            return FALSE;
        }

        Flags                = SpinLockAcquireIRQSave(&NetworkState.Lock);
        Connection->LastByte = Data[Offset + Count - 1];
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        ProbeAt  = 0;
        ProbeMls = 1000;
        Offset += Count;
    }

    NetworkError(L"ok");

    return TRUE;
}

STATIC BOOLEAN Receive(NETWORK_TCP_CONNECTION *Connection, VOID *Buffer, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls)
{
    UINT64 Start = NetworkNow();

    if (Buffer == NULL || Capacity == 0 || Length == NULL || TimeoutMls == 0)
    {
        return NetworkError(L"invalid TCP receive arguments");
    }

    *Length = 0;

    do
    {
        UINT64 Flags;

        NetworkWait();
        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

        if (!Connection->Active || !Connection->Connected || Connection->Reset)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

            return NetworkError(L"TCP connection closed or reset");
        }

        if (Connection->Count != 0)
        {
            UINT32 Count = Connection->Count < Capacity ? Connection->Count : Capacity;

            for (UINT32 Index = 0; Index < Count; Index++)
            {
                ((UINT8 *) Buffer)[Index] = Connection->Buffer[(Connection->Head + Index) % NETWORK_TCP_BUFFER_SIZE];
            }

            Connection->Head = (Connection->Head + Count) % NETWORK_TCP_BUFFER_SIZE;
            Connection->Count -= Count;
            *Length = Count;
            SendSegment(Connection, Connection->SendNext, NETWORK_TCP_ACK, NULL, 0);
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            NetworkError(L"ok");

            return TRUE;
        }

        if (Connection->PeerFIN)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            NetworkError(L"ok");

            return TRUE;
        }

        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    } while (!NetworkExpired(Start, TimeoutMls));

    return NetworkError(L"TCP receive timeout");
}

STATIC BOOLEAN Close(NETWORK_TCP_CONNECTION *Connection, UINT32 TimeoutMls)
{
    UINT64 Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    UINT64 Start = NetworkNow();

    if (!Connection->Active)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return TRUE;
    }

    if (!Connection->Active || !Connection->Connected || Connection->Reset || TimeoutMls == 0)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
        AbortConnection(Connection);

        return NetworkError(L"TCP close unavailable");
    }

    if (Connection->Count != 0)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP unread data must be drained before close");
    }

    BOOLEAN SendFIN = !Connection->FINSent;

    Connection->FINSent = TRUE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    if (SendFIN && !SendReliable(Connection, NETWORK_TCP_ACK | NETWORK_TCP_FIN, NULL, 0, TimeoutMls))
    {
        AbortConnection(Connection);

        return FALSE;
    }

    do
    {
        BOOLEAN Closed;

        NetworkWait();
        Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

        if (!Connection->Active || Connection->Reset)
        {
            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
            AbortConnection(Connection);

            return NetworkError(L"TCP connection reset during close");
        }

        Closed = Connection->PeerFIN;
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        if (Closed)
        {
            Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

            if (Connection->Count != 0)
            {
                SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

                return NetworkError(L"TCP received data during close; drain it and close again");
            }

            if (!Connection->Active || Connection->Reset)
            {
                SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

                return NetworkError(L"TCP connection reset during close");
            }

            BOOLEAN Result = FinishClose(Connection);

            SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

            if (Result)
            {
                NetworkError(L"ok");
            }

            return Result;
        }
    } while (!NetworkExpired(Start, TimeoutMls));

    AbortConnection(Connection);

    return NetworkError(L"TCP FIN timeout");
}

BOOLEAN NetworkTCPConnect(UINT32 Address, UINT16 Port, UINT32 *Socket, UINT32 TimeoutMls)
{
    NETWORK_TCP_CONNECTION *Connection = NULL;
    UINT32                  Index;
    UINT32                  Reserved = 0;
    UINT64                  Flags;

    if (Socket == NULL)
    {
        return NetworkError(L"TCP handle output invalid");
    }

    *Socket = UINT32_MAX;

    if (Port == 0 || TimeoutMls == 0)
    {
        return NetworkError(L"invalid TCP destination or timeout");
    }

    Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);

    if (!NetworkState.Ready || !NetworkState.Configured)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP network unavailable or unconfigured");
    }

    for (Index = 0; Index < NETWORK_TCP_TIME_WAIT_COUNT; Index++)
    {
        Reserved += TimeWait[Index].Expires > NetworkNow();
    }

    for (Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        Reserved += Connections[Index].Active;
    }

    if (Reserved >= NETWORK_TCP_TIME_WAIT_COUNT)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP TIME_WAIT capacity exhausted");
    }

    for (Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        NETWORK_TCP_CONNECTION *Entry = &Connections[Index];

        if (!Entry->Active && !Entry->Writing && !Entry->Reading && !Entry->Closing)
        {
            UINT32 Generation = (Entry->Generation + 1) & 0xFFFFFF;

            KMemSet(Entry, 0, sizeof(*Entry));
            Entry->Generation = Generation == 0 ? 1 : Generation;
            Connection        = Entry;
            break;
        }
    }

    if (Connection == NULL)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP socket table full");
    }

    Connection->Active             = TRUE;
    Connection->Writing            = TRUE;
    Connection->Address            = Address;
    Connection->Port               = Port;
    Connection->LocalPort          = NetworkAllocatePortLocked();
    Connection->SendNext           = (UINT32) RDTSC();
    Connection->SendUnacknowledged = Connection->SendNext;

    if (Connection->LocalPort == 0)
    {
        ResetConnection(Connection);
        Connection->Writing = FALSE;
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP ports exhausted");
    }

    UINT32 Handle = (Connection->Generation << 8) | Index;

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    BOOLEAN Result = Connect(Connection, TimeoutMls);

    Flags               = SpinLockAcquireIRQSave(&NetworkState.Lock);
    Result              = Result && Connection->Active && !Connection->Reset;
    Connection->Writing = FALSE;

    if (Result)
    {
        *Socket = Handle;
    }

    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    return Result;
}

BOOLEAN NetworkTCPSend(UINT32 Socket, CONST VOID *Buffer, UINT32 Length, UINT32 TimeoutMls)
{
    UINT64                  Flags      = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Connection = FindConnection(Socket);

    if (Connection == NULL || Connection->Writing || Connection->Closing)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP handle invalid or writer busy");
    }

    Connection->Writing = TRUE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    BOOLEAN Result = Send(Connection, Buffer, Length, TimeoutMls);

    Flags               = SpinLockAcquireIRQSave(&NetworkState.Lock);
    Connection->Writing = FALSE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    return Result;
}

BOOLEAN NetworkTCPReceive(UINT32 Socket, VOID *Buffer, UINT32 Capacity, UINT32 *Length, UINT32 TimeoutMls)
{
    if (Length != NULL)
    {
        *Length = 0;
    }

    UINT64                  Flags      = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Connection = FindConnection(Socket);

    if (Connection == NULL || Connection->Reading || Connection->Closing || !Connection->Connected)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP handle invalid or reader busy");
    }

    Connection->Reading = TRUE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    BOOLEAN Result = Receive(Connection, Buffer, Capacity, Length, TimeoutMls);

    Flags               = SpinLockAcquireIRQSave(&NetworkState.Lock);
    Connection->Reading = FALSE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    return Result;
}

BOOLEAN NetworkTCPClose(UINT32 Socket, UINT32 TimeoutMls)
{
    UINT64                  Flags      = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Connection = FindConnection(Socket);

    if (Connection == NULL || Connection->Closing || Connection->Writing || Connection->Reading)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP handle invalid or operations busy");
    }

    Connection->Closing = TRUE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    BOOLEAN Result = Close(Connection, TimeoutMls);

    Flags               = SpinLockAcquireIRQSave(&NetworkState.Lock);
    Connection->Closing = FALSE;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    return Result;
}

BOOLEAN NetworkTCPAbort(UINT32 Socket)
{
    UINT64                  Flags      = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Connection = FindConnection(Socket);

    if (Connection == NULL)
    {
        SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

        return NetworkError(L"TCP handle invalid");
    }

    AbortConnectionLocked(Connection);
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);

    return TRUE;
}

STATIC NETWORK_TCP_CONNECTION *SocketAllocateLocked(UINT32 *Handle)
{
    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        NETWORK_TCP_CONNECTION *Entry = &Connections[Index];
        if (!Entry->Active && !Entry->Writing && !Entry->Reading && !Entry->Closing)
        {
            UINT32 Generation = (Entry->Generation + 1) & 0xFFFFFFU;
            KMemSet(Entry, 0, sizeof(*Entry));
            Entry->Generation = Generation == 0 ? 1 : Generation;
            Entry->Active     = TRUE;
            Entry->SocketAPI  = TRUE;
            Entry->PeerMSS    = NETWORK_TCP_MSS;
            Entry->Parent     = UINT32_MAX;
            Entry->RetryMls   = 1000;
            *Handle           = (Entry->Generation << 8) | Index;
            return Entry;
        }
    }
    return NULL;
}

STATIC NETWORK_TCP_CONNECTION *PassiveOpen(UINT32 Address, CONST UINT8 *Packet, UINT32 Length)
{
    if (Packet[13] != NETWORK_TCP_SYN || Length != (UINT32) (Packet[12] >> 4) * 4)
    {
        return NULL;
    }
    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        NETWORK_TCP_CONNECTION *Listener = &Connections[Index];
        if (!Listener->Active || !Listener->Listening || Listener->LocalPort != NetworkRead16(Packet + 2))
        {
            continue;
        }
        UINT32 Parent  = (Listener->Generation << 8) | Index;
        UINT32 Pending = 0;
        for (UINT32 Other = 0; Other < NETWORK_TCP_SOCKET_COUNT; Other++)
        {
            Pending += Connections[Other].Active && Connections[Other].PendingAccept && Connections[Other].Parent == Parent;
        }
        if (Pending >= Listener->Backlog)
        {
            return NULL;
        }
        UINT32                  Handle;
        NETWORK_TCP_CONNECTION *Child = SocketAllocateLocked(&Handle);
        if (Child == NULL)
        {
            return NULL;
        }
        Child->Address            = Address;
        Child->Port               = NetworkRead16(Packet);
        Child->LocalPort          = Listener->LocalPort;
        Child->Parent             = Parent;
        Child->PendingAccept      = TRUE;
        Child->SynReceived        = TRUE;
        Child->Started            = NetworkNow();
        Child->ReceiveNext        = NetworkRead32(Packet + 4) + 1;
        Child->SendUnacknowledged = (UINT32) RDTSC();
        Child->SendNext           = Child->SendUnacknowledged + 1;
        Child->PeerWindow         = NetworkRead16(Packet + 14);
        for (UINT32 Offset = 20; Offset < Length;)
        {
            UINT8 Kind = Packet[Offset];
            if (Kind == 0)
            {
                break;
            }
            if (Kind == 1)
            {
                Offset++;
                continue;
            }
            if (Offset + 2 > Length || Packet[Offset + 1] < 2 || Offset + Packet[Offset + 1] > Length)
            {
                ResetConnection(Child);
                return NULL;
            }
            if (Kind == 2)
            {
                if (Packet[Offset + 1] != 4 || NetworkRead16(Packet + Offset + 2) == 0)
                {
                    ResetConnection(Child);
                    return NULL;
                }
                UINT16 MSS     = NetworkRead16(Packet + Offset + 2);
                Child->PeerMSS = MSS < NETWORK_TCP_MSS ? MSS : NETWORK_TCP_MSS;
            }
            Offset += Packet[Offset + 1];
        }
        SendSegment(Child, Child->SendUnacknowledged, NETWORK_TCP_SYN | NETWORK_TCP_ACK, NULL, 0);
        Child->SentAt = NetworkNow();
        return Child;
    }
    return NULL;
}

VOID NetworkTCPTickLocked(VOID)
{
    SocketTickLocked();
}

STATIC VOID SocketTickLocked(VOID)
{
    UINT64 Now = NetworkNow();
    for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
    {
        NETWORK_TCP_CONNECTION *Entry = &Connections[Index];
        if (!Entry->Active || !Entry->SocketAPI || Entry->Listening)
        {
            continue;
        }
        if ((Entry->Connecting || Entry->SynReceived || Entry->TxLength != 0 || Entry->Detached || Entry->FINSent) && Now - Entry->Started >= 30000)
        {
            Entry->Reset = TRUE;
            Entry->Error = ETIMEDOUT;
        }
        if (Entry->Reset)
        {
            if (Entry->PendingAccept || Entry->Detached)
            {
                AbortConnectionLocked(Entry);
            }
            continue;
        }
        if (Entry->TxLength != 0 && !SequenceBefore(Entry->SendUnacknowledged, Entry->TxSequence + Entry->TxLength))
        {
            Entry->TxLength = 0;
        }
        if (Entry->Connected && Entry->WriteClosed && Entry->TxLength == 0 && !Entry->FINSent)
        {
            Entry->FINSent = TRUE;
            Entry->Started = Now;
            Entry->SendNext++;
            Entry->SentAt = Now - Entry->RetryMls;
        }
        if (Entry->Detached && Entry->PeerFIN && Entry->FINSent && Entry->SendUnacknowledged == Entry->SendNext)
        {
            FinishClose(Entry);
            continue;
        }
        if (Now - Entry->SentAt < Entry->RetryMls)
        {
            continue;
        }
        if (Entry->Connecting || Entry->SynReceived)
        {
            if (!NetworkResolveReadyLocked(Entry->Address))
            {
                continue;
            }
            {
                SendSegment(Entry, Entry->SendNext - 1, NETWORK_TCP_SYN | (Entry->SynReceived ? NETWORK_TCP_ACK : 0), NULL, 0);
            }
        }
        else if (Entry->TxLength != 0)
        {
            UINT32 Skip = SequenceBefore(Entry->TxSequence, Entry->SendUnacknowledged) ? Entry->SendUnacknowledged - Entry->TxSequence : 0;
            SendSegment(Entry, Entry->TxSequence + Skip, NETWORK_TCP_ACK | NETWORK_TCP_PSH, Entry->TxData + Skip, Entry->TxLength - Skip);
        }
        else if (Entry->FINSent && Entry->SendUnacknowledged != Entry->SendNext)
        {
            SendSegment(Entry, Entry->SendNext - 1, NETWORK_TCP_FIN | NETWORK_TCP_ACK, NULL, 0);
        }
        else
        {
            continue;
        }
        Entry->SentAt   = Now;
        Entry->RetryMls = Entry->RetryMls < 8000 ? Entry->RetryMls * 2 : 8000;
        NetworkState.Stats.Retransmitted++;
    }
}

INT64 NetworkTCPOpenSocket(UINT32 *Socket)
{
    UINT64 Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    INT64  Result = SocketAllocateLocked(Socket) == NULL ? -ENOBUFS : 0;
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPBind(UINT32 Socket, UINT32 Address, UINT16 Port, BOOLEAN Reuse)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = 0;
    if (Entry == NULL || Entry->LocalPort != 0)
    {
        Result = -EINVAL;
    }
    else if (Address != 0 && (!NetworkState.Configured || Address != NetworkState.Config.Address))
    {
        Result = -EADDRNOTAVAIL;
    }
    else
    {
        UINT16 Selected = Port == 0 ? NetworkAllocatePortLocked() : Port;
        if (Selected == 0)
        {
            Result = -EADDRINUSE;
        }
        for (UINT32 Index = 0; Result == 0 && Index < NETWORK_TCP_SOCKET_COUNT; Index++)
        {
            if (Connections[Index].Active && &Connections[Index] != Entry && Connections[Index].LocalPort == Selected)
            {
                Result = -EADDRINUSE;
            }
        }
        if (Result == 0 && !Reuse && NetworkTCPPortInUseLocked(Selected))
        {
            Result = -EADDRINUSE;
        }
        if (Result == 0)
        {
            Entry->LocalPort    = Selected;
            Entry->BoundAddress = Address;
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPListen(UINT32 Socket, UINT32 Backlog)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = 0;
    if (Entry == NULL || Entry->Connecting || Entry->Connected)
    {
        Result = -EINVAL;
    }
    else
    {
        if (Entry->LocalPort == 0)
        {
            Entry->LocalPort = NetworkAllocatePortLocked();
        }
        if (Entry->LocalPort == 0)
        {
            Result = -EADDRINUSE;
        }
        else
        {
            Entry->Listening = TRUE;
            Entry->Backlog   = Backlog == 0 ? 1 : Backlog > NETWORK_TCP_SOCKET_COUNT ? NETWORK_TCP_SOCKET_COUNT : Backlog;
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPAccept(UINT32 Socket, UINT32 *Child)
{
    UINT64                  Flags    = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Listener = FindConnection(Socket);
    INT64                   Result   = Listener == NULL || !Listener->Listening ? -EINVAL : -EAGAIN;
    if (Result == -EAGAIN)
    {
        for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
        {
            NETWORK_TCP_CONNECTION *Entry = &Connections[Index];
            if (Entry->Active && Entry->Connected && !Entry->Reset && Entry->PendingAccept && Entry->Parent == Socket)
            {
                Entry->PendingAccept = FALSE;
                Entry->Parent        = UINT32_MAX;
                *Child               = (Entry->Generation << 8) | Index;
                Result               = 0;
                break;
            }
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPStartConnect(UINT32 Socket, UINT32 Address, UINT16 Port)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = 0;
    if (!NetworkState.Ready || !NetworkState.Configured)
    {
        Result = -ENETUNREACH;
    }
    else if (Entry == NULL || Entry->Listening || Port == 0 || Address == 0 || Address == UINT32_MAX || Address == NetworkState.Config.Address || (Address >> 24) == 127)
    {
        Result = -EINVAL;
    }
    else if (Entry->Connected || Entry->Connecting)
    {
        Result = Entry->Connected ? -EISCONN : -EALREADY;
    }
    else
    {
        if (Entry->LocalPort == 0)
        {
            Entry->LocalPort = NetworkAllocatePortLocked();
        }
        if (Entry->LocalPort == 0)
        {
            Result = -EADDRINUSE;
        }
        else
        {
            Entry->Address            = Address;
            Entry->Port               = Port;
            Entry->Connecting         = TRUE;
            Entry->Started            = NetworkNow();
            Entry->SentAt             = Entry->Started;
            Entry->SendUnacknowledged = (UINT32) RDTSC();
            Entry->SendNext           = Entry->SendUnacknowledged + 1;
            if (NetworkResolveReadyLocked(Address))
            {
                SendSegment(Entry, Entry->SendUnacknowledged, NETWORK_TCP_SYN, NULL, 0);
            }
            else
            {
                Entry->SentAt -= Entry->RetryMls;
            }
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPReadSocket(UINT32 Socket, VOID *Buffer, UINT32 Length, BOOLEAN Peek)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = -ENOTCONN;
    if (Entry != NULL && Entry->Connected)
    {
        UINT32 Count = Entry->Count < Length ? Entry->Count : Length;
        for (UINT32 Index = 0; Index < Count; Index++)
        {
            ((UINT8 *) Buffer)[Index] = Entry->Buffer[(Entry->Head + Index) % NETWORK_TCP_BUFFER_SIZE];
        }
        if (!Peek && Count != 0)
        {
            Entry->Count -= Count;
            Entry->Head = (Entry->Head + Count) % NETWORK_TCP_BUFFER_SIZE;
            SendSegment(Entry, Entry->SendNext, NETWORK_TCP_ACK, NULL, 0);
        }
        Result = Count != 0 || Length == 0 || Entry->PeerFIN || Entry->ReadClosed ? (INT64) Count : Entry->Reset ? -ECONNRESET : -EAGAIN;
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPWriteSocket(UINT32 Socket, CONST VOID *Buffer, UINT32 Length)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = -ENOTCONN;
    if (Entry != NULL && Entry->Connected)
    {
        if (Entry->Reset || Entry->WriteClosed)
        {
            Result = -EPIPE;
        }
        else if (Length == 0)
        {
            Result = 0;
        }
        else if (Entry->TxLength != 0 || Entry->PeerWindow == 0)
        {
            Result = -EAGAIN;
        }
        else
        {
            UINT32 Count = Length < Entry->PeerMSS ? Length : Entry->PeerMSS;
            Count        = Count < Entry->PeerWindow ? Count : Entry->PeerWindow;
            KMemCpy(Entry->TxData, Buffer, Count);
            Entry->TxLength   = Count;
            Entry->TxSequence = Entry->SendNext;
            Entry->SendNext += Count;
            Entry->Started  = NetworkNow();
            Entry->SentAt   = Entry->Started;
            Entry->RetryMls = 1000;
            SendSegment(Entry, Entry->TxSequence, NETWORK_TCP_ACK | NETWORK_TCP_PSH, Entry->TxData, Count);
            Result = Count;
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

INT64 NetworkTCPShutdown(UINT32 Socket, UINT32 How)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = Entry == NULL || !Entry->Connected ? -ENOTCONN : 0;
    if (Result == 0)
    {
        if (How != 1)
        {
            Entry->ReadClosed = TRUE;
            Entry->Count      = 0;
        }
        if (How != 0)
        {
            Entry->WriteClosed = TRUE;
        }
        SocketTickLocked();
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}

VOID NetworkTCPReleaseSocket(UINT32 Socket)
{
    UINT64                  Flags = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry = FindConnection(Socket);
    if (Entry != NULL)
    {
        if (Entry->Listening)
        {
            for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
            {
                if (Connections[Index].Active && Connections[Index].PendingAccept && Connections[Index].Parent == Socket)
                {
                    AbortConnectionLocked(&Connections[Index]);
                }
            }
        }
        if (!Entry->Connected || Entry->Reset)
        {
            AbortConnectionLocked(Entry);
        }
        else
        {
            Entry->Detached    = TRUE;
            Entry->WriteClosed = TRUE;
            Entry->Started     = NetworkNow();
            SocketTickLocked();
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
}

INT64 NetworkTCPGetSocketState(UINT32 Socket, NETWORK_TCP_SOCKET_STATE *State, BOOLEAN ClearError)
{
    UINT64                  Flags  = SpinLockAcquireIRQSave(&NetworkState.Lock);
    NETWORK_TCP_CONNECTION *Entry  = FindConnection(Socket);
    INT64                   Result = Entry == NULL ? -EBADF : 0;
    if (Entry != NULL)
    {
        KMemSet(State, 0, sizeof(*State));
        State->Address      = Entry->Address;
        State->Port         = Entry->Port;
        State->LocalPort    = Entry->LocalPort;
        State->LocalAddress = Entry->Connected || Entry->Connecting ? NetworkState.Config.Address : Entry->BoundAddress;
        State->Connected    = Entry->Connected;
        State->Connecting   = Entry->Connecting;
        State->Listening    = Entry->Listening;
        State->Error        = Entry->Error;
        State->Available    = Entry->Count;
        State->Events       = Entry->Reset ? 0x10 | (Entry->Error != 0 ? 8 : 0) : 0;
        if (Entry->Listening)
        {
            for (UINT32 Index = 0; Index < NETWORK_TCP_SOCKET_COUNT; Index++)
            {
                State->Events |= Connections[Index].Active && Connections[Index].Connected && !Connections[Index].Reset && Connections[Index].PendingAccept && Connections[Index].Parent == Socket ? 1 : 0;
            }
        }
        else if (Entry->Connected)
        {
            State->Events |= Entry->Count != 0 || Entry->PeerFIN || Entry->ReadClosed ? 1 : 0;
            State->Events |= !Entry->Reset && !Entry->WriteClosed && Entry->TxLength == 0 && Entry->PeerWindow != 0 ? 4 : 0;
            State->Events |= Entry->PeerFIN ? 0x2000 : 0;
        }
        if (ClearError)
        {
            Entry->Error = 0;
        }
    }
    SpinLockReleaseIRQRestore(&NetworkState.Lock, Flags);
    return Result;
}
