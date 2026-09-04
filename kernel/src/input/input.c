// kernel/src/input/input.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <cursor/cursor.h>
#include <debug/panic.h>
#include <input/virtio_input.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <input/input.h>

#define INPUT_CURSOR_REDRAW_SIZE 64

STATIC UINT32  MouseMinX = 0;
STATIC UINT32  MouseMaxX = 32767;
STATIC UINT32  MouseMinY = 0;
STATIC UINT32  MouseMaxY = 32767;
STATIC UINT32  MouseX = 0;
STATIC UINT32  MouseY = 0;
STATIC BOOLEAN ShiftPressed = FALSE;
STATIC BOOLEAN CursorReady = FALSE;
STATIC UINT32  CursorBackground[INPUT_CURSOR_REDRAW_SIZE * INPUT_CURSOR_REDRAW_SIZE];

STATIC CHAR16 KeyCodeToChar(UINT16 code, BOOLEAN shift)
{
    switch (code)
    {
    case 2:
        return shift ? L'!' : L'1';
    case 3:
        return shift ? L'@' : L'2';
    case 4:
        return shift ? L'#' : L'3';
    case 5:
        return shift ? L'$' : L'4';
    case 6:
        return shift ? L'%' : L'5';
    case 7:
        return shift ? L'^' : L'6';
    case 8:
        return shift ? L'&' : L'7';
    case 9:
        return shift ? L'*' : L'8';
    case 10:
        return shift ? L'(' : L'9';
    case 11:
        return shift ? L')' : L'0';
    case 12:
        return shift ? L'_' : L'-';
    case 13:
        return shift ? L'+' : L'=';
    case 16:
        return shift ? L'Q' : L'q';
    case 17:
        return shift ? L'W' : L'w';
    case 18:
        return shift ? L'E' : L'e';
    case 19:
        return shift ? L'R' : L'r';
    case 20:
        return shift ? L'T' : L't';
    case 21:
        return shift ? L'Y' : L'y';
    case 22:
        return shift ? L'U' : L'u';
    case 23:
        return shift ? L'I' : L'i';
    case 24:
        return shift ? L'O' : L'o';
    case 25:
        return shift ? L'P' : L'p';
    case 26:
        return shift ? L'{' : L'[';
    case 27:
        return shift ? L'}' : L']';
    case 30:
        return shift ? L'A' : L'a';
    case 31:
        return shift ? L'S' : L's';
    case 32:
        return shift ? L'D' : L'd';
    case 33:
        return shift ? L'F' : L'f';
    case 34:
        return shift ? L'G' : L'g';
    case 35:
        return shift ? L'H' : L'h';
    case 36:
        return shift ? L'J' : L'j';
    case 37:
        return shift ? L'K' : L'k';
    case 38:
        return shift ? L'L' : L'l';
    case 39:
        return shift ? L':' : L';';
    case 40:
        return shift ? L'"' : L'\'';
    case 41:
        return shift ? L'~' : L'`';
    case 43:
        return shift ? L'|' : L'\\';
    case 44:
        return shift ? L'Z' : L'z';
    case 45:
        return shift ? L'X' : L'x';
    case 46:
        return shift ? L'C' : L'c';
    case 47:
        return shift ? L'V' : L'v';
    case 48:
        return shift ? L'B' : L'b';
    case 49:
        return shift ? L'N' : L'n';
    case 50:
        return shift ? L'M' : L'm';
    case 51:
        return shift ? L'<' : L',';
    case 52:
        return shift ? L'>' : L'.';
    case 53:
        return shift ? L'?' : L'/';
    case 57:
        return L' ';
    }

    return 0;
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

STATIC VOID FlushCursorBox(UINT32 x, UINT32 y)
{
    VirtIOGPUTransferRect(x, y, INPUT_CURSOR_REDRAW_SIZE, INPUT_CURSOR_REDRAW_SIZE);
    VirtIOGPUFlushRect(x, y, INPUT_CURSOR_REDRAW_SIZE, INPUT_CURSOR_REDRAW_SIZE);
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
    for (UINT32 Row = 0; Row < INPUT_CURSOR_REDRAW_SIZE; Row++)
    {
        for (UINT32 Column = 0; Column < INPUT_CURSOR_REDRAW_SIZE; Column++)
        {
            UINT32 TargetX = x + Column;
            UINT32 TargetY = y + Row;
            UINT32 Index = Row * INPUT_CURSOR_REDRAW_SIZE + Column;

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
    for (UINT32 Row = 0; Row < INPUT_CURSOR_REDRAW_SIZE; Row++)
    {
        for (UINT32 Column = 0; Column < INPUT_CURSOR_REDRAW_SIZE; Column++)
        {
            UINT32 TargetX = x + Column;
            UINT32 TargetY = y + Row;
            UINT32 Index = Row * INPUT_CURSOR_REDRAW_SIZE + Column;

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
    SaveCursorBackground(GPU, x, y);

    for (UINT32 Row = 0; Row < CURSOR_HEIGHT; Row++)
    {
        for (UINT32 Column = 0; Column < CURSOR_WIDTH; Column++)
        {
            UINT32 TargetX = x + Column;
            UINT32 TargetY = y + Row;
            UINT32 Index;

            if (TargetX >= GPU->FrameBufferWidth || TargetY >= GPU->FrameBufferHeight)
            {
                continue;
            }

            Index = TargetY * GPU->FrameBufferWidth + TargetX;
            GPU->FrameBuffer[Index] = BlendCursorPixel(GPU->FrameBuffer[Index], CursorBitmap[Row * CURSOR_WIDTH + Column]);
        }
    }
}

VOID InputMouseHandler(VOID)
{
    VIRTIO_GPU_INFO     *GPU;
    VIRTIO_POINTER_EVENT PointerEvent;
    UINT32               OldX;
    UINT32               OldY;

    GPU = VirtIOGPUGetInfo();
    if (GPU == NULL || GPU->FrameBuffer == NULL)
    {
        return;
    }

    if (!CursorReady)
    {
        VirtIOInputGetTabletRange(&MouseMinX, &MouseMaxX, &MouseMinY, &MouseMaxY);
        DrawCursor(GPU, MouseX, MouseY);
        FlushCursorBox(MouseX, MouseY);
        CursorReady = TRUE;
    }

    while (TRUE)
    {
        while (VirtIOInputGetPointerEvent(&PointerEvent))
        {
            OldX = MouseX;
            OldY = MouseY;

            MouseX = ScalePointerCoordinate(PointerEvent.X, MouseMinX, MouseMaxX, GPU->FrameBufferWidth - 1);
            MouseY = ScalePointerCoordinate(PointerEvent.Y, MouseMinY, MouseMaxY, GPU->FrameBufferHeight - 1);

            CLI();
            RestoreCursorBackground(GPU, OldX, OldY);
            FlushCursorBox(OldX, OldY);
            DrawCursor(GPU, MouseX, MouseY);
            FlushCursorBox(MouseX, MouseY);
            STI();
        }

        HLTONCE();
    }
}

VOID InputKeyboardHandler(VOID)
{
    VIRTIO_GPU_INFO *GPU;
    VIRTIO_KEY_EVENT KeyEvent;
    CHAR16           Character;
    CHAR16           Text[2];

    GPU = VirtIOGPUGetInfo();
    Text[0] = 0;
    Text[1] = 0;

    while (TRUE)
    {
        while (VirtIOInputGetKeyEvent(&KeyEvent))
        {
            if (KeyEvent.Code == 42 || KeyEvent.Code == 54)
            {
                ShiftPressed = KeyEvent.Pressed;
                continue;
            }

            if (!KeyEvent.Pressed)
            {
                continue;
            }

            Character = KeyCodeToChar(KeyEvent.Code, ShiftPressed);
            if (Character == 0)
            {
                continue;
            }

            Text[0] = Character;
            CLI();
            if (GPU != NULL && GPU->FrameBuffer != NULL && CursorReady)
            {
                RestoreCursorBackground(GPU, MouseX, MouseY);
            }
            KPrint(Text, MouseX, MouseY, 0xFFFFFFFF, 32, PRETENDARD);
            if (GPU != NULL && GPU->FrameBuffer != NULL && CursorReady)
            {
                DrawCursor(GPU, MouseX, MouseY);
            }
            FlushCursorBox(MouseX, MouseY);
            STI();
        }

        HLTONCE();
    }
}
