// Kernel/Source/FS/DeviceFS/DeviceFS.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <FS/DeviceFS/DeviceFS.h>
#include <Syscall/Device/SyscallTerminal.h>
#include <Syscall/Device/SyscallDisplay.h>
#include <Syscall/Errno.h>
#include <Syscall/Device/Ioctl.h>
#include <Syscall/Device/VirGL.h>
#include <Render/VirGL/VirGL.h>
#include <Syscall/Memory/Memory.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Memory/PMM.h>
#include <Timer/Clock.h>
#include <Input/Input.h>

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
    UINT64 Total      = 0;

    (VOID) State;
    (VOID) Offset;

    while (Total < Count)
    {
        UINT64 Length = Count - Total > sizeof(Bytes) ? sizeof(Bytes) : Count - Total;

        if (!VMMCopyToUser(Task->Process->AddressSpace, Buffer + Total, Bytes, Length))
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

    if (!Task->Process->AssetPublisher)
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
    VIRTIO_GPU_INFO *GPU   = VirtIOGPUGetInfo();
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
        UINT64  Flags  = VirtIOGPUAcquireRenderLock();
        BOOLEAN Copied = Write ? VMMCopyFromUser(Task->Process->AddressSpace, (UINT8 *) GPU->FrameBuffer + *Offset + Total, Buffer + Total, Length) : VMMCopyToUser(Task->Process->AddressSpace, Buffer + Total, (UINT8 *) GPU->FrameBuffer + *Offset + Total, Length);

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
        Info.BitsPerPixel            = 32;
        Info.Red.Offset              = 16;
        Info.Green.Offset            = 8;
        Info.Blue.Offset             = 0;
        Info.Transp.Offset           = 24;
        Info.Red.Length = Info.Green.Length = Info.Blue.Length = Info.Transp.Length = 8;
        Info.Height = Info.Width = UINT32_MAX;

        return VMMCopyToUser(Task->Process->AddressSpace, Argument, &Info, sizeof(Info)) ? 0 : -EFAULT;
    }

    if (Request == FBIOGET_FSCREENINFO)
    {
        FB_FIX_SCREENINFO Info = {0};

        KMemCpy(Info.ID, "LineOS VirtIO", 13);
        Info.SmemLen    = GPU->FrameBufferWidth * GPU->FrameBufferHeight * sizeof(UINT32);
        Info.LineLength = GPU->FrameBufferWidth * sizeof(UINT32);
        Info.Visual     = 2;

        return VMMCopyToUser(Task->Process->AddressSpace, Argument, &Info, sizeof(Info)) ? 0 : -EFAULT;
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

        if (!VMMCopyFromUser(Task->Process->AddressSpace, &Present, Argument, sizeof(Present)))
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

STATIC INT64 DevicePointerIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access);

STATIC INT64 DeviceMasterIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    (VOID) Task;
    (VOID) State;
    (VOID) Access;

    if (Request == LOS_TIO_GET_COMPOSITOR)
    {
        return TerminalCompositor(Task, Argument, FALSE);
    }

    if (Request == TIOCGWINSZ || Request == TIOCSWINSZ)
    {
        return TerminalWindowSize(Task, Argument, Request == TIOCSWINSZ);
    }

    if (Request == FIONREAD)
    {
        return TerminalPending(Task, TRUE, Argument);
    }

    if (Request == LOS_TIO_GET_POINTER)
    {
        return DevicePointerIoctl(Task, State, Request, Argument, Access);
    }

    if (Request != LOS_TIO_WAIT_READ)
    {
        return -ENOTTY;
    }

    if (Argument > 1000)
    {
        return -EINVAL;
    }

    return TerminalWaitOutput((UINT32) Argument);
}

STATIC INT64 DeviceTerminalOpen(TASK *Task, UINT64 Flags, VOID **State)
{
    (VOID) Flags;
    *State = Task;

    return 0;
}

STATIC INT64 DeviceTerminalClose(VOID *State)
{
    TerminalGraphicsRelease(State);

    return 0;
}

