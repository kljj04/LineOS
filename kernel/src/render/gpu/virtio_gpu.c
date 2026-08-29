// kernel/src/render/gpu/virtio_gpu.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <pci/pci.h>
#include <render/gpu/virtio_gpu_protocol.h>
#include <virtio/virtio_pci.h>
#include <render/gpu/virtio_gpu.h>
#include <virtio/virtqueue.h>

STATIC VIRTIO_GPU_INFO VirtIOGPUInfo;
STATIC CONST CHAR16   *LastError = L"not initialized";

#define VIRTIO_GPU_RESOURCE_FRAMEBUFFER 1
#define VIRTIO_GPU_RESOURCE_CURSOR      2
#define VIRTIO_GPU_BYTES_PER_PIXEL      4
#define PAGE_SIZE                       4096ULL

STATIC BOOLEAN SendNoDataCommandToQueue(VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage);
STATIC BOOLEAN TransferResourceRect(UINT32 ResourceId, UINT32 FrameWidth, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height);

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

    if (!VirtQueueSend(&VirtIOGPUInfo.Device, &VirtIOGPUInfo.ControlQueue, &Request, sizeof(Request), &VirtIOGPUInfo.DisplayInfo, sizeof(VirtIOGPUInfo.DisplayInfo)))
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
    return SendNoDataCommandToQueue(&VirtIOGPUInfo.ControlQueue, Request, RequestLength, Stage);
}

