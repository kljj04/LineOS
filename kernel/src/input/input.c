// kernel/src/input/input.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <arch/x86_64/spinlock.h>
#include <cursor/cursor.h>
#include <input/virtio_input.h>
#include <render/gpu/virtio_gpu.h>
#include <input/input.h>

#define INPUT_CURSOR_REDRAW_SIZE 64

STATIC UINT32 MouseMinX = 0;
STATIC UINT32 MouseMaxX = 32767;
STATIC UINT32 MouseMinY = 0;
STATIC UINT32 MouseMaxY = 32767;

STATIC MOUSE_STATUS MouseStatus = {0};
STATIC KEYBOARD_STATUS KeyboardStatus = {0};
STATIC SPIN_LOCK MouseStatusLock;
STATIC SPIN_LOCK KeyboardStatusLock;

STATIC BOOLEAN CursorReady = FALSE;
STATIC UINT32 CursorDrawX = 0;
STATIC UINT32 CursorDrawY = 0;

STATIC UINT32 CursorBackground[INPUT_CURSOR_REDRAW_SIZE * INPUT_CURSOR_REDRAW_SIZE];

STATIC UINT32 MinUINT32(UINT32 a, UINT32 b)
{
    return a < b ? a : b;
}

STATIC UINT32 MaxUINT32(UINT32 a, UINT32 b)
{
    return a > b ? a : b;
}

STATIC UINT32 ScalePointerCoordinate(UINT32 value, UINT32 min, UINT32 max, UINT32 outputMax)
{
    if (value <= min)
    {
        return 0;
    }

    if (value >= max)
    {
        return outputMax;
    }

    if (max <= min)
    {
        return 0;
    }

    return (UINT32) (((UINT64) (value - min) * outputMax) / (max - min));
}

STATIC VOID FlushCursorRect(VIRTIO_GPU_INFO *GPU, UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
    if (x >= GPU->FrameBufferWidth || y >= GPU->FrameBufferHeight)
    {
        return;
    }

    if (x + width > GPU->FrameBufferWidth)
    {
        width = GPU->FrameBufferWidth - x;
    }

    if (y + height > GPU->FrameBufferHeight)
    {
        height = GPU->FrameBufferHeight - y;
    }

    if (width == 0 || height == 0)
    {
        return;
    }

    VirtIOGPUTransferRect(x, y, width, height);
    VirtIOGPUFlushRect(x, y, width, height);
}

STATIC VOID FlushCursorBox(VIRTIO_GPU_INFO *GPU, UINT32 x, UINT32 y)
{
    FlushCursorRect(GPU, x, y, INPUT_CURSOR_REDRAW_SIZE, INPUT_CURSOR_REDRAW_SIZE);
}

STATIC VOID FlushCursorUnion(VIRTIO_GPU_INFO *GPU, UINT32 OldX, UINT32 OldY, UINT32 NewX, UINT32 NewY)
{
    UINT32 MinX;
    UINT32 MinY;
    UINT32 MaxX;
    UINT32 MaxY;

    MinX = MinUINT32(OldX, NewX);
    MinY = MinUINT32(OldY, NewY);

    MaxX = MaxUINT32(OldX + INPUT_CURSOR_REDRAW_SIZE, NewX + INPUT_CURSOR_REDRAW_SIZE);
    MaxY = MaxUINT32(OldY + INPUT_CURSOR_REDRAW_SIZE, NewY + INPUT_CURSOR_REDRAW_SIZE);

    if (MaxX > GPU->FrameBufferWidth)
    {
        MaxX = GPU->FrameBufferWidth;
    }

    if (MaxY > GPU->FrameBufferHeight)
    {
        MaxY = GPU->FrameBufferHeight;
    }

    if (MinX >= MaxX || MinY >= MaxY)
    {
        return;
    }

    VirtIOGPUTransferRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
    VirtIOGPUFlushRect(MinX, MinY, MaxX - MinX, MaxY - MinY);
}

