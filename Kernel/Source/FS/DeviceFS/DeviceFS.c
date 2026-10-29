// Kernel/Source/FS/DeviceFS/DeviceFS.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/DeviceFS/DeviceFS.h>
#include <Syscall/SyscallTerminal.h>
#include <Syscall/SyscallDisplay.h>
#include <LineOS/Errno.h>
#include <LineOS/Ioctl.h>
#include <LineOS/Memory.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Memory/PMM.h>
#include <Timer/Clock.h>

STATIC UINT32 DeviceMountTime;

_Static_assert(sizeof(FB_VAR_SCREENINFO) == 160, "Linux fb_var_screeninfo");
_Static_assert(sizeof(FB_FIX_SCREENINFO) == 80 && __builtin_offsetof(FB_FIX_SCREENINFO, LineLength) == 48, "Linux x86-64 fb_fix_screeninfo");
_Static_assert(sizeof(LOS_FB_RECT) == sizeof(DISPLAY_RECT), "Framebuffer rectangle ABI");

STATIC INT64 DeviceOpen(TASK *Task, UINT64 Flags, VOID **State)
{
    (VOID) Task;
    (VOID) Flags;
    *State = NULL;
    return 0;
}

STATIC INT64 DeviceClose(VOID *State)
{
    (VOID) State;
    return 0;
}

STATIC INT64 DeviceNoIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    (VOID) Task;
    (VOID) State;
    (VOID) Request;
    (VOID) Argument;
    (VOID) Access;
    return -ENOTTY;
}

STATIC INT64 DeviceNullRead(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) Task;
    (VOID) State;
    (VOID) Offset;
    (VOID) Buffer;
    (VOID) Count;
    return 0;
}

STATIC INT64 DeviceDiscard(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) Task;
    (VOID) State;
    (VOID) Offset;
    (VOID) Buffer;
    return Count;
}

STATIC INT64 DeviceZeroRead(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    UINT8  Bytes[256] = {0};
    UINT64 Total = 0;
    (VOID) State;
    (VOID) Offset;
    while (Total < Count)
    {
        UINT64 Length = Count - Total > sizeof(Bytes) ? sizeof(Bytes) : Count - Total;
        if (!VMMCopyToUser(Task->AddressSpace, Buffer + Total, Bytes, Length))
        {
            return Total != 0 ? (INT64) Total : -EFAULT;
        }
        Total += Length;
    }
    return (INT64) Total;
}

STATIC INT64 DeviceFullWrite(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) Task;
    (VOID) State;
    (VOID) Offset;
    (VOID) Buffer;
    return Count == 0 ? 0 : -ENOSPC;
}

STATIC INT64 DeviceMasterOpen(TASK *Task, UINT64 Flags, VOID **State)
{
    (VOID) Flags;
    *State = NULL;
    if (!Task->AssetPublisher)
    {
        return -EACCES;
    }
    return TerminalAttach() ? 0 : -EBUSY;
}

STATIC INT64 DeviceMasterClose(VOID *State)
{
    (VOID) State;
    TerminalDetach();
    return 0;
}

STATIC INT64 DeviceTerminalRead(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) State;
    (VOID) Offset;
    return TerminalRead(Task, FALSE, Buffer, Count);
}

STATIC INT64 DeviceTerminalWrite(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) State;
    (VOID) Offset;
    return TerminalWrite(Task, FALSE, Buffer, Count);
}

STATIC INT64 DeviceMasterRead(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) State;
    (VOID) Offset;
    return TerminalRead(Task, TRUE, Buffer, Count);
}

STATIC INT64 DeviceMasterWrite(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) State;
    (VOID) Offset;
    return TerminalWrite(Task, TRUE, Buffer, Count);
}

STATIC INT64 FramebufferOpen(TASK *Task, UINT64 Flags, VOID **State)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }
    return DeviceOpen(Task, Flags, State);
}

STATIC INT64 FramebufferMap(TASK *Task, VOID *State, UINT64 Length, UINT64 Protection, UINT64 Flags, UINT64 Offset, UINT64 *Physical)
{
    (VOID) Task;
    (VOID) State;
    if ((Flags & 3) != MAP_SHARED || (Protection & PROT_EXEC) != 0)
    {
        return -EOPNOTSUPP;
    }
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }
    UINT64 Size = ((UINT64) GPU->FrameBufferWidth * GPU->FrameBufferHeight * 4 + 4095) & ~4095ULL;
    if (((UINT64) GPU->FrameBuffer & 4095) != 0 || Length == 0 || Length > UINT64_MAX - 4095 || (Offset & 4095) != 0 || Offset >= Size || ((Length + 4095) & ~4095ULL) > Size - Offset)
    {
        return -EINVAL;
    }
    *Physical = (UINT64) GPU->FrameBuffer + Offset;
    return 0;
}