STATIC BOOLEAN SendNoDataCommandToQueue(VIRTQUEUE *Queue, VOID *Request, UINT32 RequestLength, CONST CHAR16 *Stage)
{
    VIRTIO_GPU_CTRL_HEADER *RequestHeader = (VIRTIO_GPU_CTRL_HEADER *) Request;
    VIRTIO_GPU_CTRL_HEADER  Response;

    KMemSet(&Response, 0, sizeof(Response));
    SetCommandDebug(Stage, RequestHeader->Type);
    if (!VirtQueueSend(&VirtIOGPUInfo.Device, Queue, Request, RequestLength, &Response, sizeof(Response)))
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

STATIC VOID SetRectAt(VIRTIO_GPU_RECT *Rect, UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    Rect->X = X;
    Rect->Y = Y;
    Rect->Width = Width;
    Rect->Height = Height;
}

STATIC UINT32 FrameBufferOffset(UINT32 X, UINT32 Y)
{
    return (Y * VirtIOGPUInfo.FrameBufferWidth) + X;
}

STATIC INT32 Abs(INT32 Value)
{
    if (Value < 0)
    {
        return -Value;
    }

    return Value;
}

STATIC UINT32 MakePixel(UINT32 Color)
{
    return Color | 0xFF000000;
}

STATIC UINT8 GetColorAlpha(UINT32 Color)
{
    if (Color <= 0xFFFFFF)
    {
        return 255;
    }

    return (UINT8) (Color & 0xFF);
}

STATIC UINT32 GetColorRGB(UINT32 Color)
{
    if (Color <= 0xFFFFFF)
    {
        return Color;
    }

    return Color >> 8;
}

STATIC UINT32 BlendColor(UINT32 Background, UINT32 Color)
{
    UINT32 RGB = GetColorRGB(Color);
    UINT32 Alpha = GetColorAlpha(Color);
    UINT32 InverseAlpha;
    UINT32 Red;
    UINT32 Green;
    UINT32 Blue;

    if (Alpha == 255)
    {
        return MakePixel(RGB);
    }

    if (Alpha == 0)
    {
        return Background;
    }

    InverseAlpha = 255 - Alpha;
    Red = ((((RGB >> 16) & 0xFF) * Alpha) + (((Background >> 16) & 0xFF) * InverseAlpha)) / 255;
    Green = ((((RGB >> 8) & 0xFF) * Alpha) + (((Background >> 8) & 0xFF) * InverseAlpha)) / 255;
    Blue = (((RGB & 0xFF) * Alpha) + ((Background & 0xFF) * InverseAlpha)) / 255;

    return MakePixel((Red << 16) | (Green << 8) | Blue);
}

STATIC VOID FillRow(UINT32 *Destination, UINT32 Width, UINT32 Color)
{
    UINT64 Pattern = ((UINT64) Color << 32) | Color;

    while ((((UINTN) Destination) & 7) != 0 && Width != 0)
    {
        *Destination++ = Color;
        Width--;
    }

    UINT64 *Destination64 = (UINT64 *) Destination;

    while (Width >= 2)
    {
        *Destination64++ = Pattern;
        Width -= 2;
    }

    Destination = (UINT32 *) Destination64;

    if (Width != 0)
    {
        *Destination = Color;
    }
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

    *Width = MaxX - *X;
    *Height = MaxY - *Y;

    return *Width != 0 && *Height != 0;
}

STATIC BOOLEAN RectsTouchOrOverlap(VIRTIO_GPU_RECT *a, VIRTIO_GPU_RECT *b)
{
    UINT32 ARight = a->X + a->Width;
    UINT32 ABottom = a->Y + a->Height;
    UINT32 BRight = b->X + b->Width;
    UINT32 BBottom = b->Y + b->Height;

    return a->X <= BRight &&
           b->X <= ARight &&
           a->Y <= BBottom &&
           b->Y <= ABottom;
}

STATIC VOID MergeRect(VIRTIO_GPU_RECT *Target, VIRTIO_GPU_RECT *Source)
{
    UINT32 Left = Target->X < Source->X ? Target->X : Source->X;
    UINT32 Top = Target->Y < Source->Y ? Target->Y : Source->Y;
    UINT32 TargetRight = Target->X + Target->Width;
    UINT32 SourceRight = Source->X + Source->Width;
    UINT32 TargetBottom = Target->Y + Target->Height;
    UINT32 SourceBottom = Source->Y + Source->Height;
    UINT32 Right = TargetRight > SourceRight ? TargetRight : SourceRight;
    UINT32 Bottom = TargetBottom > SourceBottom ? TargetBottom : SourceBottom;

    Target->X = Left;
    Target->Y = Top;
    Target->Width = Right - Left;
    Target->Height = Bottom - Top;
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
    VIRTIO_GPU_RESOURCE_CREATE_2D_REQUEST      CreateRequest;
    VIRTIO_GPU_RESOURCE_ATTACH_BACKING_REQUEST AttachRequest;
    UINT64                                     FrameBufferBytes;
    UINT64                                     FrameBufferPages;

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
    VirtIOGPUClearDirty();

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

    if (!VirtIOGPUSetScanout())
    {
        return FALSE;
    }

    LastError = L"ok";
    return TRUE;
}

VOID FillScreen(UINT32 Color)
{
    UINT64 PixelCount;
    UINT8  Alpha;

    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        return;
    }

    Alpha = GetColorAlpha(Color);
    PixelCount = (UINT64) VirtIOGPUInfo.FrameBufferWidth * VirtIOGPUInfo.FrameBufferHeight;
    if (Alpha == 255)
    {
        FillRow(VirtIOGPUInfo.FrameBuffer, (UINT32) PixelCount, MakePixel(GetColorRGB(Color)));
        VirtIOGPUMarkDirty(0, 0, VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
        return;
    }

    for (UINT64 Index = 0; Index < PixelCount; Index++)
    {
        VirtIOGPUInfo.FrameBuffer[Index] = BlendColor(VirtIOGPUInfo.FrameBuffer[Index], Color);
    }

    VirtIOGPUMarkDirty(0, 0, VirtIOGPUInfo.FrameBufferWidth, VirtIOGPUInfo.FrameBufferHeight);
}

VOID DrawPixel(UINT32 X, UINT32 Y, UINT32 Color)
{
    if (VirtIOGPUInfo.FrameBuffer == NULL || X >= VirtIOGPUInfo.FrameBufferWidth || Y >= VirtIOGPUInfo.FrameBufferHeight)
    {
        return;
    }

    UINT32 Offset = FrameBufferOffset(X, Y);

    VirtIOGPUInfo.FrameBuffer[Offset] = BlendColor(VirtIOGPUInfo.FrameBuffer[Offset], Color);
    VirtIOGPUMarkDirty(X, Y, 1, 1);
}

VOID FillRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color)
{
    UINT32 MaxX;
    UINT32 MaxY;
    UINT8  Alpha = GetColorAlpha(Color);
    UINT32 Pixel = MakePixel(GetColorRGB(Color));

    if (VirtIOGPUInfo.FrameBuffer == NULL || X >= VirtIOGPUInfo.FrameBufferWidth || Y >= VirtIOGPUInfo.FrameBufferHeight)
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
        UINT32 *Line = &VirtIOGPUInfo.FrameBuffer[FrameBufferOffset(X, Row)];

        if (Alpha == 255)
        {
            FillRow(Line, MaxX - X, Pixel);
            continue;
        }

        for (UINT32 Column = X; Column < MaxX; Column++)
        {
            *Line = BlendColor(*Line, Color);
            Line++;
        }
    }

    VirtIOGPUMarkDirty(X, Y, MaxX - X, MaxY - Y);
}

