// Kernel/Source/Syscall/Network/SyscallSocket.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Network/SyscallSocket.h>
#include <Syscall/Errno.h>
#include <Scheduler/LBPWRR.h>
#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>

#define SOCKET_NONBLOCK 0x800U
#define SOCKET_CLOEXEC  0x80000U
#define SOCKET_DONTWAIT 0x40U
#define SOCKET_NOSIGNAL 0x4000U
#define SOCKET_PEEK     2U

typedef struct
{
    UINT16 Family;
    UINT8  Port[2];
    UINT8  Address[4];
    UINT8  Padding[8];
} SOCKET_ADDRESS;

typedef struct
{
    OPEN_FILE *File;
    UINT32     Handle;
    BOOLEAN    Reuse;
    BOOLEAN    KeepAlive;
    BOOLEAN    NoDelay;
    UINT32     ReceiveTimeout;
    UINT32     SendTimeout;
} SOCKET_FILE;

STATIC_ASSERT(sizeof(SOCKET_ADDRESS) == 16, "sockaddr_in ABI");

STATIC INT64 SocketAddressRead(TASK *Task, UINT64 Address, UINT64 Length, SOCKET_ADDRESS *Value)
{
    if (Length < sizeof(*Value))
    {
        return -EINVAL;
    }
    if (!VMMCopyFromUser(Task->Process->AddressSpace, Value, Address, sizeof(*Value)))
    {
        return -EFAULT;
    }
    return Value->Family == 2 ? 0 : -EAFNOSUPPORT;
}

STATIC INT64 SocketAddressWrite(TASK *Task, SOCKET_FILE *Socket, UINT64 Address, UINT64 LengthAddress, BOOLEAN Peer)
{
    UINT32 Length;
    if (!VMMCopyFromUser(Task->Process->AddressSpace, &Length, LengthAddress, sizeof(Length)))
    {
        return -EFAULT;
    }
    NETWORK_TCP_SOCKET_STATE State;
    INT64                    Result = NetworkTCPGetSocketState(Socket->Handle, &State, FALSE);
    if (Result < 0)
    {
        return Result;
    }
    if (Peer && !State.Connected)
    {
        return -ENOTCONN;
    }
    SOCKET_ADDRESS Value = {.Family = 2};
    NetworkWrite16(Value.Port, Peer ? State.Port : State.LocalPort);
    NetworkWrite32(Value.Address, Peer ? State.Address : State.LocalAddress);
    UINT32 Copy = Length < sizeof(Value) ? Length : sizeof(Value);
    Length      = sizeof(Value);
    return (Copy == 0 || VMMCopyToUser(Task->Process->AddressSpace, Address, &Value, Copy)) && VMMCopyToUser(Task->Process->AddressSpace, LengthAddress, &Length, sizeof(Length)) ? 0 : -EFAULT;
}

STATIC INT64 SocketTransfer(TASK *Task, SOCKET_FILE *Socket, UINT64 Buffer, UINT64 Length, BOOLEAN Write, UINT32 Flags)
{
    if ((Flags & ~(SOCKET_DONTWAIT | (Write ? SOCKET_NOSIGNAL : SOCKET_PEEK))) != 0)
    {
        return -EOPNOTSUPP;
    }
    UINT32 Count = Length > NETWORK_TCP_BUFFER_SIZE ? NETWORK_TCP_BUFFER_SIZE : (UINT32) Length;
    UINT8 *Bytes = KAlloc(Count == 0 ? 1 : Count);
    if (Bytes == NULL)
    {
        return -ENOMEM;
    }
    if (Write && Count != 0 && !VMMCopyFromUser(Task->Process->AddressSpace, Bytes, Buffer, Count))
    {
        KFree(Bytes);
        return -EFAULT;
    }
    UINT64 Start   = NetworkNow();
    UINT32 Timeout = Write ? ATOMIC_LOAD(&Socket->SendTimeout, ATOMIC_ACQUIRE) : ATOMIC_LOAD(&Socket->ReceiveTimeout, ATOMIC_ACQUIRE);
    INT64  Result;
    do
    {
        NetworkPoll();
        Result = Write ? NetworkTCPWriteSocket(Socket->Handle, Bytes, Count) : NetworkTCPReadSocket(Socket->Handle, Bytes, Count, (Flags & SOCKET_PEEK) != 0);
        if (Result != -EAGAIN || (Flags & SOCKET_DONTWAIT) != 0 || (ATOMIC_LOAD(&Socket->File->Flags, ATOMIC_ACQUIRE) & SOCKET_NONBLOCK) != 0)
        {
            break;
        }
        if (Timeout != 0 && NetworkNow() - Start >= Timeout)
        {
            break;
        }
        NetworkWait();
    } while (TRUE);
    if (!Write && Result > 0 && !VMMCopyToUser(Task->Process->AddressSpace, Buffer, Bytes, (UINT64) Result))
    {
        Result = -EFAULT;
    }
    KFree(Bytes);
    return Result;
}

