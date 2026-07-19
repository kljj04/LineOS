// framebuffer.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/bootinfo.h>
#include <render/framebuffer.h>

STATIC UINT32 *FrameBufferBase = 0;
STATIC UINT32 FrameBufferWidth = 0;
STATIC UINT32 FrameBufferHeight = 0;
STATIC UINT32 PixelsPerScanLine = 0;

VOID FrameBufferInit(LINEOS_BOOT_INFO *BootInfo)
{
    FrameBufferBase = (UINT32*)BootInfo->GOP->FrameBufferBase;
    FrameBufferWidth = BootInfo->GOP->ScreenWidth;
    FrameBufferHeight = BootInfo->GOP->ScreenHeight;
    PixelsPerScanLine = BootInfo->GOP->PixelsPerScanLine;
}

STATIC UINT32 FrameBufferOffset(UINT32 x, UINT32 y)
{
    return (y * PixelsPerScanLine) + x;
}

STATIC INT32 Abs(INT32 value)
{
    if (value < 0)
    {
        return -value;
    }

    return value;
}

VOID DrawPixel(UINT32 x, UINT32 y, UINT32 color)
{
    if (FrameBufferBase == 0 || x >= FrameBufferWidth || y >= FrameBufferHeight)
    {
        return;
    }

    FrameBufferBase[FrameBufferOffset(x, y)] = color;
}

UINT32 ReadPixel(UINT32 x, UINT32 y)
{
    if (FrameBufferBase == 0 || x >= FrameBufferWidth || y >= FrameBufferHeight)
    {
        return 0;
    }

    return FrameBufferBase[FrameBufferOffset(x, y)];
}

VOID FillScreen(UINT32 color)
{
    FillRect(0, 0, FrameBufferWidth, FrameBufferHeight, color);
}

VOID FillRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 color)
{
    UINT32 MaxX;
    UINT32 MaxY;

    if (FrameBufferBase == 0 || x >= FrameBufferWidth || y >= FrameBufferHeight)
    {
        return;
    }

    MaxX = x + width;
    MaxY = y + height;

    if (MaxX > FrameBufferWidth || MaxX < x)
    {
        MaxX = FrameBufferWidth;
    }

    if (MaxY > FrameBufferHeight || MaxY < y)
    {
        MaxY = FrameBufferHeight;
    }

    for (UINT32 DrawY = y; DrawY < MaxY; DrawY++)
    {
        UINT32 offset = FrameBufferOffset(x, DrawY);

        for (UINT32 DrawX = x; DrawX < MaxX; DrawX++)
        {
            FrameBufferBase[offset++] = color;
        }
    }
}

VOID DrawRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 color)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    FillRect(x, y, width, 1, color);
    FillRect(x, y + height - 1, width, 1, color);
    FillRect(x, y, 1, height, color);
    FillRect(x + width - 1, y, 1, height, color);
}

VOID DrawLine(INT32 x1, INT32 y1, INT32 x2, INT32 y2, UINT32 color)
{
    INT32 DeltaX = Abs(x2 - x1);
    INT32 DeltaY = -Abs(y2 - y1);
    INT32 StepX = x1 < x2 ? 1 : -1;
    INT32 StepY = y1 < y2 ? 1 : -1;
    INT32 error = DeltaX + DeltaY;

    while (1)
    {
        if (x1 >= 0 && y1 >= 0)
        {
            DrawPixel((UINT32)x1, (UINT32)y1, color);
        }

        if (x1 == x2 && y1 == y2)
        {
            break;
        }

        INT32 Error2 = error * 2;

        if (Error2 >= DeltaY)
        {
            error += DeltaY;
            x1 += StepX;
        }

        if (Error2 <= DeltaX)
        {
            error += DeltaX;
            y1 += StepY;
        }
    }
}

VOID BlendPixel(UINT32 x, UINT32 y, UINT32 color, UINT8 alpha)
{
    UINT32 background;
    UINT32 InverseAlpha;
    UINT32 red;
    UINT32 green;
    UINT32 blue;

    if (alpha == 255)
    {
        DrawPixel(x, y, color);
        return;
    }

    if (alpha == 0 || FrameBufferBase == 0 || x >= FrameBufferWidth || y >= FrameBufferHeight)
    {
        return;
    }

    background = ReadPixel(x, y);
    InverseAlpha = 255 - alpha;

    red = ((((color >> 16) & 0xFF) * alpha) + (((background >> 16) & 0xFF) * InverseAlpha)) / 255;
    green = ((((color >> 8) & 0xFF) * alpha) + (((background >> 8) & 0xFF) * InverseAlpha)) / 255;
    blue = (((color & 0xFF) * alpha) + ((background & 0xFF) * InverseAlpha)) / 255;

    DrawPixel(x, y, (red << 16) | (green << 8) | blue);
}

VOID CopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 width, UINT32 height, UINT32 TargetX, UINT32 TargetY)
{
    INT32 StepX = 1;
    INT32 StepY = 1;
    UINT32 StartX = 0;
    UINT32 StartY = 0;

    if (FrameBufferBase == 0 || width == 0 || height == 0)
    {
        return;
    }

    if (SourceX >= FrameBufferWidth || SourceY >= FrameBufferHeight || TargetX >= FrameBufferWidth || TargetY >= FrameBufferHeight)
    {
        return;
    }

    if (SourceX + width > FrameBufferWidth || SourceX + width < SourceX)
    {
        width = FrameBufferWidth - SourceX;
    }

    if (TargetX + width > FrameBufferWidth || TargetX + width < TargetX)
    {
        width = FrameBufferWidth - TargetX;
    }

    if (SourceY + height > FrameBufferHeight || SourceY + height < SourceY)
    {
        height = FrameBufferHeight - SourceY;
    }

    if (TargetY + height > FrameBufferHeight || TargetY + height < TargetY)
    {
        height = FrameBufferHeight - TargetY;
    }

    if (TargetY > SourceY)
    {
        StepY = -1;
        StartY = height - 1;
    }

    if (TargetY == SourceY && TargetX > SourceX)
    {
        StepX = -1;
        StartX = width - 1;
    }

    for (UINT32 RowIndex = 0; RowIndex < height; RowIndex++)
    {
        UINT32 CopyY = StartY + (RowIndex * StepY);

        for (UINT32 ColumnIndex = 0; ColumnIndex < width; ColumnIndex++)
        {
            UINT32 CopyX = StartX + (ColumnIndex * StepX);
            UINT32 pixel = ReadPixel(SourceX + CopyX, SourceY + CopyY);

            DrawPixel(TargetX + CopyX, TargetY + CopyY, pixel);
        }
    }
}
