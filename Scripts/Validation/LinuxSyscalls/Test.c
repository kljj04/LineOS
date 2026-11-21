#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Syscall/File/FileInternal.h>
#include <Syscall/File/SyscallStat.h>
#include <Syscall/Memory/SyscallMemory.h>
#include <Syscall/SyscallError.h>
#include <Syscall/File/Stat.h>
#include <Memory/PMM.h>
#include <Syscall/Memory/Memory.h>
#include <Syscall/Process/SyscallThread.h>
#include <Syscall/Device/SyscallIoctl.h>

STATIC PROCESS      Process;
STATIC TASK         Task;
STATIC VFS_CONTEXT  Context;
STATIC VFS_REGISTRY Registry;
STATIC UINT64       Pages[16384];
STATIC UINT64       Base            = 0x10000000;
STATIC INT64        AllocationLimit = -1;
STATIC BOOLEAN      MapFailure;
STATIC BOOLEAN      ReadFailure;
STATIC UINTN        LivePages;
STATIC UINT64       FileGeneration;
STATIC BOOLEAN      TrustedOwner;
STATIC BOOLEAN      InsecureDirectory;

VOID *KAlloc(UINTN Size)
{
    return malloc(Size);
}
VOID KFree(VOID *Address)
{
    free(Address);
}
VOID *KMemCpy(VOID *Destination, CONST VOID *Source, UINTN Size)
{
    return memcpy(Destination, Source, Size);
}
VOID *KMemSet(VOID *Destination, UINT8 Value, UINTN Size)
{
    return memset(Destination, Value, Size);
}
VOID *KAllocPages(UINTN Count)
{
    if (AllocationLimit == 0)
    {
        return NULL;
    }
    if (AllocationLimit > 0)
    {
        AllocationLimit--;
    }
    VOID *Result = aligned_alloc(4096, Count * 4096);
    if (Result != NULL)
    {
        LivePages += Count;
    }
    return Result;
}
VOID KMemFreePages(VOID *Address, UINTN Count)
{
    LivePages -= Count;
    free(Address);
}
TASK *LBPWRRGetCurrentTask(VOID)
{
    return &Task;
}
UINT64 SpinLockAcquireIRQSave(SPIN_LOCK *Lock)
{
    return 0;
}
VOID    SpinLockReleaseIRQRestore(SPIN_LOCK *Lock, UINT64 Flags) {}
BOOLEAN WaitQueueSleep(WAIT_QUEUE *Queue, SPIN_LOCK *Lock)
{
    return FALSE;
}
VOID    WaitQueueWakeAll(WAIT_QUEUE *Queue) {}
BOOLEAN ProcessBegin(VOID)
{
    return TRUE;
}
INT64 ProcessEnd(INT64 Result)
{
    return Result;
}
VOID               VMMHandleShootdown(VOID) {}
CONST DEVICE_NODE *DeviceFSGetNode(UINT32 Number)
{
    return NULL;
}
BOOLEAN VMMCopyFromUser(VMM_ADDRESS_SPACE *Space, VOID *Destination, UINT64 Source, UINT64 Size)
{
    if (Source < 4096 || Source == 0xdeadbeef)
    {
        return FALSE;
    }
    memcpy(Destination, (VOID *) Source, Size);
    return TRUE;
}
BOOLEAN VMMCopyToUser(VMM_ADDRESS_SPACE *Space, UINT64 Destination, CONST VOID *Source, UINT64 Size)
{
    if (Destination < 4096)
    {
        return FALSE;
    }
    memcpy((VOID *) Destination, Source, Size);
    return TRUE;
}
BOOLEAN VMMTranslateAddressSpace(VMM_ADDRESS_SPACE *Space, UINT64 Address, UINT64 *Physical)
{
    if (Address < Base || Address - Base >= sizeof(Pages) / 8 * 4096)
    {
        return FALSE;
    }
    *Physical = Pages[(Address - Base) / 4096];
    return *Physical != 0;
}
BOOLEAN VMMMapUserPages(VMM_ADDRESS_SPACE *Space, UINT64 Address, CONST UINT64 *Physical, UINT32 Count, UINT64 Flags)
{
    if (MapFailure)
    {
        return FALSE;
    }
    for (UINT32 Index = 0; Index < Count; Index++)
    {
        assert(Pages[(Address - Base) / 4096 + Index] == 0);
        Pages[(Address - Base) / 4096 + Index] = Physical[Index];
    }
    return TRUE;
}
BOOLEAN VMMUnmapUserPages(VMM_ADDRESS_SPACE *Space, UINT64 Address, CONST UINT64 *Physical, UINT32 Count)
{
    for (UINT32 Index = 0; Index < Count; Index++)
    {
        assert(Pages[(Address - Base) / 4096 + Index] == Physical[Index]);
        Pages[(Address - Base) / 4096 + Index] = 0;
    }
    return TRUE;
}
BOOLEAN VMMReplaceUserPages(VMM_ADDRESS_SPACE *Space, UINT64 Address, CONST UINT64 *Physical, CONST UINT64 *Expected, UINT32 Count, UINT64 Flags)
{
    if (MapFailure)
    {
        return FALSE;
    }
    for (UINT32 Index = 0; Index < Count; Index++)
    {
        assert(Pages[(Address - Base) / 4096 + Index] == Expected[Index]);
    }
    for (UINT32 Index = 0; Index < Count; Index++)
    {
        Pages[(Address - Base) / 4096 + Index] = Physical[Index];
    }
    return TRUE;
}
STATIC VOID MakeNode(UINT32 Number, VFS_NODE *Node)
{
    memset(Node, 0, sizeof(*Node));
    Node->Number     = Number;
    Node->Generation = Number == 3 ? FileGeneration : 0;
    Node->UID        = TrustedOwner ? 0 : 1000;
    Node->GID        = 1000;
    Node->Size       = Number == 3 ? 6000 : 0;
    Node->Mode       = Number <= 2 ? 0040755 : Number == 4 ? 0120777 : 0100644;
    if (Number == 2 && InsecureDirectory)
    {
        Node->Mode = 0040777;
    }
}
STATIC VFS_ERROR Root(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_NODE *Node)
{
    MakeNode(1, Node);
    return VFS_OK;
}
STATIC VFS_ERROR Child(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Parent, CONST CHAR8 *Name, VFS_NODE *Node)
{
    if (Parent > 2)
    {
        return VFS_NOT_DIRECTORY;
    }
    UINT32 Number = 0;
    if (!strcmp((char *) Name, "."))
    {
        Number = Parent;
    }
    else if (!strcmp((char *) Name, ".."))
    {
        Number = 1;
    }
    else if (Parent == 1 && !strcmp((char *) Name, "lib"))
    {
        Number = 2;
    }
    else if (Parent == 2 && !strcmp((char *) Name, "libc.so.6"))
    {
        Number = 3;
    }
    else if (Parent == 2 && !strcmp((char *) Name, "link"))
    {
        Number = 4;
    }
    if (Number == 0)
    {
        return VFS_NOT_FOUND;
    }
    MakeNode(Number, Node);
    return VFS_OK;
}
STATIC VFS_ERROR ReadLink(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, CHAR8 *Target, UINT32 *Length)
{
    memcpy(Target, "libc.so.6", 10);
    *Length = 9;
    return VFS_OK;
}
STATIC VFS_ERROR Read(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength)
{
    *ReadLength = 0;
    if (ReadFailure && Length != 0)
    {
        return VFS_IO;
    }
    while (*ReadLength < Length && Offset + *ReadLength < 6000)
    {
        ((UINT8 *) Buffer)[*ReadLength] = (UINT8) (Offset + *ReadLength);
        (*ReadLength)++;
    }
    return VFS_OK;
}
STATIC VFS_ERROR Directory(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 *Cursor, VFS_DIRECTORY_ENTRY *Entry)
{
    Entry->End = TRUE;
    return VFS_OK;
}
STATIC UINT64 SeenRequest;
STATIC INT64  Ioctl(TASK *Current, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    SeenRequest = Request;
    return 777;
}
#define PATH(Text) ((UINT64) (Text))
INT32 main(VOID)
{
    STATIC CONST VFS_BACKEND_OPERATIONS Operations = {.Root = Root, .Child = Child, .ReadLink = ReadLink, .Read = Read, .ReadDirectory = Directory};
    VFSInit(&Registry);
    UINT32 Partition;
    assert(VFSRegisterPartition(&Registry, (CONST CHAR8 *) "root", 0, &Registry, &Operations, &Partition) == VFS_OK);
    Registry.RootPartition = Partition;
    assert(VFSContextInit(&Context, &Registry, Partition, (VFS_CREDENTIALS) {1000, 1000, FALSE}) == VFS_OK);
    Task.Process         = &Process;
    Process.AddressSpace = (VOID *) 1;
    Process.FileContext  = &Context;
    Process.RealUID      = 1000;
    Process.RealGID      = 1000;
    INT64 Dir            = SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-100), .Args.RSI = (UINT64) (PATH("/lib")), .Args.RDX = (UINT64) (0x90000), .Args.R10 = (UINT64) (0)});
    assert(Dir >= 0);
    INT64 FD = SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) Dir), .Args.RSI = (UINT64) (PATH("libc.so.6")), .Args.RDX = (UINT64) (0x80000), .Args.R10 = (UINT64) (0)});
    assert(FD >= 0);
    UINT8 Buffer[32]          = {0};
    Process.Files[FD]->Offset = 19;
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (sizeof(Buffer)), .Args.R10 = (UINT64) (INT64) (42)}) == sizeof(Buffer));
    assert(Buffer[0] == 42 && Process.Files[FD]->Offset == 19);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (sizeof(Buffer)), .Args.R10 = (UINT64) (INT64) (5990)}) == 10 && Process.Files[FD]->Offset == 19);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (sizeof(Buffer)), .Args.R10 = (UINT64) (INT64) (6000)}) == 0 && Process.Files[FD]->Offset == 19);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (INT64) (42)}) == 0 && Process.Files[FD]->Offset == 19);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (1), .Args.RDX = (UINT64) (32), .Args.R10 = (UINT64) (INT64) (0)}) == -EFAULT && Process.Files[FD]->Offset == 19);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (32), .Args.R10 = (UINT64) (INT64) (-1)}) == -EINVAL);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (99), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (32), .Args.R10 = (UINT64) (INT64) (0)}) == -EBADF);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Dir), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (32), .Args.R10 = (UINT64) (INT64) (0)}) == -EISDIR);
    Process.Files[FD]->Offset = 0;
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (1), .Args.RDX = (UINT64) (0)}) == 1);
    INT64 Duplicate = SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (0), .Args.RDX = (UINT64) (10)});
    assert(Duplicate == 10 && SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Duplicate), .Args.RSI = (UINT64) (INT32) (1), .Args.RDX = (UINT64) (0)}) == 0);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Duplicate), .Args.RSI = (UINT64) (INT32) (2), .Args.RDX = (UINT64) (1)}) == 0 && SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (1), .Args.RDX = (UINT64) (0)}) == 1);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (2), .Args.RDX = (UINT64) (0)}) == 0 && SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Duplicate), .Args.RSI = (UINT64) (INT32) (1), .Args.RDX = (UINT64) (0)}) == 1);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (4), .Args.RDX = (UINT64) (0x400)}) == 0 && (SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Duplicate), .Args.RSI = (UINT64) (INT32) (3), .Args.RDX = (UINT64) (0)}) & 0x400) != 0);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Duplicate), .Args.RSI = (UINT64) (INT32) (4), .Args.RDX = (UINT64) (0)}) == 0 && !Process.Files[FD]->Append);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (1030), .Args.RDX = (UINT64) (12)}) == 12 && SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (12), .Args.RSI = (UINT64) (INT32) (1), .Args.RDX = (UINT64) (0)}) == 1);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (0), .Args.RDX = (UINT64) ((UINT64) -1)}) == -EINVAL);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (31), .Args.RSI = (UINT64) (INT32) (1), .Args.RDX = (UINT64) (0)}) == -EBADF);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (999), .Args.RDX = (UINT64) (0)}) == -EINVAL);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (2), .Args.RDX = (UINT64) (1)}) == 0);
    PROCESS ChildProcess = {0};
    TASK    Child        = {.Process = &ChildProcess};
    assert(SyscallFilesInherit(&Child, &Task));
    assert((ChildProcess.CloseOnExecFiles & (1U << FD)) != 0);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (2), .Args.RDX = (UINT64) (0)}) == 0 && (ChildProcess.CloseOnExecFiles & (1U << FD)) != 0);
    SyscallFilesRelease(&Child);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (2), .Args.RDX = (UINT64) (1)}) == 0);
    STATIC CONST DEVICE_OPERATIONS DeviceOperations = {.Ioctl = Ioctl};
    STATIC CONST DEVICE_NODE       Device           = {.Operations = &DeviceOperations};
    Process.Files[FD]->Device                       = &Device;
    assert(SyscallIoctl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (0xffffffff80304c05ULL), .Args.RDX = (UINT64) (0)}) == 777 && SeenRequest == 0x80304c05U);
    assert(SyscallFcntl(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (INT32) (4), .Args.RDX = (UINT64) (0x800)}) == -EOPNOTSUPP);
    Process.Files[FD]->Device = NULL;
    assert(SyscallClose(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Duplicate)}) == 0 && SyscallClose(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (12)}) == 0);
    UINT64 Mask        = ~0ULL;
    UINT64 Old         = 99;
    UINT64 Unblockable = (1ULL << 8) | (1ULL << 18);
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (2), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (PATH(&Old)), .Args.R10 = (UINT64) (8)}) == 0 && Old == 0 && Task.SignalMask == (~0ULL & ~Unblockable));
    Mask = 2;
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (1), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (8)}) == 0 && (Task.SignalMask & 2) == 0);
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (0), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (8)}) == 0 && (Task.SignalMask & 2) != 0);
    Old = 0;
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (999), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (PATH(&Old)), .Args.R10 = (UINT64) (8)}) == 0 && Old == Task.SignalMask);
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (3), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (8)}) == -EINVAL);
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (0), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (16)}) == -EINVAL);
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (2), .Args.RSI = (UINT64) (1), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (8)}) == -EFAULT);
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (2), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (1), .Args.R10 = (UINT64) (8)}) == -EFAULT && Task.SignalMask == Old);
    Mask = 0;
    assert(SyscallRTSigprocmask(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (2), .Args.RSI = (UINT64) (PATH(&Mask)), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (8)}) == 0);
    typedef struct
    {
        INT32 FD;
        INT16 Events;
        INT16 Returned;
    } POLL_ENTRY;
    POLL_ENTRY Poll[] = {
        {(INT32) FD, 0, -1},
        {31,         0, -1},
        {-1,         0, -1}
    };
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH(Poll)), .Args.RSI = (UINT64) (3), .Args.RDX = (UINT64) (INT32) (0)}) == 1 && Poll[0].Returned == 0 && Poll[1].Returned == 0x20 && Poll[2].Returned == 0);
    Poll[0].Events = 1;
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH(Poll)), .Args.RSI = (UINT64) (3), .Args.RDX = (UINT64) (INT32) (0)}) == 2 && Poll[0].Returned == 1);
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (1), .Args.RSI = (UINT64) (3), .Args.RDX = (UINT64) (INT32) (0)}) == -EFAULT);
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH(Poll)), .Args.RSI = (UINT64) (33), .Args.RDX = (UINT64) (INT32) (0)}) == -EINVAL);
    assert(SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (INT32) (0)}) == 0 && SyscallPoll(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (INT32) (1)}) == 0);
    assert(!VFSIsTrustedPath(&Context, (CONST CHAR8 *) "/lib/libc.so.6"));
    TrustedOwner = TRUE;
    assert(VFSIsTrustedPath(&Context, (CONST CHAR8 *) "/lib/libc.so.6"));
    assert(!VFSIsTrustedPath(&Context, (CONST CHAR8 *) "/lib/link"));
    assert(!VFSIsTrustedPath(&Context, (CONST CHAR8 *) "/lib/../lib/libc.so.6"));
    InsecureDirectory = TRUE;
    assert(!VFSIsTrustedPath(&Context, (CONST CHAR8 *) "/lib/libc.so.6"));
    InsecureDirectory = FALSE;
    TrustedOwner      = FALSE;
    STAT_INFO Info;
    Registry.NamespaceGeneration++;
    assert(SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(&Info))}) == 0);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (sizeof(Buffer)), .Args.R10 = (UINT64) (INT64) (0)}) == sizeof(Buffer));
    FileGeneration++;
    assert(SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(&Info))}) == -ESTALE);
    assert(SyscallPread64(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(Buffer)), .Args.RDX = (UINT64) (sizeof(Buffer)), .Args.R10 = (UINT64) (INT64) (0)}) == -ENOENT);
    FileGeneration--;

    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) Dir), .Args.RSI = (UINT64) (PATH("link")), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0)}) == 0 && Info.Inode == 3);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) Dir), .Args.RSI = (UINT64) (PATH("link")), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0x100)}) == 0 && Info.Inode == 4);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) FD), .Args.RSI = (UINT64) (PATH("")), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0x1000)}) == 0 && Info.Inode == 3);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) FD), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0x1000)}) == 0);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-100), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0x1000)}) == 0 && Info.Inode == 1);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-100), .Args.RSI = (UINT64) (PATH("/lib")), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (2)}) == -EINVAL);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) FD), .Args.RSI = (UINT64) (PATH("x")), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0)}) == -ENOTDIR);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-42), .Args.RSI = (UINT64) (PATH("x")), .Args.RDX = (UINT64) (PATH(&Info)), .Args.R10 = (UINT64) (0)}) == -EBADF);
    assert(SyscallNewfstatat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) Dir), .Args.RSI = (UINT64) (PATH("link")), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (0)}) == -EFAULT);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) Dir), .Args.RSI = (UINT64) (PATH("link")), .Args.RDX = (UINT64) (0x20000), .Args.R10 = (UINT64) (0)}) == -ELOOP);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) Dir), .Args.RSI = (UINT64) (PATH("libc.so.6")), .Args.RDX = (UINT64) (0x10000), .Args.R10 = (UINT64) (0)}) == -ENOTDIR);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-42), .Args.RSI = (UINT64) (PATH("x")), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (0)}) == -EBADF);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) ((INT32) FD), .Args.RSI = (UINT64) (PATH("x")), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (0)}) == -ENOTDIR);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-100), .Args.RSI = (UINT64) (PATH("")), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (0)}) == -ENOENT);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-100), .Args.RSI = (UINT64) (1), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (0)}) == -EFAULT);
    assert(SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-100), .Args.RSI = (UINT64) (PATH("/lib")), .Args.RDX = (UINT64) (0x1000000), .Args.R10 = (UINT64) (0)}) == -EINVAL);
    INT64 Absolute = SyscallOpenat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (INT32) (-42), .Args.RSI = (UINT64) (PATH("/lib/libc.so.6")), .Args.RDX = (UINT64) (0), .Args.R10 = (UINT64) (0)});
    assert(Absolute >= 0);
    SyscallClose(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Absolute)});
    assert(SyscallAccess(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("/lib/libc.so.6")), .Args.RSI = (UINT64) (4)}) == 0);
    assert(SyscallAccess(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("/lib/libc.so.6")), .Args.RSI = (UINT64) (1)}) == -EACCES);
    assert(SyscallAccess(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("/missing")), .Args.RSI = (UINT64) (0)}) == -ENOENT);
    assert(SyscallAccess(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("/lib")), .Args.RSI = (UINT64) (8)}) == -EINVAL);
    Process.RealUID = 0;
    assert(SyscallAccess(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("/lib/libc.so.6")), .Args.RSI = (UINT64) (1)}) == -EACCES);
    Process.RealUID = 2000;
    assert(SyscallAccess(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("/lib/libc.so.6")), .Args.RSI = (UINT64) (2)}) == -EACCES);
    Process.RealUID = 1000;
    assert(VFSChangeDirectory(&Context, (CONST CHAR8 *) "/lib") == VFS_OK);
    INT64 Relative = SyscallOpen(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (PATH("libc.so.6")), .Args.RSI = (UINT64) (0), .Args.RDX = (UINT64) (0)});
    assert(Relative >= 0);
    assert(VFSChangeDirectory(&Context, (CONST CHAR8 *) "/") == VFS_OK);
    assert(SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Relative), .Args.RSI = (UINT64) (PATH(&Info))}) == 0 && Info.Inode == 3);
    Base          = VMM_MMAP_START;
    INT64 Mapping = SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (8192), .Args.RDX = (UINT64) (PROT_READ | PROT_WRITE), .Args.R10 = (UINT64) (MAP_PRIVATE), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (0)});
    assert(Mapping == Base && LivePages == 2);
    assert(((UINT8 *) Pages[0])[42] == 42 && ((UINT8 *) Pages[1])[100] == 100);
    assert(((UINT8 *) Pages[1])[1904] == 0);
    assert(Process.Files[FD]->Offset == 0);
    ((UINT8 *) Pages[0])[42] = 99;
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE | MAP_FIXED_NOREPLACE), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (0)}) == -EEXIST);
    AllocationLimit = 0;
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE | MAP_FIXED), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (4096)}) == -ENOMEM);
    assert(((UINT8 *) Pages[0])[42] == 99 && LivePages == 2);
    AllocationLimit = -1;
    MapFailure      = TRUE;
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE | MAP_FIXED), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (4096)}) == -ENOMEM);
    assert(((UINT8 *) Pages[0])[42] == 99 && LivePages == 2);
    MapFailure = FALSE;
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE | MAP_FIXED), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (4096)}) == Base);
    assert(((UINT8 *) Pages[0])[42] == 42 && LivePages == 2);
    ReadFailure = TRUE;
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (0)}) == -EIO && LivePages == 2);
    ReadFailure = FALSE;
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 4096), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ | PROT_WRITE), .Args.R10 = (UINT64) (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED), .Args.R8 = (UINT64) (INT64) (-1), .Args.R9 = (UINT64) (0)}) == Base + 4096);
    assert(((UINT8 *) Pages[1])[42] == 0 && LivePages == 2);
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_SHARED), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (0)}) == -EOPNOTSUPP);
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE), .Args.R8 = (UINT64) (INT64) (-42), .Args.R9 = (UINT64) (0)}) == -EBADF);
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (1)}) == -EINVAL);
    assert(SyscallMmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 1), .Args.RSI = (UINT64) (4096), .Args.RDX = (UINT64) (PROT_READ), .Args.R10 = (UINT64) (MAP_PRIVATE | MAP_FIXED), .Args.R8 = (UINT64) (INT64) (FD), .Args.R9 = (UINT64) (0)}) == -EINVAL);
    SyscallFilesExec(&Task);
    assert(((UINT8 *) Pages[0])[42] == 42);
    assert(SyscallMunmap(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base), .Args.RSI = (UINT64) (8192)}) == 0 && LivePages == 0);
    Base = 0x10000000;

    assert(SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (FD), .Args.RSI = (UINT64) (PATH(&Info))}) == -EBADF);
    assert(SyscallFstat(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Relative), .Args.RSI = (UINT64) (PATH(&Info))}) == 0);
    SyscallFilesRelease(&Task);
    Process.BreakBase = Base;
    Process.Break     = Base;
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (0)}) == Base);
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 4097)}) == Base + 4097 && LivePages == 2);
    assert(*(UINT8 *) Pages[0] == 0);
    *(UINT8 *) Pages[0] = 42;
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 8000)}) == Base + 8000 && LivePages == 2);
    AllocationLimit = 1;
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 16384)}) == Base + 8000 && LivePages == 2);
    AllocationLimit = -1;
    MapFailure      = TRUE;
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 16384)}) == Base + 8000 && LivePages == 2);
    MapFailure = FALSE;
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base - 1)}) == Base + 8000);
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (~0ULL)}) == Base + 8000);
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 4096)}) == Base + 4096 && LivePages == 1);
    assert(*(UINT8 *) Pages[0] == 42);
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 4097)}) == Base + 4097 && *(UINT8 *) Pages[1] == 0);
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base)}) == Base && LivePages == 0);
    Pages[0] = 4096;
    assert(SyscallBrk(&(SYSCALL_CONTEXT) {.Args.RDI = (UINT64) (Base + 4096)}) == Base && LivePages == 0);
    Pages[0] = 0;
    SyscallMemoryRelease(&Task);
    puts("Linux syscall host tests PASS");
    return 0;
}