VOID DrawRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    FillRect(X, Y, Width, 1, Color);
    FillRect(X, Y + Height - 1, Width, 1, Color);
    FillRect(X, Y, 1, Height, Color);
    FillRect(X + Width - 1, Y, 1, Height, Color);
}

VOID DrawLine(INT32 X1, INT32 Y1, INT32 X2, INT32 Y2, UINT32 Color)
{
    INT32 DeltaX = Abs(X2 - X1);
    INT32 DeltaY = -Abs(Y2 - Y1);
    INT32 StepX = X1 < X2 ? 1 : -1;
    INT32 StepY = Y1 < Y2 ? 1 : -1;
    INT32 Error = DeltaX + DeltaY;

    while (1)
    {
        if (X1 >= 0 && Y1 >= 0)
        {
            DrawPixel((UINT32) X1, (UINT32) Y1, Color);
        }

        if (X1 == X2 && Y1 == Y2)
        {
            break;
        }

        INT32 Error2 = Error * 2;

        if (Error2 >= DeltaY)
        {
            Error += DeltaY;
            X1 += StepX;
        }

        if (Error2 <= DeltaX)
        {
            Error += DeltaX;
            Y1 += StepY;
        }
    }
}

STATIC VOID DrawCirclePoints(INT32 CenterX, INT32 CenterY, INT32 X, INT32 Y, UINT32 Color)
{
    DrawPixel((UINT32) (CenterX + X), (UINT32) (CenterY + Y), Color);
    DrawPixel((UINT32) (CenterX - X), (UINT32) (CenterY + Y), Color);
    DrawPixel((UINT32) (CenterX + X), (UINT32) (CenterY - Y), Color);
    DrawPixel((UINT32) (CenterX - X), (UINT32) (CenterY - Y), Color);
    DrawPixel((UINT32) (CenterX + Y), (UINT32) (CenterY + X), Color);
    DrawPixel((UINT32) (CenterX - Y), (UINT32) (CenterY + X), Color);
    DrawPixel((UINT32) (CenterX + Y), (UINT32) (CenterY - X), Color);
    DrawPixel((UINT32) (CenterX - Y), (UINT32) (CenterY - X), Color);
}

VOID DrawCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color)
{
    INT32 X = 0;
    INT32 Y = (INT32) Radius;
    INT32 Decision = 1 - (INT32) Radius;

    while (X <= Y)
    {
        DrawCirclePoints(CenterX, CenterY, X, Y, Color);
        X++;

        if (Decision < 0)
        {
            Decision += (2 * X) + 1;
        }
        else
        {
            Y--;
            Decision += (2 * (X - Y)) + 1;
        }
    }
}

VOID FillCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color)
{
    INT32 RadiusSquared = (INT32) (Radius * Radius);

    for (INT32 Y = -(INT32) Radius; Y <= (INT32) Radius; Y++)
    {
        for (INT32 X = -(INT32) Radius; X <= (INT32) Radius; X++)
        {
            if ((X * X) + (Y * Y) <= RadiusSquared)
            {
                DrawPixel((UINT32) (CenterX + X), (UINT32) (CenterY + Y), Color);
            }
        }
    }
}

VOID VirtIOGPUBlendPixel(UINT32 X, UINT32 Y, UINT32 Color, UINT8 Alpha)
{
    DrawPixel(X, Y, (GetColorRGB(Color) << 8) | Alpha);
}