STATIC INT64 DevicePointerIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    (VOID) State;

    if (Request != LOS_TIO_GET_POINTER)
    {
        return -ENOTTY;
    }

    if ((Access & 1U) == 0)
    {
        return -EACCES;
    }

    MOUSE_STATUS Status = GetMouseStatus();
    POINTER_INFO Info   = {Status.X, Status.Y, 0, Status.Wheel};

    if (Status.LeftButton)
    {
        Info.Buttons |= 1;
    }

    if (Status.RightButton)
    {
        Info.Buttons |= 2;
    }

    if (Status.MiddleButton)
    {
        Info.Buttons |= 4;
    }

    return VMMCopyToUser(Task->Process->AddressSpace, Argument, &Info, sizeof(Info)) ? 0 : -EFAULT;
}

STATIC INT64 DeviceTerminalIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    if (Request == LOS_TIO_INJECT_CLI)
    {
        return TerminalInjectCLI(Task, Argument);
    }

    if (Request == LOS_TIO_SET_COMPOSITOR)
    {
        return TerminalCompositor(Task, Argument, TRUE);
    }

    if (Request == TIOCGWINSZ)
    {
        return TerminalWindowSize(Task, Argument, FALSE);
    }

    if (Request == FIONREAD)
    {
        if ((Access & 1U) == 0)
        {
            return -EACCES;
        }

        return TerminalPending(Task, FALSE, Argument);
    }

    if (Request == LOS_TIO_GRAPHICS_BEGIN || Request == LOS_TIO_GRAPHICS_END)
    {
        if (State != Task || (Access & 2U) == 0)
        {
            return -EACCES;
        }

        return TerminalGraphics(Task, Request == LOS_TIO_GRAPHICS_BEGIN);
    }

    return DevicePointerIoctl(Task, State, Request, Argument, Access);
}

typedef struct
{
    UINT32  ResourceId;
    UINT32  Size;
    BOOLEAN ColorTarget;
    UINT32  Width;
    UINT32  Height;
    VOID   *Backing;
} VIRGL_FILE_BUFFER;

typedef struct
{
    UINT32            ContextId;
    UINT32            BufferBusy;
    VIRGL_FILE_BUFFER Buffers[LOS_VIRGL_BUFFER_LIMIT];
} VIRGL_FILE_STATE;

STATIC UINT64 NextVirGLResourceId = 0x80000000ULL;
STATIC UINT64 NextVirGLContextId  = 0x80000000ULL;

STATIC INT64 VirGLDeviceOpen(TASK *Task, UINT64 Flags, VOID **State)
{
    if (VirtIOGPUGetCapsetCount() == 0)
    {
        return -ENODEV;
    }

    (VOID) Task;
    (VOID) Flags;

    VIRGL_FILE_STATE *File = KAlloc(sizeof(*File));

    if (File == NULL)
    {
        return -ENOMEM;
    }

    KMemSet(File, 0, sizeof(*File));
    *State = File;

    return 0;
}

STATIC INT64 VirGLDeviceClose(VOID *State)
{
    VIRGL_FILE_STATE *File   = State;
    INT64             Result = 0;

    if (File != NULL)
    {
        if (File->ContextId != 0 && !VirGLDestroyContext(File->ContextId))
        {
            Result = -EIO;
        }

        for (UINT32 Index = 0; Index < LOS_VIRGL_BUFFER_LIMIT; Index++)
        {
            VIRGL_FILE_BUFFER *Buffer = &File->Buffers[Index];

            if (Buffer->ResourceId != 0)
            {
                if (!VirGLDestroyResource(Buffer->ResourceId))
                {
                    Result = -EIO;
                }
                else if (Buffer->Backing != NULL)
                {
                    KMemFreePages(Buffer->Backing, ((UINT64) Buffer->Width * Buffer->Height * 4 + 4095) / 4096);
                }
            }
        }

        KFree(File);
    }

    return Result;
}

STATIC INT64 VirGLDeviceCreateContext(VIRGL_FILE_STATE *File)
{
    UINT32 Expected = 0;

    if (!ATOMIC_COMPARE_EXCHANGE(&File->ContextId, &Expected, 1, FALSE, ATOMIC_ACQ_REL, ATOMIC_ACQUIRE))
    {
        return -EBUSY;
    }

    UINT64 Id = ATOMIC_FETCH_ADD(&NextVirGLContextId, 1, ATOMIC_RELAXED);

    if (Id > 0xFFFFFFFFULL || !VirGLCreateContext((UINT32) Id, NULL))
    {
        ATOMIC_STORE(&File->ContextId, 0, ATOMIC_RELEASE);

        return -EIO;
    }

    ATOMIC_STORE(&File->ContextId, (UINT32) Id, ATOMIC_RELEASE);

    return 0;
}

