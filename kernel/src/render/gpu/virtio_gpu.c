// virtio_gpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu_protocol.h>
#include <render/gpu/virtio_pci.h>
#include <render/gpu/virtio_gpu.h>
#include <render/gpu/virtqueue.h>

STATIC VIRTIO_GPU_INFO VirtIOGPUInfo;
STATIC CONST CHAR16 *LastError = L"not initialized";

#define VIRTIO_GPU_RESOURCE_FRAMEBUFFER 1
#define VIRTIO_GPU_BYTES_PER_PIXEL 4
#define PAGE_SIZE 4096ULL

STATIC UINT64 AlignUp(UINT64 Value, UINT64 Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

STATIC VOID ResetInfo(VOID)
{
    KMemSet(&VirtIOGPUInfo, 0, sizeof(VirtIOGPUInfo));
    VirtIOGPUInfo.Debug.LastStage = L"reset";
}

STATIC VOID SetCommandDebug(CONST CHAR16 *Stage, UINT32 Command)
{
    VirtIOGPUInfo.Debug.LastStage = Stage;
    VirtIOGPUInfo.Debug.LastCommand = Command;
    VirtIOGPUInfo.Debug.LastResponse = 0;
}

STATIC BOOLEAN GetDisplayInfo(VOID)
{
    VIRTIO_GPU_GET_DISPLAY_INFO_REQUEST Request;

    KMemSet(&Request, 0, sizeof(Request));
    KMemSet(&VirtIOGPUInfo.DisplayInfo, 0, sizeof(VirtIOGPUInfo.DisplayInfo));
    Request.Header.Type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    SetCommandDebug(L"display info", Request.Header.Type);

    if (!VirtQueueSend(&VirtIOGPUInfo.Device, &VirtIOGPUInfo.ControlQueue, &Request, sizeof(Request),
                       &VirtIOGPUInfo.DisplayInfo, sizeof(VirtIOGPUInfo.DisplayInfo)))
    {
        LastError = VirtQueueGetLastError();
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
    VIRTIO_GPU_CTRL_HEADER *RequestHeader = (VIRTIO_GPU_CTRL_HEADER *) Request;
    VIRTIO_GPU_CTRL_HEADER Response;

    KMemSet(&Response, 0, sizeof(Response));
    SetCommandDebug(Stage, RequestHeader->Type);
    if (!VirtQueueSend(&VirtIOGPUInfo.Device, &VirtIOGPUInfo.ControlQueue, Request, RequestLength, &Response,
                       sizeof(Response)))
    {
        LastError = VirtQueueGetLastError();
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

STATIC VOID SetRect(VIRTIO_GPU_RECT *Rect, UINT32 Width, UINT32 Height)
{
    Rect->X = 0;
    Rect->Y = 0;
    Rect->Width = Width;
    Rect->Height = Height;
}

BOOLEAN VirtIOGPUInit(VOID)
{
    ResetInfo();
    if (!VirtIOPCIInitDevice(&VirtIOGPUInfo.Device, VIRTIO_GPU_VENDOR_ID, VIRTIO_GPU_DEVICE_ID))
    {
        LastError = VirtIOPCIGetLastError();
        return FALSE;
    }
    VirtIOGPUInfo.Debug.InitOK = TRUE;

    if (!VirtIOPCIStartDevice(&VirtIOGPUInfo.Device))
    {
        LastError = VirtIOPCIGetLastError();
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

    VirtIOGPUInfo.Device.CommonConfig->DeviceStatus |= VIRTIO_STATUS_DRIVER_OK;
    if (!GetDisplayInfo())
    {
        return FALSE;
    }
    VirtIOGPUInfo.Debug.DisplayInfoOK = TRUE;

    VirtIOGPUInfo.Found = TRUE;
    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtIOGPUCreateFrameBuffer(UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_RESOURCE_CREATE_2D_REQUEST CreateRequest;
    VIRTIO_GPU_RESOURCE_ATTACH_BACKING_REQUEST AttachRequest;
    UINT64 FrameBufferBytes;
    UINT64 FrameBufferPages;

    if (!VirtIOGPUInfo.Found)
    {
        LastError = L"virtio gpu not ready";
        return FALSE;
    }

    if (Width == 0 || Height == 0)
    {
        LastError = L"gpu framebuffer size invalid";
        return FALSE;
    }

    FrameBufferBytes = (UINT64) Width * Height * VIRTIO_GPU_BYTES_PER_PIXEL;
    FrameBufferPages = AlignUp(FrameBufferBytes, PAGE_SIZE) / PAGE_SIZE;
    VirtIOGPUInfo.FrameBuffer = (UINT32 *) KAllocPages((UINTN) FrameBufferPages);
    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        LastError = L"gpu framebuffer alloc failed";
        return FALSE;
    }

    VirtIOGPUInfo.FrameBufferWidth = Width;
    VirtIOGPUInfo.FrameBufferHeight = Height;
    VirtIOGPUInfo.ResourceId = VIRTIO_GPU_RESOURCE_FRAMEBUFFER;

    KMemSet(&CreateRequest, 0, sizeof(CreateRequest));
    CreateRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    CreateRequest.ResourceId = VirtIOGPUInfo.ResourceId;
    CreateRequest.Format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    CreateRequest.Width = Width;
    CreateRequest.Height = Height;
    if (!SendNoDataCommand(&CreateRequest, sizeof(CreateRequest), L"create 2d"))
    {
        return FALSE;
    }
    VirtIOGPUInfo.Debug.CreateOK = TRUE;

    KMemSet(&AttachRequest, 0, sizeof(AttachRequest));
    AttachRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    AttachRequest.ResourceId = VirtIOGPUInfo.ResourceId;
    AttachRequest.EntryCount = 1;
    AttachRequest.Entry.Address = (UINT64) VirtIOGPUInfo.FrameBuffer;
    AttachRequest.Entry.Length = (UINT32) FrameBufferBytes;
    if (!SendNoDataCommand(&AttachRequest, sizeof(AttachRequest), L"attach backing"))
    {
        return FALSE;
    }
    VirtIOGPUInfo.Debug.AttachOK = TRUE;

    LastError = L"ok";
    return TRUE;
}

VOID VirtIOGPUFill(UINT32 Color)
{
    UINT64 PixelCount;

    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        return;
    }

    PixelCount = (UINT64) VirtIOGPUInfo.FrameBufferWidth * VirtIOGPUInfo.FrameBufferHeight;
    for (UINT64 Index = 0; Index < PixelCount; Index++)
    {
        VirtIOGPUInfo.FrameBuffer[Index] = Color | 0xFF000000;
    }
}

VOID VirtIOGPUFillRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color)
{
    UINT32 MaxX;
    UINT32 MaxY;
    UINT32 Pixel = Color | 0xFF000000;

    if (VirtIOGPUInfo.FrameBuffer == NULL || X >= VirtIOGPUInfo.FrameBufferWidth ||
        Y >= VirtIOGPUInfo.FrameBufferHeight)
    {
        return;
    }

    MaxX = X + Width;
    MaxY = Y + Height;

    if (MaxX > VirtIOGPUInfo.FrameBufferWidth || MaxX < X)
    {
        MaxX = VirtIOGPUInfo.FrameBufferWidth;
    }

    if (MaxY > VirtIOGPUInfo.FrameBufferHeight || MaxY < Y)
    {
        MaxY = VirtIOGPUInfo.FrameBufferHeight;
    }

    for (UINT32 Row = Y; Row < MaxY; Row++)
    {
        UINT32 *Line = &VirtIOGPUInfo.FrameBuffer[(Row * VirtIOGPUInfo.FrameBufferWidth) + X];

        for (UINT32 Column = X; Column < MaxX; Column++)
        {
            *Line++ = Pixel;
        }
    }
}

UINT32 VirtIOGPUReadPixel(UINT32 X, UINT32 Y)
{
    if (VirtIOGPUInfo.FrameBuffer == NULL || X >= VirtIOGPUInfo.FrameBufferWidth ||
        Y >= VirtIOGPUInfo.FrameBufferHeight)
    {
        return 0;
    }

    return VirtIOGPUInfo.FrameBuffer[(Y * VirtIOGPUInfo.FrameBufferWidth) + X];
}

STATIC VOID MemoryFence(VOID)
{
    __asm__ volatile("mfence" ::: "memory");
}

BOOLEAN VirtIOGPUFlush(VOID)
{
    VIRTIO_GPU_SET_SCANOUT_REQUEST ScanoutRequest;
    VIRTIO_GPU_TRANSFER_TO_HOST_2D_REQUEST TransferRequest;
    VIRTIO_GPU_RESOURCE_FLUSH_REQUEST FlushRequest;

    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        LastError = L"gpu framebuffer missing";
        return FALSE;
    }

    KMemSet(&ScanoutRequest, 0, sizeof(ScanoutRequest));
    ScanoutRequest.Header.Type = VIRTIO_GPU_CMD_SET_SCANOUT;
    SetRect(&ScanoutRequest.Rect, VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
    ScanoutRequest.ScanoutId = 0;
    ScanoutRequest.ResourceId = VirtIOGPUInfo.ResourceId;
    if (!SendNoDataCommand(&ScanoutRequest, sizeof(ScanoutRequest), L"set scanout"))
    {
        return FALSE;
    }
    VirtIOGPUInfo.Debug.SetScanoutOK = TRUE;

    MemoryFence();

    KMemSet(&TransferRequest, 0, sizeof(TransferRequest));
    TransferRequest.Header.Type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    SetRect(&TransferRequest.Rect, VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
    TransferRequest.Offset = 0;
    TransferRequest.ResourceId = VirtIOGPUInfo.ResourceId;
    if (!SendNoDataCommand(&TransferRequest, sizeof(TransferRequest), L"transfer 2d"))
    {
        return FALSE;
    }
    VirtIOGPUInfo.Debug.TransferOK = TRUE;

    KMemSet(&FlushRequest, 0, sizeof(FlushRequest));
    FlushRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    SetRect(&FlushRequest.Rect, VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
    FlushRequest.ResourceId = VirtIOGPUInfo.ResourceId;
    if (!SendNoDataCommand(&FlushRequest, sizeof(FlushRequest), L"flush"))
    {
        return FALSE;
    }
    VirtIOGPUInfo.Debug.FlushOK = TRUE;

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