VOID CopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 Width, UINT32 Height, UINT32 TargetX, UINT32 TargetY)
{
    UINTN CopySize;

    if (VirtIOGPUInfo.FrameBuffer == NULL || Width == 0 || Height == 0)
    {
        return;
    }

    if (SourceX >= VirtIOGPUInfo.FrameBufferWidth || SourceY >= VirtIOGPUInfo.FrameBufferHeight || TargetX >= VirtIOGPUInfo.FrameBufferWidth || TargetY >= VirtIOGPUInfo.FrameBufferHeight)
    {
        return;
    }

    if (SourceX + Width > VirtIOGPUInfo.FrameBufferWidth || SourceX + Width < SourceX)
    {
        Width = VirtIOGPUInfo.FrameBufferWidth - SourceX;
    }

    if (TargetX + Width > VirtIOGPUInfo.FrameBufferWidth || TargetX + Width < TargetX)
    {
        Width = VirtIOGPUInfo.FrameBufferWidth - TargetX;
    }

    if (SourceY + Height > VirtIOGPUInfo.FrameBufferHeight || SourceY + Height < SourceY)
    {
        Height = VirtIOGPUInfo.FrameBufferHeight - SourceY;
    }

    if (TargetY + Height > VirtIOGPUInfo.FrameBufferHeight || TargetY + Height < TargetY)
    {
        Height = VirtIOGPUInfo.FrameBufferHeight - TargetY;
    }

    if (Width == 0 || Height == 0)
    {
        return;
    }

    CopySize = (UINTN) Width * sizeof(UINT32);

    if (TargetY > SourceY)
    {
        for (UINT32 RowIndex = Height; RowIndex > 0; RowIndex--)
        {
            UINT32  CopyY = RowIndex - 1;
            UINT32 *Destination = &VirtIOGPUInfo.FrameBuffer[FrameBufferOffset(TargetX, TargetY + CopyY)];
            UINT32 *Source = &VirtIOGPUInfo.FrameBuffer[FrameBufferOffset(SourceX, SourceY + CopyY)];

            KMemMove(Destination, Source, CopySize);
        }

        VirtIOGPUMarkDirty(TargetX, TargetY, Width, Height);
        return;
    }

    for (UINT32 RowIndex = 0; RowIndex < Height; RowIndex++)
    {
        UINT32 *Destination = &VirtIOGPUInfo.FrameBuffer[FrameBufferOffset(TargetX, TargetY + RowIndex)];
        UINT32 *Source = &VirtIOGPUInfo.FrameBuffer[FrameBufferOffset(SourceX, SourceY + RowIndex)];

        if (Destination == Source)
        {
            continue;
        }

        KMemMove(Destination, Source, CopySize);
    }

    VirtIOGPUMarkDirty(TargetX, TargetY, Width, Height);
}

UINT32 ReadPixel(UINT32 X, UINT32 Y)
{
    if (VirtIOGPUInfo.FrameBuffer == NULL || X >= VirtIOGPUInfo.FrameBufferWidth || Y >= VirtIOGPUInfo.FrameBufferHeight)
    {
        return 0;
    }

    return VirtIOGPUInfo.FrameBuffer[FrameBufferOffset(X, Y)];
}

STATIC VOID MemoryFence(VOID)
{
    ASM("mfence" ::: "memory");
}

VOID VirtIOGPUMarkDirty(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_RECT Rect;

    if (!ClipRect(&X, &Y, &Width, &Height))
    {
        return;
    }

    Rect.X = X;
    Rect.Y = Y;
    Rect.Width = Width;
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
    VirtIOGPUInfo.Dirty = TRUE;
}

VOID VirtIOGPUClearDirty(VOID)
{
    VirtIOGPUInfo.Dirty = FALSE;
    VirtIOGPUInfo.DirtyRectCount = 0;
}