STATIC INT64 VirGLDeviceCreateBuffer(TASK *Task, VIRGL_FILE_STATE *File, UINT32 ContextId, UINT64 Argument, BOOLEAN ColorTarget)
{
    LOS_VIRGL_BUFFER_CREATE Query  = {0};
    LOS_VIRGL_TARGET_CREATE Target = {0};

    if (!VMMCopyFromUser(Task->Process->AddressSpace, ColorTarget ? (VOID *) &Target : (VOID *) &Query, Argument, ColorTarget ? sizeof(Target) : sizeof(Query)))
    {
        return -EFAULT;
    }

    if (ColorTarget ? (Target.Width == 0 || Target.Height == 0 || Target.Width > 4096 || Target.Height > 4096) : (Query.Size == 0 || Query.Size > LOS_VIRGL_MAX_BUFFER_BYTES))
    {
        return -EINVAL;
    }

    for (UINT32 Index = 0; Index < LOS_VIRGL_BUFFER_LIMIT; Index++)
    {
        VIRGL_FILE_BUFFER *Buffer = &File->Buffers[Index];

        if (Buffer->ResourceId != 0)
        {
            continue;
        }

        UINT64 Id = ATOMIC_FETCH_ADD(&NextVirGLResourceId, 1, ATOMIC_RELAXED);

        if (Id > 0xFFFFFFFFULL || !(ColorTarget ? VirGLCreateRenderTarget((UINT32) Id, Target.Width, Target.Height) : VirGLCreateVertexBuffer((UINT32) Id, Query.Size)))
        {
            return -EIO;
        }

        Buffer->ResourceId  = (UINT32) Id;
        Buffer->Size        = Query.Size;
        Buffer->ColorTarget = ColorTarget;
        Buffer->Width       = Target.Width;
        Buffer->Height      = Target.Height;
        BOOLEAN Attached    = VirGLAttachResource(ContextId, Buffer->ResourceId);
        Query.Handle        = Buffer->ResourceId;
        Target.Handle       = Buffer->ResourceId;

        if (Attached && VMMCopyToUser(Task->Process->AddressSpace, Argument, ColorTarget ? (VOID *) &Target : (VOID *) &Query, ColorTarget ? sizeof(Target) : sizeof(Query)))
        {
            return 0;
        }

        if (Attached)
        {
            VirGLDetachResource(ContextId, Buffer->ResourceId);
        }

        if (VirGLDestroyResource(Buffer->ResourceId))
        {
            Buffer->ResourceId = 0;
        }

        return Attached ? -EFAULT : -EIO;
    }

    return -ENOSPC;
}

STATIC INT64 VirGLDeviceBufferIoctl(TASK *Task, VIRGL_FILE_STATE *File, UINT64 Request, UINT64 Argument)
{
    UINT32                 ContextId = ATOMIC_LOAD(&File->ContextId, ATOMIC_ACQUIRE);
    LOS_VIRGL_BUFFER_WRITE Query     = {0};

    if (ContextId < 0x80000000U)
    {
        return -EINVAL;
    }

    if (Request == LOS_VIRGL_CREATE_BUFFER || Request == LOS_VIRGL_CREATE_TARGET)
    {
        return VirGLDeviceCreateBuffer(Task, File, ContextId, Argument, Request == LOS_VIRGL_CREATE_TARGET);
    }

    UINTN Size = Request == LOS_VIRGL_DESTROY_BUFFER ? sizeof(Query.Handle) : sizeof(Query);

    if (!VMMCopyFromUser(Task->Process->AddressSpace, &Query, Argument, Size))
    {
        return -EFAULT;
    }

    for (UINT32 Index = 0; Index < LOS_VIRGL_BUFFER_LIMIT; Index++)
    {
        VIRGL_FILE_BUFFER *Buffer = &File->Buffers[Index];

        if (Buffer->ResourceId == 0 || Buffer->ResourceId != Query.Handle)
        {
            continue;
        }

        if (Request == LOS_VIRGL_DESTROY_BUFFER)
        {
            VirGLDetachResource(ContextId, Buffer->ResourceId);

            if (!VirGLDestroyResource(Buffer->ResourceId))
            {
                return -EIO;
            }

            if (Buffer->Backing != NULL)
            {
                KMemFreePages(Buffer->Backing, ((UINT64) Buffer->Width * Buffer->Height * 4 + 4095) / 4096);
            }

            KMemSet(Buffer, 0, sizeof(*Buffer));

            return 0;
        }

        if (Buffer->ColorTarget || Query.Reserved != 0 || Query.Size == 0 || Query.Size > LOS_VIRGL_MAX_UPLOAD_BYTES || Query.Offset > Buffer->Size || Query.Size > Buffer->Size - Query.Offset)
        {
            return -EINVAL;
        }

        VOID *Data = KAlloc(Query.Size);

        if (Data == NULL)
        {
            return -ENOMEM;
        }

        INT64 Result = -EFAULT;

        if (VMMCopyFromUser(Task->Process->AddressSpace, Data, Query.Address, Query.Size))
        {
            Result = VirGLWriteVertexBuffer(ContextId, Buffer->ResourceId, Query.Offset, Data, Query.Size) ? 0 : -EIO;
        }

        KFree(Data);

        return Result;
    }

    return -ENOENT;
}