STATIC INT64 FramebufferData(TASK *Task, UINT64 *Offset, UINT64 Buffer, UINT64 Count, BOOLEAN Write)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    UINT64           Total = 0;
    UINT64           Size;
    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }
    Size = (UINT64) GPU->FrameBufferWidth * GPU->FrameBufferHeight * sizeof(UINT32);
    if (*Offset >= Size)
    {
        return Write && Count != 0 ? -ENOSPC : 0;
    }
    if (Count > Size - *Offset)
    {
        Count = Size - *Offset;
    }
    while (Total < Count)
    {
        UINT64  Length = Count - Total > 4096 ? 4096 : Count - Total;
        UINT64  Flags = VirtIOGPUAcquireRenderLock();
        BOOLEAN Copied = Write ? VMMCopyFromUser(Task->AddressSpace, (UINT8 *) GPU->FrameBuffer + *Offset + Total, Buffer + Total, Length) : VMMCopyToUser(Task->AddressSpace, Buffer + Total, (UINT8 *) GPU->FrameBuffer + *Offset + Total, Length);
        VirtIOGPUReleaseRenderLock(Flags);
        if (!Copied)
        {
            break;
        }
        Total += Length;
    }
    *Offset += Total;
    if (Write && Total != 0)
    {
        VirtIOGPUMarkDirty(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
        if (!VirtIOGPUPresent())
        {
            return -EIO;
        }
    }
    return Total != 0 || Count == 0 ? (INT64) Total : -EFAULT;
}

STATIC INT64 FramebufferRead(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) State;
    return FramebufferData(Task, Offset, Buffer, Count, FALSE);
}

STATIC INT64 FramebufferWrite(TASK *Task, VOID *State, UINT64 *Offset, UINT64 Buffer, UINT64 Count)
{
    (VOID) State;
    return FramebufferData(Task, Offset, Buffer, Count, TRUE);
}

STATIC INT64 FramebufferIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    (VOID) State;
    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }
    if (Request == FBIOGET_VSCREENINFO)
    {
        FB_VAR_SCREENINFO Info = {0};
        Info.XRes = Info.XResVirtual = GPU->FrameBufferWidth;
        Info.YRes = Info.YResVirtual = GPU->FrameBufferHeight;
        Info.BitsPerPixel = 32;
        Info.Red.Offset = 16;
        Info.Green.Offset = 8;
        Info.Blue.Offset = 0;
        Info.Transp.Offset = 24;
        Info.Red.Length = Info.Green.Length = Info.Blue.Length = Info.Transp.Length = 8;
        Info.Height = Info.Width = UINT32_MAX;
        return VMMCopyToUser(Task->AddressSpace, Argument, &Info, sizeof(Info)) ? 0 : -EFAULT;
    }
    if (Request == FBIOGET_FSCREENINFO)
    {
        FB_FIX_SCREENINFO Info = {0};
        KMemCpy(Info.ID, "LineOS VirtIO", 13);
        Info.SmemLen = GPU->FrameBufferWidth * GPU->FrameBufferHeight * sizeof(UINT32);
        Info.LineLength = GPU->FrameBufferWidth * sizeof(UINT32);
        Info.Visual = 2;
        return VMMCopyToUser(Task->AddressSpace, Argument, &Info, sizeof(Info)) ? 0 : -EFAULT;
    }
    if (Request != LOS_FBIO_PRESENT && Request != LOS_FBIO_PRESENT_RECT && Request != LOS_FBIO_FLUSH)
    {
        return -ENOTTY;
    }
    if ((Access & 2U) == 0)
    {
        return -EACCES;
    }
    if (Request == LOS_FBIO_PRESENT)
    {
        LOS_FB_PRESENT Present;
        if (!VMMCopyFromUser(Task->AddressSpace, &Present, Argument, sizeof(Present)))
        {
            return -EFAULT;
        }
        return SyscallDisplayPresent(Present.Buffer, Present.Size);
    }
    if (Request == LOS_FBIO_PRESENT_RECT)
    {
        return SyscallDisplayPresentRect(Argument);
    }
    VirtIOGPUMarkDirty(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
    return VirtIOGPUPresent() ? 0 : -EIO;
}

