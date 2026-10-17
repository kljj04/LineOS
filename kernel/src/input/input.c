// kernel/src/input/input.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/AMD64/CPU.h>
#include <arch/AMD64/SpinLock.h>
#include <cursor/cursor.h>
#include <input/VirtIOInput.h>
#include <render/GPU/VirtIOGPU.h>
#include <input/input.h>

#include "debug/debug.h"

#define INPUT_CURSOR_SIZE 64

STATIC UINT32 MouseMinX = 0;
STATIC UINT32 MouseMaxX = 32767;
STATIC UINT32 MouseMinY = 0;
STATIC UINT32 MouseMaxY = 32767;

STATIC MOUSE_STATUS    MouseStatus = {0};
STATIC KEYBOARD_STATUS KeyboardStatus = {0};

STATIC SPIN_LOCK MouseStatusLock;
STATIC SPIN_LOCK KeyboardStatusLock;
STATIC SPIN_LOCK CursorLock;

STATIC BOOLEAN CursorReady = FALSE;
STATIC BOOLEAN CursorVisible = FALSE;
STATIC UINT32  CursorDrawX = 0;
STATIC UINT32  CursorDrawY = 0;

STATIC UINT32 ScalePointerCoordinate(UINT32 Value, UINT32 Min, UINT32 Max, UINT32 OutputMax)
{
    if (Max <= Min || Value <= Min)
    {
        return 0;
    }

    if (Value >= Max)
    {
        return OutputMax;
    }

    return (UINT32) (((UINT64) (Value - Min) * OutputMax) / (Max - Min));
}

MOUSE_STATUS GetMouseStatus(VOID)
{
    MOUSE_STATUS Status;
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&MouseStatusLock);
    Status = MouseStatus;
    SpinLockReleaseIRQRestore(&MouseStatusLock, Flags);

    return Status;
}

KEYBOARD_STATUS GetKeyboardStatus(VOID)
{
    KEYBOARD_STATUS Status;
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&KeyboardStatusLock);
    Status = KeyboardStatus;
    SpinLockReleaseIRQRestore(&KeyboardStatusLock, Flags);

    return Status;
}

STATIC BOOLEAN InitHardwareCursor(VOID)
{
    MOUSE_STATUS Status;

    if (CursorReady)
    {
        return TRUE;
    }

    if (CURSOR_WIDTH > INPUT_CURSOR_SIZE || CURSOR_HEIGHT > INPUT_CURSOR_SIZE)
    {
        return FALSE;
    }

    Status = GetMouseStatus();

    if (!VirtIOGPUCreateCursor(INPUT_CURSOR_SIZE, INPUT_CURSOR_SIZE, 0, 0))
    {
        return FALSE;
    }

    if (!VirtIOGPUSetCursorImage(CursorBitmap, CURSOR_WIDTH, CURSOR_HEIGHT, 0, 0, Status.X, Status.Y))
    {
        return FALSE;
    }

    CursorDrawX = Status.X;
    CursorDrawY = Status.Y;
    CursorReady = TRUE;
    CursorVisible = TRUE;

    DebugWriteLine("[CURSOR] Moving to center");
    if (VirtIOGPUMoveCursor(VirtIOGPUGetFrameBufferWidth() / 2, VirtIOGPUGetFrameBufferHeight() / 2))
    {
        DebugWriteLine("[CURSOR] Move succeeded");
    }
    else
    {
        DebugWriteLine("[CURSOR] Move failed");
    }

    return TRUE;
}

VOID HideMouseCursor(VOID)
{
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&CursorLock);

    if (CursorReady && CursorVisible)
    {
        if (VirtIOGPUHideCursor())
        {
            CursorVisible = FALSE;
        }
    }

    SpinLockReleaseIRQRestore(&CursorLock, Flags);
}

VOID ShowMouseCursor(VOID)
{
    MOUSE_STATUS Status;
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&CursorLock);

    if (!InitHardwareCursor())
    {
        SpinLockReleaseIRQRestore(&CursorLock, Flags);
        return;
    }

    if (!CursorVisible)
    {
        Status = GetMouseStatus();

        if (VirtIOGPUUpdateCursor(Status.X, Status.Y))
        {
            CursorDrawX = Status.X;
            CursorDrawY = Status.Y;
            CursorVisible = TRUE;
        }
    }

    SpinLockReleaseIRQRestore(&CursorLock, Flags);
}

VOID UpdateMouseCursor(VOID)
{
    MOUSE_STATUS Status;
    UINT64 Flags;

    Flags = SpinLockAcquireIRQSave(&CursorLock);

    if (!InitHardwareCursor())
    {
        SpinLockReleaseIRQRestore(&CursorLock, Flags);
        return;
    }

    if (!CursorVisible)
    {
        SpinLockReleaseIRQRestore(&CursorLock, Flags);
        return;
    }

    Status = GetMouseStatus();

    if (CursorDrawX != Status.X || CursorDrawY != Status.Y)
    {
        if (VirtIOGPUUpdateCursor(Status.X, Status.Y))
        {
            CursorDrawX = Status.X;
            CursorDrawY = Status.Y;
        }
    }

    SpinLockReleaseIRQRestore(&CursorLock, Flags);
}

VOID InputMouseHandler(VOID)
{
    VIRTIO_GPU_INFO      *GPU;
    VIRTIO_POINTER_EVENT  PointerEvent;
    MOUSE_STATUS          Status;
    UINT64                Flags;

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

            Flags = SpinLockAcquireIRQSave(&MouseStatusLock);
            MouseStatus = Status;
            SpinLockReleaseIRQRestore(&MouseStatusLock, Flags);

            UpdateMouseCursor();
        }

        HLTONCE();
    }
}

VOID InputKeyboardHandler(VOID)
{
    VIRTIO_KEY_EVENT KeyEvent;
    UINT64 Flags;

    while (TRUE)
    {
        while (VirtIOInputGetKeyEvent(&KeyEvent))
        {
            Flags = SpinLockAcquireIRQSave(&KeyboardStatusLock);

            KeyboardStatus.KeyCode = KeyEvent.Code;
            KeyboardStatus.Pressed = KeyEvent.Pressed;

            SpinLockReleaseIRQRestore(&KeyboardStatusLock, Flags);
        }

        HLTONCE();
    }
}