STATIC INT64 VirGLDeviceSubmit(TASK *Task, VIRGL_FILE_STATE *File, UINT64 Argument)
{
    LOS_VIRGL_SUBMIT Request;
    UINT32           ContextId = ATOMIC_LOAD(&File->ContextId, ATOMIC_ACQUIRE);

    if (ContextId < 0x80000000U)
    {
        return -EINVAL;
    }

    if (!VMMCopyFromUser(Task->Process->AddressSpace, &Request, Argument, sizeof(Request)))
    {
        return -EFAULT;
    }

    if (Request.Reserved != 0 || Request.Words == 0 || Request.Words > LOS_VIRGL_MAX_COMMAND_WORDS)
    {
        return -EINVAL;
    }

    UINT32  Size     = Request.Words * sizeof(UINT32);
    UINT32 *Commands = KAlloc(Size);

    if (Commands == NULL)
    {
        return -ENOMEM;
    }

    INT64 Result = -EFAULT;

    if (VMMCopyFromUser(Task->Process->AddressSpace, Commands, Request.Address, Size))
    {
        Result = -EOPNOTSUPP;

        if (Request.Words == 19 && Commands[0] == 0x00050801U && Commands[1] != 0 && Commands[3] == VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM && Commands[4] == 0 && Commands[5] == 0 && Commands[6] == 0x00030005U && Commands[7] == 1 && Commands[8] == 0 && Commands[9] == Commands[1] && Commands[10] == 0x00080007U && Commands[11] == 4 && Commands[16] == 0 && Commands[17] == 0 && Commands[18] == 0)
        {
            Result = -ENOENT;

            for (UINT32 Index = 0; Index < LOS_VIRGL_BUFFER_LIMIT; Index++)
            {
                VIRGL_FILE_BUFFER *Buffer = &File->Buffers[Index];

                if (Buffer->ResourceId != 0 && Buffer->ColorTarget && Buffer->ResourceId == Commands[2])
                {
                    Result = VirGLClearRenderTarget(ContextId, Buffer->ResourceId, Commands[1], Commands + 12) ? 0 : -EIO;
                    break;
                }
            }
        }
        else if (Request.Words == 15 && Commands[0] == 0x00020000U && Commands[3] == 0x000C0008U && Commands[4] == 0 && Commands[5] == 3 && Commands[6] == 4 && Commands[7] == 0 && Commands[8] == 1 && Commands[9] == 0 && Commands[10] == 0 && Commands[11] == 0 && Commands[12] == 0 && Commands[13] == 0 && Commands[14] == 2)
        {
            VIRGL_FILE_BUFFER *Target   = NULL;
            VIRGL_FILE_BUFFER *Vertices = NULL;

            for (UINT32 Index = 0; Index < LOS_VIRGL_BUFFER_LIMIT; Index++)
            {
                VIRGL_FILE_BUFFER *Buffer = &File->Buffers[Index];

                if (Buffer->ColorTarget && Buffer->ResourceId == Commands[1] && Buffer->ResourceId != 0)
                {
                    Target = Buffer;
                }

                if (!Buffer->ColorTarget && Buffer->ResourceId == Commands[2] && Buffer->ResourceId != 0 && Buffer->Size >= 48)
                {
                    Vertices = Buffer;
                }
            }

            Result = -ENOENT;

            if (Target != NULL && Vertices != NULL)
            {
                Result = VirGLDrawPositionTriangle(ContextId, Target->ResourceId, Vertices->ResourceId, Target->Width, Target->Height) ? 0 : -EIO;
            }
        }
        else
        {
            Result = 0;

            for (UINT32 Index = 0; Index < Request.Words; Index++)
            {
                if (Commands[Index] != 0)
                {
                    Result = -EOPNOTSUPP;
                    break;
                }
            }

            if (Result == 0 && !VirGLSubmit(ContextId, Commands, Size))
            {
                Result = -EIO;
            }
        }
    }

    KFree(Commands);

    return Result;
}