STATIC CONST DEVICE_OPERATIONS NullOperations = {DeviceOpen, DeviceNullRead, DeviceDiscard, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS ZeroOperations = {DeviceOpen, DeviceZeroRead, DeviceDiscard, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS FullOperations = {DeviceOpen, DeviceZeroRead, DeviceFullWrite, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS TerminalOperations = {DeviceOpen, DeviceTerminalRead, DeviceTerminalWrite, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS MasterOperations = {DeviceMasterOpen, DeviceMasterRead, DeviceMasterWrite, DeviceMasterClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS FramebufferOperations = {FramebufferOpen, FramebufferRead, FramebufferWrite, DeviceClose, FramebufferMap, FramebufferIoctl};
STATIC CONST DEVICE_NODE       DeviceNodes[] = {{(CONST CHAR8 *) "null", 0666, FALSE, &NullOperations}, {(CONST CHAR8 *) "zero", 0666, FALSE, &ZeroOperations}, {(CONST CHAR8 *) "full", 0666, FALSE, &FullOperations}, {(CONST CHAR8 *) "lsh1", 0666, FALSE, &TerminalOperations}, {(CONST CHAR8 *) "lsh1-master", 0600, FALSE, &MasterOperations}, {(CONST CHAR8 *) "fb0", 0666, TRUE, &FramebufferOperations}};

CONST DEVICE_NODE *DeviceFSGetNode(UINT32 Number)
{
    return Number >= 2 && Number - 2 < sizeof(DeviceNodes) / sizeof(DeviceNodes[0]) ? &DeviceNodes[Number - 2] : NULL;
}

STATIC VOID DeviceFSNode(UINT32 Number, VFS_NODE *Node)
{
    KMemSet(Node, 0, sizeof(*Node));
    Node->Number = Number;
    Node->AccessTime = DeviceMountTime;
    Node->ModifyTime = DeviceMountTime;
    Node->ChangeTime = DeviceMountTime;
    Node->Mode = Number == 1 ? VFS_MODE_DIRECTORY | 0755 : 0x2000U | DeviceFSGetNode(Number)->Mode;
}

STATIC VFS_ERROR DeviceFSRoot(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_NODE *Node)
{
    (VOID) Backend;
    (VOID) Credentials;
    DeviceFSNode(1, Node);
    return VFS_OK;
}

STATIC BOOLEAN DeviceNameEqual(CONST CHAR8 *A, CONST CHAR8 *B)
{
    while (*A != 0 && *A == *B)
    {
        A++;
        B++;
    }
    return *A == *B;
}

STATIC VFS_ERROR DeviceFSChild(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Parent, CONST CHAR8 *Name, VFS_NODE *Node)
{
    (VOID) Backend;
    (VOID) Credentials;
    if (Parent != 1)
    {
        return VFS_NOT_DIRECTORY;
    }
    if (DeviceNameEqual(Name, (CONST CHAR8 *) ".") || DeviceNameEqual(Name, (CONST CHAR8 *) ".."))
    {
        DeviceFSNode(1, Node);
        return VFS_OK;
    }
    for (UINT32 Index = 0; Index < sizeof(DeviceNodes) / sizeof(DeviceNodes[0]); Index++)
    {
        if (DeviceNameEqual(Name, DeviceNodes[Index].Name))
        {
            DeviceFSNode(Index + 2, Node);
            return VFS_OK;
        }
    }
    return VFS_NOT_FOUND;
}

STATIC VFS_ERROR DeviceFSReadLink(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, CHAR8 *Target, UINT32 *Length)
{
    (VOID) Backend;
    (VOID) Credentials;
    (VOID) Number;
    (VOID) Target;
    (VOID) Length;
    return VFS_INVALID;
}

STATIC VFS_ERROR DeviceFSReadDirectory(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, UINT32 Number, UINT64 *Cursor, VFS_DIRECTORY_ENTRY *Entry)
{
    (VOID) Backend;
    (VOID) Credentials;
    KMemSet(Entry, 0, sizeof(*Entry));
    if (Number != 1)
    {
        return VFS_NOT_DIRECTORY;
    }
    if (*Cursor >= sizeof(DeviceNodes) / sizeof(DeviceNodes[0]) + 2)
    {
        Entry->End = TRUE;
        return VFS_OK;
    }
    CONST CHAR8 *Name = *Cursor == 0 ? (CONST CHAR8 *) "." : *Cursor == 1 ? (CONST CHAR8 *) ".." : DeviceNodes[*Cursor - 2].Name;
    UINT32       Length = 0;
    while (Name[Length] != 0)
    {
        Length++;
    }
    KMemCpy(Entry->Name, Name, Length + 1);
    DeviceFSNode(*Cursor < 2 ? 1 : (UINT32) *Cursor, &Entry->Node);
    (*Cursor)++;
    return VFS_OK;
}

STATIC VFS_ERROR DeviceFSMutate(VOID *Backend, CONST VFS_CREDENTIALS *Credentials, VFS_MUTATION *Mutation)
{
    (VOID) Backend;
    (VOID) Credentials;
    (VOID) Mutation;
    return VFS_READ_ONLY;
}

STATIC CONST VFS_BACKEND_OPERATIONS DeviceFSOperations = {DeviceFSRoot, DeviceFSChild, DeviceFSReadLink, DeviceFSReadDirectory, NULL, NULL, DeviceFSMutate};

VFS_ERROR DeviceFSMount(VFS_REGISTRY *Registry)
{
    UINT32    Partition;
    VFS_ERROR Error = VFSRegisterPartition(Registry, (CONST CHAR8 *) "dev", 0, (VOID *) DeviceNodes, &DeviceFSOperations, &Partition);
    if (Error == VFS_OK)
    {
        DeviceMountTime = (UINT32) ClockGetUnixTime();
        Registry->DevicePartition = Partition;
    }
    return Error;
}
