// Kernel/Source/Render/GPU/VirtIOGPU.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Memory/PMM.h>
#include <Scheduler/WaitQueue.h>
#include <Timer/TSC.h>
#include <Arch/AMD64/CPU.h>
#include <PCI/PCI.h>
#include <Render/GPU/VirtIOGPUProtocol.h>
#include <Arch/AMD64/SpinLock.h>
#include <VirtIO/VirtIOPCI.h>
#include <Render/GPU/VirtIOGPU.h>

#include <VirtIO/VirtQueue.h>

STATIC VIRTIO_GPU_INFO VirtIOGPUInfo;
STATIC CONST CHAR16   *LastError = L"not initialized";
STATIC SPIN_LOCK       CommandLock;
STATIC SPIN_LOCK       RenderLock;
STATIC BOOLEAN         ControlQueueTimedOut;
STATIC UINT64          InterruptCount;
STATIC BOOLEAN         ControlInterruptEnabled;
STATIC WAIT_QUEUE      ControlWaiters;
STATIC UINT64          ControlDeadline;
STATIC BOOLEAN         CursorQueueTimedOut;

#define VIRTIO_GPU_RESOURCE_FRAMEBUFFER 1
#define VIRTIO_GPU_RESOURCE_CURSOR      2
#define VIRTIO_GPU_BYTES_PER_PIXEL      4
#define VIRTIO_GPU_QUEUE_TIMEOUT        100000000
#define VIRTIO_GPU_BOUNCE_BUFFER_SIZE   1024
#define VIRTIO_GPU_DMA_LIMIT            0x100000000ULL
#define PAGE_SIZE                       4096ULL

STATIC UINT8 ControlRequestBuffer[VIRTIO_GPU_BOUNCE_BUFFER_SIZE];
STATIC UINT8 ControlResponseBuffer[VIRTIO_GPU_BOUNCE_BUFFER_SIZE];
STATIC UINT8 CursorRequestBuffer[VIRTIO_GPU_BOUNCE_BUFFER_SIZE];
STATIC UINT8 CursorResponseBuffer[VIRTIO_GPU_BOUNCE_BUFFER_SIZE];

STATIC BOOLEAN SendCommandToQueue(VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 ResponseLength, UINT32 *ReplyLength, CONST CHAR16 *Stage, BOOLEAN CanSleep);
STATIC BOOLEAN SendNoDataCommandToQueue(VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage);
STATIC BOOLEAN SendCursorCommand(VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage);
STATIC BOOLEAN TransferResourceRect(UINT32 ResourceId, UINT32 FrameWidth, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);

STATIC UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC VOID ResetInfo(VOID)
{
    SpinLockInit(&CommandLock);
    WaitQueueInit(&ControlWaiters);
    ATOMIC_STORE(&ControlDeadline, 0, ATOMIC_RELEASE);
    ATOMIC_STORE(&ControlInterruptEnabled, FALSE, ATOMIC_RELEASE);
    SpinLockInit(&RenderLock);
    ControlQueueTimedOut = FALSE;
    CursorQueueTimedOut  = FALSE;
    KMemSet(&VirtIOGPUInfo, 0, sizeof(VirtIOGPUInfo));
    VirtIOGPUInfo.Debug.LastStage = L"reset";
}

STATIC VOID SetCommandDebug(CONST CHAR16 *Stage, UINT32 Command)
{
    VirtIOGPUInfo.Debug.LastStage    = Stage;
    VirtIOGPUInfo.Debug.LastCommand  = Command;
    VirtIOGPUInfo.Debug.LastResponse = 0;
}