STATIC INT64 VirGLDeviceReadTarget(TASK *Task, VIRGL_FILE_STATE *File, UINT64 Argument)
{
    LOS_VIRGL_TARGET_READ Request;

    if (!VMMCopyFromUser(Task->Process->AddressSpace, &Request, Argument, sizeof(Request)))
    {
        return -EFAULT;
    }

    for (UINT32 Index = 0; Index < LOS_VIRGL_BUFFER_LIMIT; Index++)
    {
        VIRGL_FILE_BUFFER *Buffer = &File->Buffers[Index];

        if (!Buffer->ColorTarget || Buffer->ResourceId == 0 || Buffer->ResourceId != Request.Handle)
        {
            continue;
        }

        UINT32 Size  = Buffer->Width * Buffer->Height * 4;
        UINTN  Pages = (Size + 4095) / 4096;

        if (Request.Capacity < Size || Buffer->Backing != NULL)
        {
            return -EINVAL;
        }

        Buffer->Backing = KAllocPagesBelow(Pages, 0x100000000ULL);

        if (Buffer->Backing == NULL)
        {
            return -ENOMEM;
        }

        BOOLEAN CanFree;
        UINT32  ContextId = ATOMIC_LOAD(&File->ContextId, ATOMIC_ACQUIRE);
        BOOLEAN Success   = VirGLReadColorTarget(ContextId, Buffer->ResourceId, Buffer->Width, Buffer->Height, Buffer->Backing, &CanFree);
        INT64   Result    = Success ? (VMMCopyToUser(Task->Process->AddressSpace, Request.Address, Buffer->Backing, Size) ? 0 : -EFAULT) : -EIO;

        if (CanFree)
        {
            KMemFreePages(Buffer->Backing, Pages);
            Buffer->Backing = NULL;
        }

        return Result;
    }

    return -ENOENT;
}

STATIC INT64 VirGLDeviceIoctl(TASK *Task, VOID *State, UINT64 Request, UINT64 Argument, UINT32 Access)
{
    VIRGL_CAPSET_INFO Info;

    (VOID) Access;

    if (Request == LOS_VIRGL_CREATE_CONTEXT)
    {
        return VirGLDeviceCreateContext(State);
    }

    if (Request == LOS_VIRGL_CREATE_BUFFER || Request == LOS_VIRGL_WRITE_BUFFER || Request == LOS_VIRGL_DESTROY_BUFFER || Request == LOS_VIRGL_CREATE_TARGET || Request == LOS_VIRGL_SUBMIT_COMMANDS || Request == LOS_VIRGL_READ_TARGET)
    {
        VIRGL_FILE_STATE *File = State;

        if (ATOMIC_EXCHANGE(&File->BufferBusy, 1, ATOMIC_ACQUIRE) != 0)
        {
            return -EBUSY;
        }

        INT64 Result = Request == LOS_VIRGL_SUBMIT_COMMANDS ? VirGLDeviceSubmit(Task, File, Argument) : Request == LOS_VIRGL_READ_TARGET ? VirGLDeviceReadTarget(Task, File, Argument) : VirGLDeviceBufferIoctl(Task, File, Request, Argument);
        ATOMIC_STORE(&File->BufferBusy, 0, ATOMIC_RELEASE);

        return Result;
    }

    if (Request == LOS_VIRGL_GET_CAPSET_INFO)
    {
        LOS_VIRGL_CAPSET_INFO Query;

        if (!VMMCopyFromUser(Task->Process->AddressSpace, &Query, Argument, sizeof(Query)))
        {
            return -EFAULT;
        }

        if (!VirGLGetCapsetInfo(Query.Id, &Info))
        {
            return -EINVAL;
        }

        Query.MaxVersion = Info.MaxVersion;
        Query.MaxSize    = Info.MaxSize;

        return VMMCopyToUser(Task->Process->AddressSpace, Argument, &Query, sizeof(Query)) ? 0 : -EFAULT;
    }

    if (Request == LOS_VIRGL_READ_CAPSET)
    {
        LOS_VIRGL_CAPSET_READ Query;
        INT64                 Result = -EIO;

        if (!VMMCopyFromUser(Task->Process->AddressSpace, &Query, Argument, sizeof(Query)))
        {
            return -EFAULT;
        }

        if (!VirGLGetCapsetInfo(Query.Id, &Info) || Query.Version > Info.MaxVersion || Info.MaxSize > LOS_VIRGL_MAX_CAPSET_BYTES || Query.Capacity < Info.MaxSize)
        {
            return -EINVAL;
        }

        VOID *Data = KAlloc(Info.MaxSize);

        if (Data == NULL)
        {
            return -ENOMEM;
        }

        if (VirGLReadCapset(Query.Id, Query.Version, Data, Info.MaxSize, &Query.BytesRead))
        {
            Result = VMMCopyToUser(Task->Process->AddressSpace, Query.Address, Data, Query.BytesRead) && VMMCopyToUser(Task->Process->AddressSpace, Argument, &Query, sizeof(Query)) ? 0 : -EFAULT;
        }

        KFree(Data);

        return Result;
    }

    return -ENOTTY;
}

