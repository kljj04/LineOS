// Kernel/Source/Syscall/Device/SyscallDisplay.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Syscall/Device/SyscallDisplay.h>
#include <Scheduler/LBPWRR.h>
#include <Scheduler/Task.h>
#include <Render/GPU/VirtIOGPU.h>
#include <Memory/VMM.h>
#include <Syscall/Errno.h>

INT64 SyscallDisplayInfo(UINT64 UserInfo)
{
    TASK            *Task;
    VIRTIO_GPU_INFO *GPU;
    DISPLAY_INFO     Info;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }

    Info.Width = GPU->FrameBufferWidth;
    Info.Height = GPU->FrameBufferHeight;
    Info.Pitch = Info.Width * sizeof(UINT32);
    Info.Format = DISPLAY_FORMAT_BGRA8888;

    if (!VMMCopyToUser(Task->AddressSpace, UserInfo, &Info, sizeof(Info)))
    {
        return -EFAULT;
    }

    return 0;
}

INT64 SyscallDisplayPresent(UINT64 UserBuffer, UINT64 BufferSize)
{
    TASK            *Task;
    VIRTIO_GPU_INFO *GPU;
    UINT64           RequiredSize;
    UINT64           Offset;
    UINT64           Chunk;
    BOOLEAN          Success = TRUE;
    UINT64           IRQFlags;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }

    RequiredSize = (UINT64) GPU->FrameBufferWidth * GPU->FrameBufferHeight * sizeof(UINT32);

    if (RequiredSize == 0 || BufferSize != RequiredSize)
    {
        return -EINVAL;
    }

    if (UserBuffer >= (1ULL << 47) || RequiredSize > (1ULL << 47) - UserBuffer)
    {
        return -EFAULT;
    }

    IRQFlags = VirtIOGPUAcquireRenderLock();

    for (Offset = 0; Offset < RequiredSize; Offset += Chunk)
    {
        Chunk = 4096 - ((UserBuffer + Offset) & 4095);

        if (Chunk > RequiredSize - Offset)
        {
            Chunk = RequiredSize - Offset;
        }

        if (!VMMCopyFromUser(Task->AddressSpace, (UINT8 *) GPU->FrameBuffer + Offset, UserBuffer + Offset, Chunk))
        {
            Success = FALSE;
            break;
        }
    }

    VirtIOGPUReleaseRenderLock(IRQFlags);

    if (Offset != 0)
    {
        VirtIOGPUMarkDirty(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);

        if (!VirtIOGPUPresent() && Success)
        {
            return -EIO;
        }
    }

    return Success ? 0 : -EFAULT;
}

INT64 SyscallDisplayPresentRect(UINT64 UserRect)
{
    TASK            *Task;
    VIRTIO_GPU_INFO *GPU;
    DISPLAY_RECT     Rect;
    UINT64           IRQFlags;
    UINT64           Row;
    UINT64           DestinationOffset;
    UINT64           SourceOffset;
    UINT64           RowSize;
    BOOLEAN          Success = TRUE;

    Task = LBPWRRGetCurrentTask();

    if (Task == NULL || Task->AddressSpace == NULL)
    {
        return -ESRCH;
    }

    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return -ENODEV;
    }

    if (!VMMCopyFromUser(Task->AddressSpace, &Rect, UserRect, sizeof(Rect)))
    {
        return -EFAULT;
    }

    if (Rect.Width == 0 || Rect.Height == 0 || Rect.X >= GPU->FrameBufferWidth || Rect.Y >= GPU->FrameBufferHeight)
    {
        return -EINVAL;
    }

    if (Rect.Width > GPU->FrameBufferWidth - Rect.X || Rect.Height > GPU->FrameBufferHeight - Rect.Y)
    {
        return -EINVAL;
    }

    RowSize = (UINT64) Rect.Width * sizeof(UINT32);

    if (Rect.Buffer >= (1ULL << 47) || (UINT64) Rect.Height * RowSize > (1ULL << 47) - Rect.Buffer)
    {
        return -EFAULT;
    }

    IRQFlags = VirtIOGPUAcquireRenderLock();

    for (Row = 0; Row < Rect.Height; Row++)
    {
        DestinationOffset = ((UINT64) (Rect.Y + Row) * GPU->FrameBufferWidth + Rect.X) * sizeof(UINT32);
        SourceOffset = Rect.Buffer + Row * RowSize;

        if (!VMMCopyFromUser(Task->AddressSpace, (UINT8 *) GPU->FrameBuffer + DestinationOffset, SourceOffset, RowSize))
        {
            Success = FALSE;
            break;
        }
    }

    VirtIOGPUReleaseRenderLock(IRQFlags);

    if (!Success)
    {
        return -EFAULT;
    }

    VirtIOGPUMarkDirty(Rect.X, Rect.Y, Rect.Width, Rect.Height);

    return VirtIOGPUPresent() ? 0 : -EIO;
}