STATIC BOOLEAN GetDisplayInfo(VOID)
{
    VIRTIO_GPU_GET_DISPLAY_INFO_REQUEST Request;

    KMemSet(&Request, 0, sizeof(Request));
    KMemSet(&VirtIOGPUInfo.DisplayInfo, 0, sizeof(VirtIOGPUInfo.DisplayInfo));
    Request.Header.Type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    SetCommandDebug(L"display info", Request.Header.Type);

    if (!SendCommandToQueue(&VirtIOGPUInfo.ControlQueue, &Request, sizeof(Request), &VirtIOGPUInfo.DisplayInfo, sizeof(VirtIOGPUInfo.DisplayInfo), NULL, L"display info", FALSE))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.LastResponse = VirtIOGPUInfo.DisplayInfo.Header.Type;

    if (VirtIOGPUInfo.DisplayInfo.Header.Type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
    {
        LastError = L"gpu display info rejected";

        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN SendNoDataCommand(VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage)
{
    return SendNoDataCommandToQueue(&VirtIOGPUInfo.ControlQueue, Request, RequestLength, Stage);
}

STATIC BOOLEAN SendNoDataCommandToQueue(VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage)
{
    VIRTIO_GPU_CTRL_HEADER Response;

    KMemSet(&Response, 0, sizeof(Response));

    if (!SendCommandToQueue(Queue, Request, RequestLength, &Response, sizeof(Response), NULL, Stage, FALSE))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.LastResponse = Response.Type;

    if (Response.Type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LastError = L"gpu command rejected";

        return FALSE;
    }

    return TRUE;
}

STATIC BOOLEAN SendCommandToQueue(VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, VOID *Response, UINT32 ResponseLength, UINT32 *ReplyLength, CONST CHAR16 *Stage, BOOLEAN CanSleep)
{
    VIRTIO_GPU_CTRL_HEADER *RequestHeader;
    VIRTQUEUE_BUFFER        Buffers[2];
    UINT8                  *RequestBuffer;
    UINT8                  *ResponseBuffer;
    UINT16                  Head;
    UINT32                  UsedLength;
    UINT64                  Flags;
    UINT64                  ResponseOffset = AlignUp(RequestLength, PAGE_SIZE);
    UINTN                   Pages          = 0;
    VOID                   *Allocation     = NULL;

    if (Queue == NULL || Request == NULL || RequestLength < sizeof(VIRTIO_GPU_CTRL_HEADER) || Response == NULL || ResponseLength == 0)
    {
        LastError = L"gpu command args invalid";

        return FALSE;
    }

    if (Queue == &VirtIOGPUInfo.CursorQueue && (RequestLength > VIRTIO_GPU_BOUNCE_BUFFER_SIZE || ResponseLength > VIRTIO_GPU_BOUNCE_BUFFER_SIZE))
    {
        LastError = L"gpu command too large";

        return FALSE;
    }

    BOOLEAN Sleep = CanSleep && Queue == &VirtIOGPUInfo.ControlQueue && ATOMIC_LOAD(&ControlInterruptEnabled, ATOMIC_ACQUIRE) && TSCGetFrequency() != 0;
    BOOLEAN Completed;

    Flags = SpinLockAcquireIRQSave(&CommandLock);

    if ((Queue == &VirtIOGPUInfo.ControlQueue && ControlQueueTimedOut) || (Queue == &VirtIOGPUInfo.CursorQueue && CursorQueueTimedOut))
    {
        LastError = L"gpu queue unavailable after timeout";
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    Sleep = Sleep && ATOMIC_LOAD(&ControlDeadline, ATOMIC_RELAXED) == 0;

    if (Queue == &VirtIOGPUInfo.CursorQueue)
    {
        RequestBuffer  = CursorRequestBuffer;
        ResponseBuffer = CursorResponseBuffer;
    }
    else if (Sleep || RequestLength > sizeof(ControlRequestBuffer) || ResponseLength > sizeof(ControlResponseBuffer))
    {
        Pages      = (UINTN) (AlignUp(ResponseOffset + ResponseLength, PAGE_SIZE) / PAGE_SIZE);
        Allocation = KAllocPagesBelow(Pages, VIRTIO_GPU_DMA_LIMIT);

        if (Allocation == NULL)
        {
            LastError = L"gpu command DMA allocation failed";
            SpinLockReleaseIRQRestore(&CommandLock, Flags);

            return FALSE;
        }

        RequestBuffer  = Allocation;
        ResponseBuffer = RequestBuffer + ResponseOffset;
    }
    else
    {
        RequestBuffer  = ControlRequestBuffer;
        ResponseBuffer = ControlResponseBuffer;
    }

    KMemCpy(RequestBuffer, Request, RequestLength);
    KMemSet(ResponseBuffer, 0, ResponseLength);

    RequestHeader = (VIRTIO_GPU_CTRL_HEADER *) Request;
    SetCommandDebug(Stage, RequestHeader->Type);

    Buffers[0].Buffer = RequestBuffer;
    Buffers[0].Length = RequestLength;
    Buffers[0].Flags  = 0;
    Buffers[1].Buffer = ResponseBuffer;
    Buffers[1].Length = ResponseLength;
    Buffers[1].Flags  = VIRTQ_DESC_F_WRITE;

    if (!VirtQueueBuildChain(Queue, Buffers, 2, &Head))
    {
        LastError = VirtQueueGetLastError();
        if (Allocation != NULL)
        {
            KMemFreePages(Allocation, Pages);
        }

        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    if (!VirtQueueSubmit(&VirtIOGPUInfo.Device, Queue, Head))
    {
        LastError = VirtQueueGetLastError();
        VirtQueueFreeChain(Queue, Head);
        if (Allocation != NULL)
        {
            KMemFreePages(Allocation, Pages);
        }

        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    if (Sleep)
    {
        ATOMIC_STORE(&ControlDeadline, RDTSC() + TSCGetFrequency(), ATOMIC_RELEASE);

        while (!(Completed = VirtQueueTryWaitUsed(Queue, Head, &UsedLength)) && !ControlQueueTimedOut)
        {
            if ((INT64) (RDTSC() - ATOMIC_LOAD(&ControlDeadline, ATOMIC_RELAXED)) >= 0)
            {
                break;
            }

            if (!WaitQueueSleep(&ControlWaiters, &CommandLock))
            {
                Completed = VirtQueueWaitUsed(Queue, Head, &UsedLength, VIRTIO_GPU_QUEUE_TIMEOUT);
                break;
            }
        }

        ATOMIC_STORE(&ControlDeadline, 0, ATOMIC_RELEASE);
    }
    else
    {
        Completed = VirtQueueWaitUsed(Queue, Head, &UsedLength, VIRTIO_GPU_QUEUE_TIMEOUT);
    }

    if (!Completed)
    {
        LastError = L"gpu command timeout";

        if (Queue == &VirtIOGPUInfo.CursorQueue)
        {
            CursorQueueTimedOut = TRUE;
        }
        else
        {
            ControlQueueTimedOut = TRUE;
        }

        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    VirtQueueFreeChain(Queue, Head);
    KMemCpy(Response, ResponseBuffer, ResponseLength);
    if (ReplyLength != NULL)
    {
        *ReplyLength = UsedLength;
    }

    if (Allocation != NULL)
    {
        KMemFreePages(Allocation, Pages);
    }

    LastError = L"ok";
    SpinLockReleaseIRQRestore(&CommandLock, Flags);

    return TRUE;
}

STATIC BOOLEAN SendCursorCommand(VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage)
{
    VIRTIO_GPU_CTRL_HEADER *RequestHeader;
    VIRTQUEUE_BUFFER        Buffers[2];
    UINT16                  Head;
    UINT32                  UsedLength;
    UINT64                  Flags;

    if (Request == NULL || RequestLength == 0)
    {
        LastError = L"cursor command args invalid";

        return FALSE;
    }

    if (RequestLength > VIRTIO_GPU_BOUNCE_BUFFER_SIZE)
    {
        LastError = L"cursor command too large";

        return FALSE;
    }

    Flags = SpinLockAcquireIRQSave(&CommandLock);

    if (CursorQueueTimedOut)
    {
        LastError = L"gpu queue unavailable after timeout";
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    KMemCpy(CursorRequestBuffer, Request, RequestLength);
    KMemSet(CursorResponseBuffer, 0, sizeof(VIRTIO_GPU_CTRL_HEADER));

    RequestHeader = (VIRTIO_GPU_CTRL_HEADER *) Request;
    SetCommandDebug(Stage, RequestHeader->Type);

    Buffers[0].Buffer = CursorRequestBuffer;
    Buffers[0].Length = RequestLength;
    Buffers[0].Flags  = 0;
    Buffers[1].Buffer = CursorResponseBuffer;
    Buffers[1].Length = sizeof(VIRTIO_GPU_CTRL_HEADER);
    Buffers[1].Flags  = VIRTQ_DESC_F_WRITE;

    if (!VirtQueueBuildChain(&VirtIOGPUInfo.CursorQueue, Buffers, 2, &Head))
    {
        LastError = VirtQueueGetLastError();
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    if (!VirtQueueSubmit(&VirtIOGPUInfo.Device, &VirtIOGPUInfo.CursorQueue, Head))
    {
        LastError = VirtQueueGetLastError();
        VirtQueueFreeChain(&VirtIOGPUInfo.CursorQueue, Head);
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    if (!VirtQueueWaitUsed(&VirtIOGPUInfo.CursorQueue, Head, &UsedLength, VIRTIO_GPU_QUEUE_TIMEOUT))
    {
        LastError           = VirtQueueGetLastError();
        CursorQueueTimedOut = TRUE;
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    VirtQueueFreeChain(&VirtIOGPUInfo.CursorQueue, Head);

    if (UsedLength == 0)
    {
        VirtIOGPUInfo.Debug.LastResponse = VIRTIO_GPU_RESP_OK_NODATA;
        LastError                        = L"ok";
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return TRUE;
    }

    if (UsedLength < sizeof(VIRTIO_GPU_CTRL_HEADER))
    {
        LastError = L"cursor response too small";
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    RequestHeader                    = (VIRTIO_GPU_CTRL_HEADER *) CursorResponseBuffer;
    VirtIOGPUInfo.Debug.LastResponse = RequestHeader->Type;

    if (RequestHeader->Type != VIRTIO_GPU_RESP_OK_NODATA)
    {
        LastError = L"cursor command rejected";
        SpinLockReleaseIRQRestore(&CommandLock, Flags);

        return FALSE;
    }

    LastError = L"ok";
    SpinLockReleaseIRQRestore(&CommandLock, Flags);

    return TRUE;
}

STATIC VOID SetRect(VIRTIO_GPU_RECT *Rect, UINT32 Width, UINT32 Height)
{
    Rect->X      = 0;
    Rect->Y      = 0;
    Rect->Width  = Width;
    Rect->Height = Height;
}

STATIC VOID SetRectAt(VIRTIO_GPU_RECT *Rect, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    Rect->X      = X;
    Rect->Y      = Y;
    Rect->Width  = Width;
    Rect->Height = Height;
}

STATIC BOOLEAN ClipRect(UINT32 *X, UINT32 *Y, UINT32 *Width, UINT32 *Height)
{
    UINT32 MaxX;
    UINT32 MaxY;

    if (VirtIOGPUInfo.FrameBuffer == NULL || *Width == 0 || *Height == 0 || *X >= VirtIOGPUInfo.FrameBufferWidth || *Y >= VirtIOGPUInfo.FrameBufferHeight)
    {
        return FALSE;
    }

    MaxX = *X + *Width;
    MaxY = *Y + *Height;

    if (MaxX > VirtIOGPUInfo.FrameBufferWidth || MaxX < *X)
    {
        MaxX = VirtIOGPUInfo.FrameBufferWidth;
    }

    if (MaxY > VirtIOGPUInfo.FrameBufferHeight || MaxY < *Y)
    {
        MaxY = VirtIOGPUInfo.FrameBufferHeight;
    }

    *Width  = MaxX - *X;
    *Height = MaxY - *Y;

    return *Width != 0 && *Height != 0;
}

STATIC BOOLEAN RectsTouchOrOverlap(VIRTIO_GPU_RECT *A, VIRTIO_GPU_RECT *B)
{
    UINT32 ARight  = A->X + A->Width;
    UINT32 ABottom = A->Y + A->Height;
    UINT32 BRight  = B->X + B->Width;
    UINT32 BBottom = B->Y + B->Height;

    return A->X <= BRight && B->X <= ARight && A->Y <= BBottom && B->Y <= ABottom;
}

STATIC VOID MergeRect(VIRTIO_GPU_RECT *Target, VIRTIO_GPU_RECT *Source)
{
    UINT32 Left         = Target->X < Source->X ? Target->X : Source->X;
    UINT32 Top          = Target->Y < Source->Y ? Target->Y : Source->Y;
    UINT32 TargetRight  = Target->X + Target->Width;
    UINT32 SourceRight  = Source->X + Source->Width;
    UINT32 TargetBottom = Target->Y + Target->Height;
    UINT32 SourceBottom = Source->Y + Source->Height;
    UINT32 Right        = TargetRight > SourceRight ? TargetRight : SourceRight;
    UINT32 Bottom       = TargetBottom > SourceBottom ? TargetBottom : SourceBottom;

    Target->X      = Left;
    Target->Y      = Top;
    Target->Width  = Right - Left;
    Target->Height = Bottom - Top;
}

BOOLEAN VirtIOGPUInit(VOID)
{
    PCI_DEVICE *PCIDevice;

    ResetInfo();

    PCIDevice = PCIFindDevice(VIRTIO_GPU_VENDOR_ID, VIRTIO_GPU_DEVICE_ID);

    if (PCIDevice == NULL)
    {
        return FALSE;
    }

    if (!VirtIOPCIInitDevice(&VirtIOGPUInfo.Device, PCIDevice))
    {
        return FALSE;
    }

    if (!VirtIOPCIStartDeviceWithFeatures(&VirtIOGPUInfo.Device, 1U << VIRTIO_GPU_F_VIRGL, 0))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.StartOK = TRUE;

    if (!VirtQueueInit(&VirtIOGPUInfo.Device, &VirtIOGPUInfo.ControlQueue, 0, 64))
    {
        LastError = VirtQueueGetLastError();

        return FALSE;
    }

    VirtIOGPUInfo.Debug.ControlQueueOK = TRUE;

    if (!VirtQueueInit(&VirtIOGPUInfo.Device, &VirtIOGPUInfo.CursorQueue, 1, 16))
    {
        LastError = VirtQueueGetLastError();

        return FALSE;
    }

    VirtIOGPUInfo.Debug.CursorQueueOK = TRUE;

    VirtIOPCIReadyDevice(&VirtIOGPUInfo.Device);

    if (!GetDisplayInfo())
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.DisplayInfoOK = TRUE;

    VirtIOGPUInfo.Found = TRUE;
    LastError           = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUEnableInterrupt(VOID)
{
    BOOLEAN Enabled = VirtIOGPUInfo.Found && VirtIOPCIEnableQueueMSIX(&VirtIOGPUInfo.Device, VirtIOGPUInfo.ControlQueue.Index, VIRTIO_GPU_IRQ_VECTOR);
    ATOMIC_STORE(&ControlInterruptEnabled, Enabled, ATOMIC_RELEASE);

    return Enabled;
}

VOID VirtIOGPUHandleInterrupt(VOID)
{
    ATOMIC_FETCH_ADD(&InterruptCount, 1, ATOMIC_RELAXED);
    VirtIOGPUCheckCompletion(TRUE);
}

VOID VirtIOGPUCheckCompletion(BOOLEAN Interrupt)
{
    if (!ATOMIC_LOAD(&ControlInterruptEnabled, ATOMIC_ACQUIRE) || ATOMIC_LOAD(&ControlDeadline, ATOMIC_ACQUIRE) == 0)
    {
        return;
    }

    UINT64 Flags = SpinLockAcquireIRQSave(&CommandLock);

    UINT64     Deadline = ATOMIC_LOAD(&ControlDeadline, ATOMIC_RELAXED);
    VIRTQUEUE *Queue    = &VirtIOGPUInfo.ControlQueue;

    if (Deadline != 0 && (Interrupt || (INT64) (RDTSC() - Deadline) >= 0 || ((CONST VOLATILE VIRTQ_USED *) Queue->Used)->Index != Queue->UsedIndex))
    {
        WaitQueueWakeAll(&ControlWaiters);
    }

    SpinLockReleaseIRQRestore(&CommandLock, Flags);
}

UINT64 VirtIOGPUGetInterruptCount(VOID)
{
    return ATOMIC_LOAD(&InterruptCount, ATOMIC_RELAXED);
}

BOOLEAN VirtIOGPUCreateFrameBuffer(UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_RESOURCE_CREATE_2D_REQUEST      CreateRequest;
    VIRTIO_GPU_RESOURCE_ATTACH_BACKING_REQUEST AttachRequest;
    UINT64                                     FrameBufferBytes;
    UINT64                                     FrameBufferPages;

    if (!VirtIOGPUInfo.Found)
    {
        LastError = L"virtio gpu not ready";

        return FALSE;
    }

    if (VirtIOGPUInfo.FrameBuffer != NULL || Width == 0 || Height == 0 || (UINT64) Width * Height > UINT32_MAX / VIRTIO_GPU_BYTES_PER_PIXEL)
    {
        LastError = L"gpu framebuffer size invalid";

        return FALSE;
    }

    FrameBufferBytes          = (UINT64) Width * Height * VIRTIO_GPU_BYTES_PER_PIXEL;
    FrameBufferPages          = AlignUp(FrameBufferBytes, PAGE_SIZE) / PAGE_SIZE;
    VirtIOGPUInfo.FrameBuffer = (UINT32 *) KAllocPagesBelow((UINTN) FrameBufferPages, VIRTIO_GPU_DMA_LIMIT);

    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        LastError = L"gpu framebuffer alloc failed";

        return FALSE;
    }

    VirtIOGPUInfo.FrameBufferWidth  = Width;
    VirtIOGPUInfo.FrameBufferHeight = Height;
    VirtIOGPUInfo.ResourceId        = VIRTIO_GPU_RESOURCE_FRAMEBUFFER;
    VirtIOGPUClearDirty();

    KMemSet(&CreateRequest, 0, sizeof(CreateRequest));
    CreateRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    CreateRequest.ResourceId  = VirtIOGPUInfo.ResourceId;
    CreateRequest.Format      = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    CreateRequest.Width       = Width;
    CreateRequest.Height      = Height;

    if (!SendNoDataCommand(&CreateRequest, sizeof(CreateRequest), L"create 2d"))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.CreateOK = TRUE;

    KMemSet(&AttachRequest, 0, sizeof(AttachRequest));
    AttachRequest.Header.Type   = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    AttachRequest.ResourceId    = VirtIOGPUInfo.ResourceId;
    AttachRequest.EntryCount    = 1;
    AttachRequest.Entry.Address = (UINT64) VirtIOGPUInfo.FrameBuffer;
    AttachRequest.Entry.Length  = (UINT32) FrameBufferBytes;

    if (!SendNoDataCommand(&AttachRequest, sizeof(AttachRequest), L"attach backing"))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.AttachOK = TRUE;

    if (!VirtIOGPUSetScanout())
    {
        return FALSE;
    }

    LastError = L"ok";

    return TRUE;
}

STATIC VOID MemoryFence(VOID)
{
    ASM("mfence" :: : "memory");
}

VOID VirtIOGPUMarkDirtyLocked(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_RECT Rect;

    if (!ClipRect(&X, &Y, &Width, &Height))
    {
        return;
    }

    Rect.X      = X;
    Rect.Y      = Y;
    Rect.Width  = Width;
    Rect.Height = Height;

    for (UINT32 Index = 0; Index < VirtIOGPUInfo.DirtyRectCount; Index++)
    {
        if (!RectsTouchOrOverlap(&VirtIOGPUInfo.DirtyRects[Index], &Rect))
        {
            continue;
        }

        MergeRect(&VirtIOGPUInfo.DirtyRects[Index], &Rect);

        for (UINT32 MergeIndex = 0; MergeIndex < VirtIOGPUInfo.DirtyRectCount; MergeIndex++)
        {
            if (MergeIndex == Index || !RectsTouchOrOverlap(&VirtIOGPUInfo.DirtyRects[Index], &VirtIOGPUInfo.DirtyRects[MergeIndex]))
            {
                continue;
            }

            MergeRect(&VirtIOGPUInfo.DirtyRects[Index], &VirtIOGPUInfo.DirtyRects[MergeIndex]);
            VirtIOGPUInfo.DirtyRects[MergeIndex] = VirtIOGPUInfo.DirtyRects[VirtIOGPUInfo.DirtyRectCount - 1];
            VirtIOGPUInfo.DirtyRectCount--;
            MergeIndex = 0;
        }

        VirtIOGPUInfo.Dirty = TRUE;

        return;
    }

    if (VirtIOGPUInfo.DirtyRectCount >= VIRTIO_GPU_MAX_DIRTY_RECTS)
    {
        VirtIOGPUInfo.DirtyRectCount = 1;
        SetRect(&VirtIOGPUInfo.DirtyRects[0], VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
        VirtIOGPUInfo.Dirty = TRUE;

        return;
    }

    VirtIOGPUInfo.DirtyRects[VirtIOGPUInfo.DirtyRectCount++] = Rect;
    VirtIOGPUInfo.Dirty                                      = TRUE;
}

VOID VirtIOGPUMarkDirty(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    UINT64 Flags;

    Flags = VirtIOGPUAcquireRenderLock();
    VirtIOGPUMarkDirtyLocked(X, Y, Width, Height);
    VirtIOGPUReleaseRenderLock(Flags);
}

VOID VirtIOGPUClearDirty(VOID)
{
    UINT64 Flags;

    Flags                        = VirtIOGPUAcquireRenderLock();
    VirtIOGPUInfo.Dirty          = FALSE;
    VirtIOGPUInfo.DirtyRectCount = 0;
    VirtIOGPUReleaseRenderLock(Flags);
}

UINT64 VirtIOGPUAcquireRenderLock(VOID)
{
    return SpinLockAcquireIRQSave(&RenderLock);
}

VOID VirtIOGPUReleaseRenderLock(UINT64 Flags)
{
    SpinLockReleaseIRQRestore(&RenderLock, Flags);
}

BOOLEAN VirtIOGPUSetScanout(VOID)
{
    VIRTIO_GPU_SET_SCANOUT_REQUEST ScanoutRequest;

    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        LastError = L"gpu framebuffer missing";

        return FALSE;
    }

    KMemSet(&ScanoutRequest, 0, sizeof(ScanoutRequest));
    ScanoutRequest.Header.Type = VIRTIO_GPU_CMD_SET_SCANOUT;
    SetRect(&ScanoutRequest.Rect, VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
    ScanoutRequest.ScanoutId  = 0;
    ScanoutRequest.ResourceId = VirtIOGPUInfo.ResourceId;

    if (!SendNoDataCommand(&ScanoutRequest, sizeof(ScanoutRequest), L"set scanout"))
    {
        return FALSE;
    }

    VirtIOGPUInfo.ScanoutSet         = TRUE;
    VirtIOGPUInfo.Debug.SetScanoutOK = TRUE;
    LastError                        = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUTransferRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    if (!ClipRect(&X, &Y, &Width, &Height))
    {
        return TRUE;
    }

    return TransferResourceRect(VirtIOGPUInfo.ResourceId, VirtIOGPUInfo.FrameBufferWidth, X, Y, Width, Height);
}

STATIC BOOLEAN TransferResourceRect(UINT32 ResourceId, UINT32 FrameWidth, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_TRANSFER_TO_HOST_2D_REQUEST TransferRequest;

    MemoryFence();

    KMemSet(&TransferRequest, 0, sizeof(TransferRequest));
    TransferRequest.Header.Type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    SetRectAt(&TransferRequest.Rect, X, Y, Width, Height);
    TransferRequest.Offset     = ((UINT64) Y * FrameWidth + X) * VIRTIO_GPU_BYTES_PER_PIXEL;
    TransferRequest.ResourceId = ResourceId;

    if (!SendNoDataCommand(&TransferRequest, sizeof(TransferRequest), L"transfer 2d"))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.TransferOK = TRUE;
    LastError                      = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUFlushRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_RESOURCE_FLUSH_REQUEST FlushRequest;

    if (!ClipRect(&X, &Y, &Width, &Height))
    {
        return TRUE;
    }

    KMemSet(&FlushRequest, 0, sizeof(FlushRequest));
    FlushRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    SetRectAt(&FlushRequest.Rect, X, Y, Width, Height);
    FlushRequest.ResourceId = VirtIOGPUInfo.ResourceId;

    if (!SendNoDataCommand(&FlushRequest, sizeof(FlushRequest), L"flush"))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.FlushOK = TRUE;
    LastError                   = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUPresent(VOID)
{
    UINT64 Flags;

    Flags = VirtIOGPUAcquireRenderLock();

    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        LastError = L"gpu framebuffer missing";
        VirtIOGPUReleaseRenderLock(Flags);

        return FALSE;
    }

    if (!VirtIOGPUInfo.ScanoutSet && !VirtIOGPUSetScanout())
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return FALSE;
    }

    if (!VirtIOGPUInfo.Dirty)
    {
        LastError = L"ok";
        VirtIOGPUReleaseRenderLock(Flags);

        return TRUE;
    }

    for (UINT32 Index = 0; Index < VirtIOGPUInfo.DirtyRectCount; Index++)
    {
        VIRTIO_GPU_RECT *Rect = &VirtIOGPUInfo.DirtyRects[Index];

        if (!VirtIOGPUTransferRect(Rect->X, Rect->Y, Rect->Width, Rect->Height))
        {
            VirtIOGPUReleaseRenderLock(Flags);

            return FALSE;
        }

        if (!VirtIOGPUFlushRect(Rect->X, Rect->Y, Rect->Width, Rect->Height))
        {
            VirtIOGPUReleaseRenderLock(Flags);

            return FALSE;
        }
    }

    VirtIOGPUInfo.Dirty          = FALSE;
    VirtIOGPUInfo.DirtyRectCount = 0;
    LastError                    = L"ok";
    VirtIOGPUReleaseRenderLock(Flags);

    return TRUE;
}

BOOLEAN VirtIOGPUFlush(VOID)
{
    return VirtIOGPUPresent();
}

BOOLEAN VirtIOGPUCreateCursor(UINT32 Width, UINT32 Height, UINT32 HotX, UINT32 HotY)
{
    VIRTIO_GPU_RESOURCE_CREATE_2D_REQUEST      CreateRequest;
    VIRTIO_GPU_RESOURCE_ATTACH_BACKING_REQUEST AttachRequest;
    UINT64                                     CursorBytes;
    UINT64                                     CursorPages;

    if (!VirtIOGPUInfo.Found || Width == 0 || Height == 0)
    {
        LastError = L"cursor args invalid";

        return FALSE;
    }

    CursorBytes                = (UINT64) Width * Height * VIRTIO_GPU_BYTES_PER_PIXEL;
    CursorPages                = AlignUp(CursorBytes, PAGE_SIZE) / PAGE_SIZE;
    VirtIOGPUInfo.CursorBuffer = (UINT32 *) KAllocPagesBelow((UINTN) CursorPages, VIRTIO_GPU_DMA_LIMIT);

    if (VirtIOGPUInfo.CursorBuffer == NULL)
    {
        LastError = L"cursor alloc failed";

        return FALSE;
    }

    VirtIOGPUInfo.CursorResourceId = VIRTIO_GPU_RESOURCE_CURSOR;
    VirtIOGPUInfo.CursorWidth      = Width;
    VirtIOGPUInfo.CursorHeight     = Height;
    VirtIOGPUInfo.CursorHotX       = HotX;
    VirtIOGPUInfo.CursorHotY       = HotY;

    KMemSet(VirtIOGPUInfo.CursorBuffer, 0, (UINTN) CursorBytes);

    KMemSet(&CreateRequest, 0, sizeof(CreateRequest));
    CreateRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    CreateRequest.ResourceId  = VirtIOGPUInfo.CursorResourceId;
    CreateRequest.Format      = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    CreateRequest.Width       = Width;
    CreateRequest.Height      = Height;

    if (!SendNoDataCommand(&CreateRequest, sizeof(CreateRequest), L"cursor create"))
    {
        return FALSE;
    }

    KMemSet(&AttachRequest, 0, sizeof(AttachRequest));
    AttachRequest.Header.Type   = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    AttachRequest.ResourceId    = VirtIOGPUInfo.CursorResourceId;
    AttachRequest.EntryCount    = 1;
    AttachRequest.Entry.Address = (UINT64) VirtIOGPUInfo.CursorBuffer;
    AttachRequest.Entry.Length  = (UINT32) CursorBytes;

    if (!SendNoDataCommand(&AttachRequest, sizeof(AttachRequest), L"cursor attach"))
    {
        return FALSE;
    }

    LastError = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUSetCursorImage(CONST UINT32 *Pixels, UINT32 Width, UINT32 Height, UINT32 HotX, UINT32 HotY, UINT32 X, UINT32 Y)
{
    UINT64 CursorBytes;

    if (VirtIOGPUInfo.CursorBuffer == NULL || VirtIOGPUInfo.CursorResourceId == 0 || Pixels == NULL)
    {
        LastError = L"cursor image args invalid";

        return FALSE;
    }

    if (Width == 0 || Height == 0 || Width > VirtIOGPUInfo.CursorWidth || Height > VirtIOGPUInfo.CursorHeight)
    {
        LastError = L"cursor image size invalid";

        return FALSE;
    }

    CursorBytes = (UINT64) VirtIOGPUInfo.CursorWidth * VirtIOGPUInfo.CursorHeight * VIRTIO_GPU_BYTES_PER_PIXEL;
    KMemSet(VirtIOGPUInfo.CursorBuffer, 0, (UINTN) CursorBytes);

    for (UINT32 Row = 0; Row < Height; Row++)
    {
        for (UINT32 Column = 0; Column < Width; Column++)
        {
            VirtIOGPUInfo.CursorBuffer[Row * VirtIOGPUInfo.CursorWidth + Column] = Pixels[Row * Width + Column];
        }
    }

    VirtIOGPUInfo.CursorHotX = HotX;
    VirtIOGPUInfo.CursorHotY = HotY;

    return VirtIOGPUUpdateCursor(X, Y);
}

BOOLEAN VirtIOGPUUpdateCursor(UINT32 X, UINT32 Y)
{
    VIRTIO_GPU_UPDATE_CURSOR_REQUEST Request;

    if (VirtIOGPUInfo.CursorBuffer == NULL || VirtIOGPUInfo.CursorResourceId == 0)
    {
        LastError = L"cursor missing";

        return FALSE;
    }

    if (!TransferResourceRect(VirtIOGPUInfo.CursorResourceId, VirtIOGPUInfo.CursorWidth, 0, 0, VirtIOGPUInfo.CursorWidth, VirtIOGPUInfo.CursorHeight))
    {
        return FALSE;
    }

    KMemSet(&Request, 0, sizeof(Request));
    Request.Header.Type        = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    Request.Position.ScanoutId = 0;
    Request.Position.X         = X;
    Request.Position.Y         = Y;
    Request.ResourceId         = VirtIOGPUInfo.CursorResourceId;
    Request.HotX               = VirtIOGPUInfo.CursorHotX;
    Request.HotY               = VirtIOGPUInfo.CursorHotY;

    if (!SendCursorCommand(&Request, sizeof(Request), L"cursor update"))
    {
        return FALSE;
    }

    LastError = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUMoveCursor(UINT32 X, UINT32 Y)
{
    VIRTIO_GPU_UPDATE_CURSOR_REQUEST Request;

    if (VirtIOGPUInfo.CursorResourceId == 0)
    {
        LastError = L"cursor missing";

        return FALSE;
    }

    KMemSet(&Request, 0, sizeof(Request));
    Request.Header.Type        = VIRTIO_GPU_CMD_MOVE_CURSOR;
    Request.Position.ScanoutId = 0;
    Request.Position.X         = X;
    Request.Position.Y         = Y;
    Request.ResourceId         = VirtIOGPUInfo.CursorResourceId;

    if (!SendCursorCommand(&Request, sizeof(Request), L"cursor move"))
    {
        return FALSE;
    }

    LastError = L"ok";

    return TRUE;
}

BOOLEAN VirtIOGPUHideCursor(VOID)
{
    VIRTIO_GPU_UPDATE_CURSOR_REQUEST Request;

    if (VirtIOGPUInfo.CursorResourceId == 0)
    {
        LastError = L"cursor missing";

        return FALSE;
    }

    KMemSet(&Request, 0, sizeof(Request));

    Request.Header.Type        = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    Request.Position.ScanoutId = 0;
    Request.ResourceId         = 0;

    if (!SendCursorCommand(&Request, sizeof(Request), L"cursor hide"))
    {
        return FALSE;
    }

    LastError = L"ok";

    return TRUE;
}

VIRTIO_GPU_INFO *VirtIOGPUGetInfo(VOID)
{
    return &VirtIOGPUInfo;
}

CONST CHAR16 *VirtIOGPUGetLastError(VOID)
{
    return LastError;
}

UINT32 VirtIOGPUGetFrameBufferWidth(VOID)
{
    return VirtIOGPUInfo.FrameBufferWidth;
}

UINT32 VirtIOGPUGetFrameBufferHeight(VOID)
{
    return VirtIOGPUInfo.FrameBufferHeight;
}

BOOLEAN VirtIOGPUPanicLocksAvailable(VOID)
{
    return ATOMIC_LOAD(&ControlDeadline, ATOMIC_ACQUIRE) == 0 && ATOMIC_LOAD(&RenderLock.Locked, ATOMIC_ACQUIRE) == 0 && ATOMIC_LOAD(&CommandLock.Locked, ATOMIC_ACQUIRE) == 0;
}

UINT32 VirtIOGPUGetCapsetCount(VOID)
{
    CONST VOLATILE VIRTIO_GPU_CONFIG *Config;

    if (!VirtIOGPUInfo.Found || (VirtIOGPUInfo.Device.NegotiatedFeaturesLow & (1U << VIRTIO_GPU_F_VIRGL)) == 0)
    {
        return 0;
    }

    Config = (CONST VOLATILE VIRTIO_GPU_CONFIG *) VirtIOGPUInfo.Device.DeviceConfig;

    return Config->NumCapsets;
}

BOOLEAN VirtIOGPUGetCapsetInfo(UINT32 Index, VIRTIO_GPU_GET_CAPSET_INFO_RESPONSE *Response)
{
    VIRTIO_GPU_GET_CAPSET_INFO_REQUEST Request = {0};
    UINT32                             ReplyLength;

    if (Response == NULL || Index >= VirtIOGPUGetCapsetCount())
    {
        return FALSE;
    }

    Request.Header.Type = VIRTIO_GPU_CMD_GET_CAPSET_INFO;
    Request.CapsetIndex = Index;

    if (!SendCommandToQueue(&VirtIOGPUInfo.ControlQueue, &Request, sizeof(Request), Response, sizeof(*Response), &ReplyLength, L"capset info", FALSE))
    {
        return FALSE;
    }

    return ReplyLength == sizeof(*Response) && Response->Header.Type == VIRTIO_GPU_RESP_OK_CAPSET_INFO;
}

STATIC BOOLEAN SendVirGLCommand(VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage, BOOLEAN CanSleep)
{
    VIRTIO_GPU_CTRL_HEADER Response = {0};
    UINT32                 ReplyLength;

    if (!VirtIOGPUInfo.Found || (VirtIOGPUInfo.Device.NegotiatedFeaturesLow & (1U << VIRTIO_GPU_F_VIRGL)) == 0)
    {
        return FALSE;
    }

    if (!SendCommandToQueue(&VirtIOGPUInfo.ControlQueue, Request, RequestLength, &Response, sizeof(Response), &ReplyLength, Stage, CanSleep))
    {
        return FALSE;
    }

    CONST VIRTIO_GPU_CTRL_HEADER *Header = Request;

    return ReplyLength == sizeof(Response) && Response.Type == VIRTIO_GPU_RESP_OK_NODATA && ((Header->Flags & VIRTIO_GPU_FLAG_FENCE) == 0 || ((Response.Flags & VIRTIO_GPU_FLAG_FENCE) != 0 && Response.FenceId == Header->FenceId));
}

BOOLEAN VirtIOGPUSendVirGLCommand(VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage)
{
    return SendVirGLCommand(Request, RequestLength, Stage, FALSE);
}

BOOLEAN VirtIOGPUSubmitVirGL(VOID *Request, UINT32 RequestLength)
{
    return SendVirGLCommand(Request, RequestLength, L"virgl submit", TRUE);
}

BOOLEAN VirtIOGPUReadCapset(UINT32 Id, UINT32 Version, VOID *Data, UINT32 Size, UINT32 *BytesRead)
{
    VIRTIO_GPU_GET_CAPSET_REQUEST Request = {0};
    VIRTIO_GPU_CTRL_HEADER       *Response;
    UINT32                        ResponseLength;
    UINT32                        ReplyLength;
    BOOLEAN                       Success;

    if (Data == NULL || BytesRead == NULL || Size == 0 || Size > 0xFFFFFFFFU - sizeof(*Response) || VirtIOGPUGetCapsetCount() == 0)
    {
        return FALSE;
    }

    *BytesRead     = 0;
    ResponseLength = sizeof(*Response) + Size;
    Response       = KAlloc(ResponseLength);

    if (Response == NULL)
    {
        return FALSE;
    }

    Request.Header.Type = VIRTIO_GPU_CMD_GET_CAPSET;
    Request.CapsetId    = Id;
    Request.Version     = Version;
    Success             = SendCommandToQueue(&VirtIOGPUInfo.ControlQueue, &Request, sizeof(Request), Response, ResponseLength, &ReplyLength, L"get capset", FALSE) && ReplyLength > sizeof(*Response) && ReplyLength <= ResponseLength && Response->Type == VIRTIO_GPU_RESP_OK_CAPSET;

    if (Success)
    {
        *BytesRead = ReplyLength - sizeof(*Response);
        KMemCpy(Data, Response + 1, *BytesRead);
    }

    KFree(Response);

    return Success;
}