STATIC INT64 SocketRead(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) Offset;
    return SocketTransfer(Task, State, Buffer, Count, FALSE, 0);
}

STATIC INT64 SocketWrite(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) Offset;
    return SocketTransfer(Task, State, Buffer, Count, TRUE, 0);
}

STATIC INT64 SocketClose(VOID *State)
{
    SOCKET_FILE *Socket = State;
    NetworkTCPReleaseSocket(Socket->Handle);
    KFree(Socket);
    return 0;
}

STATIC INT64 SocketIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    (VOID) Access;
    SOCKET_FILE *Socket = State;
    INT32        Value;
    if (Request == 0x5421)
    {
        if (!VMMCopyFromUser(Task->Process->AddressSpace, &Value, Argument, sizeof(Value)))
        {
            return -EFAULT;
        }
        INT64 Result = FileBegin(Socket->File);
        if (Result < 0)
        {
            return Result;
        }
        UINT32 Flags = Socket->File->Flags;
        ATOMIC_STORE(&Socket->File->Flags, Value != 0 ? Flags | SOCKET_NONBLOCK : Flags & ~SOCKET_NONBLOCK, ATOMIC_RELEASE);
        FileEnd(Socket->File);
        return 0;
    }
    if (Request == 0x541B)
    {
        NETWORK_TCP_SOCKET_STATE Status;
        INT64                    Result = NetworkTCPGetSocketState(Socket->Handle, &Status, FALSE);
        if (Result < 0)
        {
            return Result;
        }
        Value = (INT32) Status.Available;
        return VMMCopyToUser(Task->Process->AddressSpace, Argument, &Value, sizeof(Value)) ? 0 : -EFAULT;
    }
    return -ENOTTY;
}

STATIC CONST DEVICE_OPERATIONS SocketOperations = {.Read = SocketRead, .Write = SocketWrite, .Close = SocketClose, .Ioctl = SocketIoctl};
STATIC CONST DEVICE_NODE       SocketDevice     = {.Name = (CONST CHAR8 *) "socket", .Mode = 0xC1FF, .Operations = &SocketOperations};

STATIC INT64 SocketInstall(TASK *Task, UINT32 Handle, UINT32 Flags)
{
    OPEN_FILE   *File   = KAlloc(sizeof(*File));
    SOCKET_FILE *Socket = KAlloc(sizeof(*Socket));
    if (File == NULL || Socket == NULL)
    {
        KFree(File);
        KFree(Socket);
        NetworkTCPReleaseSocket(Handle);
        return -ENOMEM;
    }
    KMemSet(File, 0, sizeof(*File));
    KMemSet(Socket, 0, sizeof(*Socket));
    File->References  = 1;
    File->Socket      = TRUE;
    File->Access      = FILE_READ | FILE_WRITE;
    File->Flags       = 2 | (Flags & SOCKET_NONBLOCK);
    File->Device      = &SocketDevice;
    File->DeviceState = Socket;
    Socket->Handle    = Handle;
    Socket->File      = File;
    WaitQueueInit(&File->Waiters);
    INT64 Result = FileInstall(Task, File, (Flags & SOCKET_CLOEXEC) != 0);
    if (Result < 0)
    {
        FileDrop(&File);
    }
    return Result;
}

INT64 SyscallSocket(SYSCALL_CONTEXT *Context)
{
    TASK *Task = LBPWRRGetCurrentTask();
    if (Task == NULL || Task->Process->AddressSpace == NULL)
    {
        return -ESRCH;
    }
    if ((INT32) Context->Args.RDI != 2)
    {
        return -EAFNOSUPPORT;
    }
    UINT32 Type = (UINT32) Context->Args.RSI;
    if ((Type & ~(SOCKET_NONBLOCK | SOCKET_CLOEXEC)) != 1)
    {
        return -ESOCKTNOSUPPORT;
    }
    if ((INT32) Context->Args.RDX != 0 && (INT32) Context->Args.RDX != 6)
    {
        return -EPROTONOSUPPORT;
    }
    UINT32 Handle;
    INT64  Result = NetworkTCPOpenSocket(&Handle);
    return Result < 0 ? Result : SocketInstall(Task, Handle, Type);
}