BOOLEAN VirtIOGPUSetScanout(VOID)
{
    VIRTIO_GPU_SET_SCANOUT_REQUEST         ScanoutRequest;

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

    VirtIOGPUInfo.ScanoutSet = TRUE;
    VirtIOGPUInfo.Debug.SetScanoutOK = TRUE;
    LastError = L"ok";
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
    TransferRequest.Offset = ((UINT64) Y * FrameWidth + X) * VIRTIO_GPU_BYTES_PER_PIXEL;
    TransferRequest.ResourceId = ResourceId;
    if (!SendNoDataCommand(&TransferRequest, sizeof(TransferRequest), L"transfer 2d"))
    {
        return FALSE;
    }

    VirtIOGPUInfo.Debug.TransferOK = TRUE;
    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtIOGPUFlushRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height)
{
    VIRTIO_GPU_RESOURCE_FLUSH_REQUEST      FlushRequest;

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
    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtIOGPUPresent(VOID)
{
    if (VirtIOGPUInfo.FrameBuffer == NULL)
    {
        LastError = L"gpu framebuffer missing";
        return FALSE;
    }

    if (!VirtIOGPUInfo.ScanoutSet && !VirtIOGPUSetScanout())
    {
        return FALSE;
    }

    if (!VirtIOGPUInfo.Dirty)
    {
        LastError = L"ok";
        return TRUE;
    }

    for (UINT32 Index = 0; Index < VirtIOGPUInfo.DirtyRectCount; Index++)
    {
        VIRTIO_GPU_RECT *Rect = &VirtIOGPUInfo.DirtyRects[Index];

        if (!VirtIOGPUTransferRect(Rect->X, Rect->Y, Rect->Width, Rect->Height))
        {
            return FALSE;
        }

        if (!VirtIOGPUFlushRect(Rect->X, Rect->Y, Rect->Width, Rect->Height))
        {
            return FALSE;
        }
    }

    VirtIOGPUClearDirty();
    LastError = L"ok";
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
    VIRTIO_GPU_UPDATE_CURSOR_REQUEST           CursorRequest;
    UINT64                                     CursorBytes;
    UINT64                                     CursorPages;

    if (!VirtIOGPUInfo.Found || Width == 0 || Height == 0)
    {
        LastError = L"cursor args invalid";
        return FALSE;
    }

    CursorBytes = (UINT64) Width * Height * VIRTIO_GPU_BYTES_PER_PIXEL;
    CursorPages = AlignUp(CursorBytes, PAGE_SIZE) / PAGE_SIZE;
    VirtIOGPUInfo.CursorBuffer = (UINT32 *) KAllocPages((UINTN) CursorPages);
    if (VirtIOGPUInfo.CursorBuffer == NULL)
    {
        LastError = L"cursor alloc failed";
        return FALSE;
    }

    VirtIOGPUInfo.CursorResourceId = VIRTIO_GPU_RESOURCE_CURSOR;
    VirtIOGPUInfo.CursorWidth = Width;
    VirtIOGPUInfo.CursorHeight = Height;

    KMemSet(VirtIOGPUInfo.CursorBuffer, 0, (UINTN) CursorBytes);
    for (UINT32 Y = 0; Y < Height; Y++)
    {
        for (UINT32 X = 0; X < Width; X++)
        {
            if (X == 0 || Y == 0 || X == Y || (X < 6 && Y < 18) || (Y >= 14 && Y < 20 && X >= 6 && X <= 10))
            {
                VirtIOGPUInfo.CursorBuffer[(Y * Width) + X] = 0xFFFFFFFF;
            }
        }
    }

    KMemSet(&CreateRequest, 0, sizeof(CreateRequest));
    CreateRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    CreateRequest.ResourceId = VirtIOGPUInfo.CursorResourceId;
    CreateRequest.Format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    CreateRequest.Width = Width;
    CreateRequest.Height = Height;
    if (!SendNoDataCommand(&CreateRequest, sizeof(CreateRequest), L"cursor create"))
    {
        return FALSE;
    }

    KMemSet(&AttachRequest, 0, sizeof(AttachRequest));
    AttachRequest.Header.Type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    AttachRequest.ResourceId = VirtIOGPUInfo.CursorResourceId;
    AttachRequest.EntryCount = 1;
    AttachRequest.Entry.Address = (UINT64) VirtIOGPUInfo.CursorBuffer;
    AttachRequest.Entry.Length = (UINT32) CursorBytes;
    if (!SendNoDataCommand(&AttachRequest, sizeof(AttachRequest), L"cursor attach"))
    {
        return FALSE;
    }

    if (!TransferResourceRect(VirtIOGPUInfo.CursorResourceId, Width, 0, 0, Width, Height))
    {
        return FALSE;
    }

    KMemSet(&CursorRequest, 0, sizeof(CursorRequest));
    CursorRequest.Header.Type = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    CursorRequest.Position.ScanoutId = 0;
    CursorRequest.ResourceId = VirtIOGPUInfo.CursorResourceId;
    CursorRequest.HotX = HotX;
    CursorRequest.HotY = HotY;

    if (!SendNoDataCommandToQueue(&VirtIOGPUInfo.CursorQueue, &CursorRequest, sizeof(CursorRequest), L"cursor update"))
    {
        return FALSE;
    }

    LastError = L"ok";
    return TRUE;
}

BOOLEAN VirtIOGPUUpdateCursor(UINT32 X, UINT32 Y)
{
    VIRTIO_GPU_UPDATE_CURSOR_REQUEST Request;

    if (VirtIOGPUInfo.CursorBuffer == NULL)
    {
        LastError = L"cursor missing";
        return FALSE;
    }

    KMemSet(&Request, 0, sizeof(Request));
    Request.Header.Type = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    Request.Position.ScanoutId = 0;
    Request.Position.X = X;
    Request.Position.Y = Y;
    Request.ResourceId = VirtIOGPUInfo.CursorResourceId;
    Request.HotX = 0;
    Request.HotY = 0;

    if (!SendNoDataCommandToQueue(&VirtIOGPUInfo.CursorQueue, &Request, sizeof(Request), L"cursor update"))
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
    Request.Header.Type = VIRTIO_GPU_CMD_MOVE_CURSOR;
    Request.Position.ScanoutId = 0;
    Request.Position.X = X;
    Request.Position.Y = Y;
    Request.ResourceId = VirtIOGPUInfo.CursorResourceId;

    if (!SendNoDataCommandToQueue(&VirtIOGPUInfo.CursorQueue, &Request, sizeof(Request), L"cursor move"))
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