STATIC UINT32 BlendCursorPixel(UINT32 Background, UINT32 Cursor)
{
    UINT32 Alpha;
    UINT32 InverseAlpha;
    UINT32 Red;
    UINT32 Green;
    UINT32 Blue;

    Alpha = Cursor >> 24;

    if (Alpha == 0)
    {
        return Background;
    }

    if (Alpha == 255)
    {
        return Cursor;
    }

    InverseAlpha = 255 - Alpha;

    Red = ((((Cursor >> 16) & 0xFF) * Alpha) + (((Background >> 16) & 0xFF) * InverseAlpha)) / 255;
    Green = ((((Cursor >> 8) & 0xFF) * Alpha) + (((Background >> 8) & 0xFF) * InverseAlpha)) / 255;
    Blue = (((Cursor & 0xFF) * Alpha) + ((Background & 0xFF) * InverseAlpha)) / 255;

    return 0xFF000000 | (Red << 16) | (Green << 8) | Blue;
}

STATIC VOID SaveCursorBackground(VIRTIO_GPU_INFO *GPU, UINT32 x, UINT32 y)
{
    UINT32 TargetX;
    UINT32 TargetY;
    UINT32 Index;

    for (UINT32 Row = 0; Row < INPUT_CURSOR_REDRAW_SIZE; Row++)
    {
        for (UINT32 Column = 0; Column < INPUT_CURSOR_REDRAW_SIZE; Column++)
        {
            TargetX = x + Column;
            TargetY = y + Row;
            Index = Row * INPUT_CURSOR_REDRAW_SIZE + Column;

            if (TargetX >= GPU->FrameBufferWidth || TargetY >= GPU->FrameBufferHeight)
            {
                CursorBackground[Index] = 0;
                continue;
            }

            CursorBackground[Index] = GPU->FrameBuffer[TargetY * GPU->FrameBufferWidth + TargetX];
        }
    }
}

STATIC VOID RestoreCursorBackground(VIRTIO_GPU_INFO *GPU, UINT32 x, UINT32 y)
{
    UINT32 TargetX;
    UINT32 TargetY;
    UINT32 Index;

    for (UINT32 Row = 0; Row < INPUT_CURSOR_REDRAW_SIZE; Row++)
    {
        for (UINT32 Column = 0; Column < INPUT_CURSOR_REDRAW_SIZE; Column++)
        {
            TargetX = x + Column;
            TargetY = y + Row;
            Index = Row * INPUT_CURSOR_REDRAW_SIZE + Column;

            if (TargetX >= GPU->FrameBufferWidth || TargetY >= GPU->FrameBufferHeight)
            {
                continue;
            }

            GPU->FrameBuffer[TargetY * GPU->FrameBufferWidth + TargetX] = CursorBackground[Index];
        }
    }
}

STATIC VOID DrawCursor(VIRTIO_GPU_INFO *GPU, UINT32 x, UINT32 y)
{
    UINT32 TargetX;
    UINT32 TargetY;
    UINT32 Index;

    SaveCursorBackground(GPU, x, y);

    for (UINT32 Row = 0; Row < CURSOR_HEIGHT; Row++)
    {
        for (UINT32 Column = 0; Column < CURSOR_WIDTH; Column++)
        {
            TargetX = x + Column;
            TargetY = y + Row;

            if (TargetX >= GPU->FrameBufferWidth || TargetY >= GPU->FrameBufferHeight)
            {
                continue;
            }

            Index = TargetY * GPU->FrameBufferWidth + TargetX;
            GPU->FrameBuffer[Index] = BlendCursorPixel(GPU->FrameBuffer[Index], CursorBitmap[Row * CURSOR_WIDTH + Column]);
        }
    }
}

MOUSE_STATUS GetMouseStatus(VOID)
{
    MOUSE_STATUS Status;
    UINT64       flags;

    flags = SpinLockAcquireIRQSave(&MouseStatusLock);
    Status = MouseStatus;
    SpinLockReleaseIRQRestore(&MouseStatusLock, flags);

    return Status;
}

KEYBOARD_STATUS GetKeyboardStatus(VOID)
{
    KEYBOARD_STATUS Status;
    UINT64          flags;

    flags = SpinLockAcquireIRQSave(&KeyboardStatusLock);
    Status = KeyboardStatus;
    SpinLockReleaseIRQRestore(&KeyboardStatusLock, flags);

    return Status;
}

VOID HideMouseCursor(VOID)
{
    VIRTIO_GPU_INFO *GPU;
    UINT64           flags;

    GPU = VirtIOGPUGetInfo();

    flags = VirtIOGPUAcquireRenderLock();
    if (GPU == NULL || GPU->FrameBuffer == NULL || !CursorReady)
    {
        VirtIOGPUReleaseRenderLock(flags);
        return;
    }

    RestoreCursorBackground(GPU, CursorDrawX, CursorDrawY);
    FlushCursorBox(GPU, CursorDrawX, CursorDrawY);

    CursorReady = FALSE;
    VirtIOGPUReleaseRenderLock(flags);
}