INT64 SyscallBind(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    SOCKET_ADDRESS Address;
    INT64          Result = SocketAddressRead(Task, Context->Args.RSI, Context->Args.RDX, &Address);
    if (Result < 0)
    {
        return Result;
    }
    UINT16 Port = NetworkRead16(Address.Port);
    if (Port != 0 && Port < 1024 && Task->Process->FileContext->Credentials.UID != 0)
    {
        return -EACCES;
    }
    SOCKET_FILE *Socket = File->DeviceState;
    return NetworkTCPBind(Socket->Handle, NetworkRead32(Address.Address), Port, ATOMIC_LOAD(&Socket->Reuse, ATOMIC_ACQUIRE));
}

INT64 SyscallListen(SYSCALL_CONTEXT *Context)
{
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(LBPWRRGetCurrentTask(), Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    return NetworkTCPListen(((SOCKET_FILE *) File->DeviceState)->Handle, (INT32) Context->Args.RSI < 0 ? NETWORK_TCP_SOCKET_COUNT : (UINT32) Context->Args.RSI);
}

INT64 SyscallAccept(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    UINT32 Flags = Context->Number == SYSCALL_ACCEPT4 ? (UINT32) Context->Args.R10 : 0;
    if ((Flags & ~(SOCKET_NONBLOCK | SOCKET_CLOEXEC)) != 0)
    {
        return -EINVAL;
    }
    SOCKET_FILE *Socket = File->DeviceState;
    UINT32       Child;
    INT64        Result;
    do
    {
        NetworkPoll();
        Result = NetworkTCPAccept(Socket->Handle, &Child);
        if (Result != -EAGAIN || (ATOMIC_LOAD(&File->Flags, ATOMIC_ACQUIRE) & SOCKET_NONBLOCK) != 0)
        {
            break;
        }
        NetworkWait();
    } while (TRUE);
    if (Result < 0)
    {
        return Result;
    }
    SOCKET_FILE Temporary = {.Handle = Child};
    if (Context->Args.RSI != 0 && (Result = SocketAddressWrite(Task, &Temporary, Context->Args.RSI, Context->Args.RDX, TRUE)) < 0)
    {
        NetworkTCPReleaseSocket(Child);
        return Result;
    }
    return SocketInstall(Task, Child, Flags);
}

INT64 SyscallConnect(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    SOCKET_ADDRESS Address;
    INT64          Result = SocketAddressRead(Task, Context->Args.RSI, Context->Args.RDX, &Address);
    if (Result < 0)
    {
        return Result;
    }
    SOCKET_FILE *Socket = File->DeviceState;
    Result              = NetworkTCPStartConnect(Socket->Handle, NetworkRead32(Address.Address), NetworkRead16(Address.Port));
    if (Result < 0)
    {
        return Result;
    }
    if ((ATOMIC_LOAD(&File->Flags, ATOMIC_ACQUIRE) & SOCKET_NONBLOCK) != 0)
    {
        return -EINPROGRESS;
    }
    for (;;)
    {
        NetworkWait();
        NETWORK_TCP_SOCKET_STATE State;
        Result = NetworkTCPGetSocketState(Socket->Handle, &State, FALSE);
        if (Result < 0 || State.Error != 0 || State.Connected)
        {
            return Result < 0 ? Result : State.Error != 0 ? -(INT64) State.Error : 0;
        }
    }
}

INT64 SyscallSocketName(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    return SocketAddressWrite(Task, File->DeviceState, Context->Args.RSI, Context->Args.RDX, Context->Number == SYSCALL_GETPEERNAME);
}

INT64 SyscallSendTo(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    if (Context->Args.R8 != 0)
    {
        return -EISCONN;
    }
    return SocketTransfer(Task, File->DeviceState, Context->Args.RSI, Context->Args.RDX, TRUE, (UINT32) Context->Args.R10);
}

INT64 SyscallRecvFrom(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    INT64 Result = SocketTransfer(Task, File->DeviceState, Context->Args.RSI, Context->Args.RDX, FALSE, (UINT32) Context->Args.R10);
    if (Result >= 0 && Context->Args.R8 != 0)
    {
        INT64 Error = SocketAddressWrite(Task, File->DeviceState, Context->Args.R8, Context->Args.R9, TRUE);
        if (Error < 0)
        {
            return Error;
        }
    }
    return Result;
}

INT64 SyscallShutdown(SYSCALL_CONTEXT *Context)
{
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(LBPWRRGetCurrentTask(), Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    if ((UINT32) Context->Args.RSI > 2)
    {
        return -EINVAL;
    }
    return NetworkTCPShutdown(((SOCKET_FILE *) File->DeviceState)->Handle, (UINT32) Context->Args.RSI);
}

INT64 SyscallSocketOption(SYSCALL_CONTEXT *Context)
{
    TASK      *Task                                    = LBPWRRGetCurrentTask();
    OPEN_FILE *File __attribute__((cleanup(FileDrop))) = FileAcquire(Task, Context->Args.RDI);
    if (File == NULL || !File->Socket)
    {
        return File == NULL ? -EBADF : -ENOTSOCK;
    }
    SOCKET_FILE *Socket = File->DeviceState;
    INT32        Level  = (INT32) Context->Args.RSI;
    INT32        Option = (INT32) Context->Args.RDX;
    BOOLEAN      Set    = Context->Number == SYSCALL_SETSOCKOPT;
    INT32        Value  = 0;
    UINT32       Length = (UINT32) Context->Args.R8;
    if (!Set && !VMMCopyFromUser(Task->Process->AddressSpace, &Length, Context->Args.R8, sizeof(Length)))
    {
        return -EFAULT;
    }
    if (Level == 1 && (Option == 20 || Option == 21))
    {
        struct
        {
            INT64 Seconds;
            INT64 Microseconds;
        } Time = {0};
        if (Length < sizeof(Time))
        {
            return -EINVAL;
        }
        UINT32 *Timeout = Option == 20 ? &Socket->ReceiveTimeout : &Socket->SendTimeout;
        if (Set)
        {
            if (!VMMCopyFromUser(Task->Process->AddressSpace, &Time, Context->Args.R10, sizeof(Time)))
            {
                return -EFAULT;
            }
            if (Time.Seconds < 0 || Time.Seconds > 2147483 || Time.Microseconds < 0 || Time.Microseconds >= 1000000)
            {
                return -EINVAL;
            }
            ATOMIC_STORE(Timeout, (UINT32) (Time.Seconds * 1000 + (Time.Microseconds + 999) / 1000), ATOMIC_RELEASE);
            return 0;
        }
        UINT32 TimeoutValue = ATOMIC_LOAD(Timeout, ATOMIC_ACQUIRE);
        Time.Seconds        = TimeoutValue / 1000;
        Time.Microseconds   = (TimeoutValue % 1000) * 1000;
        Length              = sizeof(Time);
        return VMMCopyToUser(Task->Process->AddressSpace, Context->Args.R10, &Time, sizeof(Time)) && VMMCopyToUser(Task->Process->AddressSpace, Context->Args.R8, &Length, sizeof(Length)) ? 0 : -EFAULT;
    }
    BOOLEAN *Boolean = Level == 1 && Option == 2 ? &Socket->Reuse : Level == 1 && Option == 9 ? &Socket->KeepAlive : Level == 6 && Option == 1 ? &Socket->NoDelay : NULL;
    if (Boolean != NULL)
    {
        if (Set)
        {
            if (Length < sizeof(Value))
            {
                return -EINVAL;
            }
            if (!VMMCopyFromUser(Task->Process->AddressSpace, &Value, Context->Args.R10, sizeof(Value)))
            {
                return -EFAULT;
            }
            if (Level == 1 && Option == 9 && Value != 0)
            {
                return -EOPNOTSUPP;
            }
            ATOMIC_STORE(Boolean, Value != 0, ATOMIC_RELEASE);
            return 0;
        }
        Value = ATOMIC_LOAD(Boolean, ATOMIC_ACQUIRE);
    }
    else if (!Set && Level == 1)
    {
        NETWORK_TCP_SOCKET_STATE State;
        INT64                    Result = NetworkTCPGetSocketState(Socket->Handle, &State, Option == 4);
        if (Result < 0)
        {
            return Result;
        }
        switch (Option)
        {
            case 3:
                Value = 1;
                break;
            case 4:
                Value = State.Error;
                break;
            case 7:
            case 8:
                Value = NETWORK_TCP_BUFFER_SIZE;
                break;
            case 30:
                Value = State.Listening;
                break;
            case 38:
                Value = 6;
                break;
            case 39:
                Value = 2;
                break;
            default:
                return -ENOPROTOOPT;
        }
    }
    else
    {
        return -ENOPROTOOPT;
    }
    if (Set)
    {
        return -ENOPROTOOPT;
    }
    UINT32 Copy = Length < sizeof(Value) ? Length : sizeof(Value);
    Length      = sizeof(Value);
    return (Copy == 0 || VMMCopyToUser(Task->Process->AddressSpace, Context->Args.R10, &Value, Copy)) && VMMCopyToUser(Task->Process->AddressSpace, Context->Args.R8, &Length, sizeof(Length)) ? 0 : -EFAULT;
}

INT64 SyscallSocketPoll(OPEN_FILE *File, UINT16 Events)
{
    NETWORK_TCP_SOCKET_STATE State;
    INT64                    Result = NetworkTCPGetSocketState(((SOCKET_FILE *) File->DeviceState)->Handle, &State, FALSE);
    return Result < 0 ? 8 | 0x10 : State.Events & (Events | 8 | 0x10);
}
