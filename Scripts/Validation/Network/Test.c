// Scripts/Validation/Network/Test.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#define main LinuxFileTestsMain
#include "../LinuxSyscalls/Test.c"
#undef main
#include <Network/NetworkInternal.h>
#include <Syscall/Network/SyscallSocket.h>

EXTERN UINT64 NetworkTestTime;
EXTERN UINT8  NetworkTestPacket[1500];
EXTERN UINT32 NetworkTestPackets;

STATIC VOID Packet(UINT16 Local, UINT16 Remote, UINT32 Sequence, UINT32 ACK, UINT8 Flags, CONST char *Data)
{
    UINT8  Bytes[1500] = {0};
    UINT32 Length      = Data == NULL ? 0 : (UINT32) strlen(Data);
    NetworkWrite16(Bytes, Remote);
    NetworkWrite16(Bytes + 2, Local);
    NetworkWrite32(Bytes + 4, Sequence);
    NetworkWrite32(Bytes + 8, ACK);
    Bytes[12] = 0x50;
    Bytes[13] = Flags;
    NetworkWrite16(Bytes + 14, 8192);
    if (Length != 0)
        memcpy(Bytes + 20, Data, Length);
    NetworkWrite16(Bytes + 16, NetworkTransportChecksum(NETWORK_IPV4(10, 0, 2, 2), NetworkState.Config.Address, 6, Bytes, 20 + Length));
    NetworkTCPInputLocked(NETWORK_IPV4(10, 0, 2, 2), Bytes, 20 + Length);
    NetworkTCPTickLocked();
}

STATIC INT64 Socket(UINT32 Flags)
{
    return SyscallSocket(&(SYSCALL_CONTEXT) {
        .Args = {.RDI = 2, .RSI = 1 | Flags}
    });
}

STATIC VOID CloseFD(INT64 FD)
{
    assert(SyscallClose(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) FD}) == 0);
}

STATIC INT32 Option(INT64 FD, INT32 Name)
{
    INT32  Value  = -1;
    UINT32 Length = sizeof(Value);
    assert(SyscallSocketOption(&(SYSCALL_CONTEXT) {
               .Number = SYSCALL_GETSOCKOPT, .Args = {.RDI = FD, .RSI = 1, .RDX = Name, .R10 = (UINT64) &Value, .R8 = (UINT64) &Length}
    }) == 0);
    return Value;
}