VOID ShowMouseCursor(VOID)
{
    VIRTIO_GPU_INFO *GPU;
    MOUSE_STATUS status;
    UINT64 flags;

    GPU = VirtIOGPUGetInfo();

    flags = VirtIOGPUAcquireRenderLock();
    if (GPU == NULL || GPU->FrameBuffer == NULL || CursorReady)
    {
        VirtIOGPUReleaseRenderLock(flags);
        return;
    }

    status = GetMouseStatus();

    CursorDrawX = status.X;
    CursorDrawY = status.Y;

    DrawCursor(GPU, CursorDrawX, CursorDrawY);
    FlushCursorBox(GPU, CursorDrawX, CursorDrawY);

    CursorReady = TRUE;
    VirtIOGPUReleaseRenderLock(flags);
}

VOID UpdateMouseCursor(VOID)
{
    VIRTIO_GPU_INFO *GPU;
    MOUSE_STATUS status;
    UINT32 OldX;
    UINT32 OldY;
    UINT64 flags;

    GPU = VirtIOGPUGetInfo();

    flags = VirtIOGPUAcquireRenderLock();
    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        VirtIOGPUReleaseRenderLock(flags);
        return;
    }

    status = GetMouseStatus();

    if (!CursorReady)
    {
        CursorDrawX = status.X;
        CursorDrawY = status.Y;

        DrawCursor(GPU, CursorDrawX, CursorDrawY);
        FlushCursorBox(GPU, CursorDrawX, CursorDrawY);

        CursorReady = TRUE;
        VirtIOGPUReleaseRenderLock(flags);
        return;
    }

    if (CursorDrawX == status.X && CursorDrawY == status.Y)
    {
        VirtIOGPUReleaseRenderLock(flags);
        return;
    }

    OldX = CursorDrawX;
    OldY = CursorDrawY;

    RestoreCursorBackground(GPU, OldX, OldY);

    CursorDrawX = status.X;
    CursorDrawY = status.Y;

    DrawCursor(GPU, CursorDrawX, CursorDrawY);

    FlushCursorUnion(GPU, OldX, OldY, CursorDrawX, CursorDrawY);
    VirtIOGPUReleaseRenderLock(flags);
}

VOID InputMouseHandler(VOID)
{
    VIRTIO_GPU_INFO *GPU;
    VIRTIO_POINTER_EVENT PointerEvent;
    MOUSE_STATUS Status;
    UINT64 flags;

    GPU = VirtIOGPUGetInfo();

    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return;
    }

    VirtIOInputGetTabletRange(&MouseMinX, &MouseMaxX, &MouseMinY, &MouseMaxY);

    while (TRUE)
    {
        while (VirtIOInputGetPointerEvent(&PointerEvent))
        {
            Status.X = ScalePointerCoordinate(PointerEvent.X, MouseMinX, MouseMaxX, GPU->FrameBufferWidth - 1);
            Status.Y = ScalePointerCoordinate(PointerEvent.Y, MouseMinY, MouseMaxY, GPU->FrameBufferHeight - 1);

            Status.LeftButton = PointerEvent.LeftButton;
            Status.RightButton = PointerEvent.RightButton;
            Status.MiddleButton = PointerEvent.MiddleButton;
            Status.Wheel = 0;

            flags = SpinLockAcquireIRQSave(&MouseStatusLock);
            MouseStatus = Status;
            SpinLockReleaseIRQRestore(&MouseStatusLock, flags);
        }

        HLTONCE();
    }
}

VOID InputKeyboardHandler(VOID)
{
    VIRTIO_KEY_EVENT KeyEvent;
    UINT64 flags;

    while (TRUE)
    {
        while (VirtIOInputGetKeyEvent(&KeyEvent))
        {
            flags = SpinLockAcquireIRQSave(&KeyboardStatusLock);
            KeyboardStatus.KeyCode = KeyEvent.Code;
            KeyboardStatus.Pressed = KeyEvent.Pressed;
            SpinLockReleaseIRQRestore(&KeyboardStatusLock, flags);
        }

        HLTONCE();
    }
}