STATIC CONST DEVICE_OPERATIONS VirGLOperations       = {VirGLDeviceOpen, NULL, NULL, VirGLDeviceClose, NULL, VirGLDeviceIoctl};
STATIC CONST DEVICE_OPERATIONS NullOperations        = {DeviceOpen, DeviceNullRead, DeviceDiscard, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS ZeroOperations        = {DeviceOpen, DeviceZeroRead, DeviceDiscard, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS FullOperations        = {DeviceOpen, DeviceZeroRead, DeviceFullWrite, DeviceClose, NULL, DeviceNoIoctl};
STATIC CONST DEVICE_OPERATIONS TerminalOperations    = {DeviceTerminalOpen, DeviceTerminalRead, DeviceTerminalWrite, DeviceTerminalClose, NULL, DeviceTerminalIoctl};
STATIC CONST DEVICE_OPERATIONS MasterOperations      = {DeviceMasterOpen, DeviceMasterRead, DeviceMasterWrite, DeviceMasterClose, NULL, DeviceMasterIoctl};
STATIC CONST DEVICE_OPERATIONS FramebufferOperations = {FramebufferOpen, FramebufferRead, FramebufferWrite, DeviceClose, FramebufferMap, FramebufferIoctl};
STATIC CONST DEVICE_NODE       DeviceNodes[]         = {
    {(CONST CHAR8 *) "null",        0666, FALSE, &NullOperations       },
    {(CONST CHAR8 *) "zero",        0666, FALSE, &ZeroOperations       },
    {(CONST CHAR8 *) "full",        0666, FALSE, &FullOperations       },
    {(CONST CHAR8 *) "lsh1",        0666, FALSE, &TerminalOperations   },
    {(CONST CHAR8 *) "lsh1-master", 0600, FALSE, &MasterOperations     },
    {(CONST CHAR8 *) "fb0",         0666, TRUE,  &FramebufferOperations},
    {(CONST CHAR8 *) "virgl0",      0444, FALSE, &VirGLOperations      }
};

CONST DEVICE_NODE *DeviceFSGetNode(UINT32 Number)
{
    return Number >= 2 && Number - 2 < sizeof(DeviceNodes) / sizeof(DeviceNodes[0]) ? &DeviceNodes[Number - 2] : NULL;
}

STATIC VOID DeviceFSNode(UINT32 Number, VFS_NODE *Node)
{
    KMemSet(Node, 0, sizeof(*Node));
    Node->Number     = Number;
    Node->AccessTime = DeviceMountTime;
    Node->ModifyTime = DeviceMountTime;
    Node->ChangeTime = DeviceMountTime;
    Node->Mode       = Number == 1 ? VFS_MODE_DIRECTORY | 0755 : 0x2000U | DeviceFSGetNode(Number)->Mode;
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

    CONST CHAR8 *Name   = *Cursor == 0 ? (CONST CHAR8 *) "." : *Cursor == 1 ? (CONST CHAR8 *) ".." : DeviceNodes[*Cursor - 2].Name;
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
        DeviceMountTime           = (UINT32) ClockGetUnixTime();
        Registry->DevicePartition = Partition;
    }

    return Error;
}