INT32 main(VOID)
{
    assert(LinuxFileTestsMain() == 0);
    SyscallFilesRelease(&Task);
    NetworkState.Ready          = TRUE;
    NetworkState.Configured     = TRUE;
    NetworkState.Config.Address = NETWORK_IPV4(10, 0, 2, 15);
    assert(SyscallSocket(&(SYSCALL_CONTEXT) {.Args.RDI = 1, .Args.RSI = 1}) == -EAFNOSUPPORT);
    assert(SyscallSocket(&(SYSCALL_CONTEXT) {.Args.RDI = 2, .Args.RSI = 2}) == -ESOCKTNOSUPPORT);
    INT64 Listener = Socket(0x80800);
    assert(Listener >= 0);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = Listener, .Args.RSI = 1}) == 1);
    struct
    {
        UINT16 Family;
        UINT8  Port[2];
        UINT8  Address[4];
        UINT8  Padding[8];
    } Address = {.Family = 2};
    NetworkWrite16(Address.Port, 22);
    assert(SyscallBind(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Listener, .RSI = (UINT64) &Address, .RDX = 16}
    }) == -EACCES);
    NetworkWrite16(Address.Port, 8022);
    assert(SyscallBind(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Listener, .RSI = (UINT64) &Address, .RDX = 15}
    }) == -EINVAL);
    assert(SyscallBind(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Listener, .RSI = 1, .RDX = 16}
    }) == -EFAULT);
    assert(SyscallBind(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Listener, .RSI = (UINT64) &Address, .RDX = 16}
    }) == 0);
    assert(SyscallListen(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Listener, .RSI = 1}
    }) == 0);
    assert(Option(Listener, 30) == 1);
    assert(SyscallAccept(&(SYSCALL_CONTEXT) {.Args.RDI = Listener}) == -EAGAIN);
    INT64 Other = Socket(0);
    assert(SyscallBind(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Other, .RSI = (UINT64) &Address, .RDX = 16}
    }) == -EADDRINUSE);
    CloseFD(Other);
    Packet(8022, 40000, 1000, 0, 2, NULL);
    assert(NetworkTestPacket[13] == 0x12 && NetworkRead32(NetworkTestPacket + 8) == 1001);
    UINT32 Server = NetworkRead32(NetworkTestPacket + 4);
    UINT32 Sent   = NetworkTestPackets;
    Packet(8022, 40001, 2000, 0, 2, NULL);
    assert(NetworkTestPackets == Sent);
    Packet(8022, 40000, 1002, Server + 1, 16, NULL);
    assert(SyscallAccept(&(SYSCALL_CONTEXT) {.Args.RDI = Listener}) == -EAGAIN);
    Packet(8022, 40000, 1001, Server + 1, 16, "abc");
    struct
    {
        INT32 FD;
        INT16 Events;
        INT16 Returned;
    } Poll = {Listener, 1, 0};
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) &Poll, .Args.RSI = 1}) == 1 && Poll.Returned == 1);
    UINT32 AddressLength = 4;
    INT64  Child         = SyscallAccept(&(SYSCALL_CONTEXT) {
                 .Number = SYSCALL_ACCEPT4, .Args = {.RDI = Listener, .RSI = (UINT64) &Address, .RDX = (UINT64) &AddressLength, .R10 = 0x80800}
    });
    assert(Child >= 0 && AddressLength == 16 && NetworkRead16(Address.Port) == 40000);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = Child, .Args.RSI = 1}) == 1);
    STAT_INFO Info;
    assert(SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = Child, .Args.RSI = (UINT64) &Info}) == 0 && (Info.Mode & 0xF000) == 0xC000);
    char Bytes[1024] = {0};
    assert(SyscallRead(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 3}
    }) == 3 &&
           memcmp(Bytes, "abc", 3) == 0);
    assert(SyscallRead(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 3}
    }) == -EAGAIN);
    Packet(8022, 40000, 1004, Server + 1, 24, "def");
    assert(SyscallRecvFrom(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 3, .R10 = 2}
    }) == 3);
    assert(SyscallRead(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 3}
    }) == 3 &&
           memcmp(Bytes, "def", 3) == 0);
    memset(Bytes, 'X', sizeof(Bytes));
    INT64 Count = SyscallFileWrite(&(SYSCALL_CONTEXT) {
        .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = sizeof(Bytes)}
    });
    assert(Count == 536);
    assert(SyscallFileWrite(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 1}
    }) == -EAGAIN);
    NetworkTestTime += 1001;
    NetworkPoll();
    assert(NetworkTestPacket[13] == 24 && NetworkRead32(NetworkTestPacket + 4) == Server + 1);
    Packet(8022, 40000, 1007, Server + 1 + (UINT32) Count, 16, NULL);
    Poll.FD     = Child;
    Poll.Events = 4;
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) &Poll, .Args.RSI = 1}) == 1 && Poll.Returned == 4);
    INT64 Duplicate = SyscallFcntl(&(SYSCALL_CONTEXT) {
        .Args = {.RDI = Child, .RSI = 0}
    });
    assert(Duplicate >= 0);
    CloseFD(Child);
    Child = Duplicate;
    assert(SyscallShutdown(&(SYSCALL_CONTEXT) {.Args.RDI = Child, .Args.RSI = 1}) == 0);
    assert(NetworkTestPacket[13] == 17);
    assert(SyscallFileWrite(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 1}
    }) == -EPIPE);
    Packet(8022, 40000, 1007, Server + (UINT32) Count + 2, 17, NULL);
    assert(SyscallRead(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Child, .RSI = (UINT64) Bytes, .RDX = 3}
    }) == 0);
    CloseFD(Child);
    CloseFD(Listener);
    INT64 Client = Socket(0x800);
    NetworkWrite16(Address.Port, 9000);
    NetworkWrite32(Address.Address, NETWORK_IPV4(10, 0, 2, 2));
    assert(SyscallConnect(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Client, .RSI = (UINT64) &Address, .RDX = 16}
    }) == -EINPROGRESS);
    UINT16 Local    = NetworkRead16(NetworkTestPacket);
    UINT32 Sequence = NetworkRead32(NetworkTestPacket + 4);
    Packet(Local, 9000, 5000, Sequence + 1, 0x12, NULL);
    assert(Option(Client, 4) == 0);
    Poll.FD     = Client;
    Poll.Events = 4;
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) &Poll, .Args.RSI = 1}) == 1 && Poll.Returned == 4);
    CloseFD(Client);
    Client = Socket(0x800);
    assert(SyscallConnect(&(SYSCALL_CONTEXT) {
               .Args = {.RDI = Client, .RSI = (UINT64) &Address, .RDX = 16}
    }) == -EINPROGRESS);
    Local    = NetworkRead16(NetworkTestPacket);
    Sequence = NetworkRead32(NetworkTestPacket + 4);
    Packet(Local, 9000, 0, Sequence + 1, 0x14, NULL);
    assert(Option(Client, 4) == ECONNREFUSED && Option(Client, 4) == 0);
    CloseFD(Client);
    SyscallFilesRelease(&Task);
    puts("TCP packets + socket FD ABI: listen/accept/connect, backlog, payload, retransmission, flags, poll, EOF and close PASS");
    return 0;
}
