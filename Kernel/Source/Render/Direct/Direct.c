// Kernel/Source/Render/Direct/Direct.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Memory/PMM.h>
#include <Render/Direct/Direct.h>
#include <Render/GPU/VirtIOGPU.h>

STATIC UINT32 FrameBufferOffset(CONST VIRTIO_GPU_INFO *GPU, UINT32 X, UINT32 Y)
{
    return (Y * GPU->FrameBufferWidth) + X;
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

VOID FillScreen(UINT32 Color)
{
    CONST VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    UINT64                 PixelCount;
    UINT8                  Alpha;
    UINT64                 Flags;

    Flags = VirtIOGPUAcquireRenderLock();

    if (GPU->FrameBuffer == NULL)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    Alpha = GetColorAlpha(Color);
    PixelCount = (UINT64) GPU->FrameBufferWidth * GPU->FrameBufferHeight;

    if (Alpha == 255)
    {
        FillRow(GPU->FrameBuffer, (UINT32) PixelCount, MakePixel(GetColorRGB(Color)));
        VirtIOGPUMarkDirtyLocked(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    for (UINT64 Index = 0; Index < PixelCount; Index++)
    {
        GPU->FrameBuffer[Index] = BlendColor(GPU->FrameBuffer[Index], Color);
    }

    VirtIOGPUMarkDirtyLocked(0, 0, GPU->FrameBufferWidth, GPU->FrameBufferHeight);
    VirtIOGPUReleaseRenderLock(Flags);
}

VOID DrawPixel(UINT32 X, UINT32 Y, UINT32 Color)
{
    CONST VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    UINT64                 Flags;

    Flags = VirtIOGPUAcquireRenderLock();

    if (GPU->FrameBuffer == NULL || X >= GPU->FrameBufferWidth || Y >= GPU->FrameBufferHeight)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    UINT32 Offset = FrameBufferOffset(GPU, X, Y);

    GPU->FrameBuffer[Offset] = BlendColor(GPU->FrameBuffer[Offset], Color);
    VirtIOGPUMarkDirtyLocked(X, Y, 1, 1);
    VirtIOGPUReleaseRenderLock(Flags);
}

VOID FillRect(UINT32 X, UINT32 Y, UINT32 Width, UINT32 Height, UINT32 Color)
{
    CONST VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    UINT32                 MaxX;
    UINT32                 MaxY;
    UINT8                  Alpha = GetColorAlpha(Color);
    UINT32                 Pixel = MakePixel(GetColorRGB(Color));
    UINT64                 Flags;

    Flags = VirtIOGPUAcquireRenderLock();

    if (GPU->FrameBuffer == NULL || X >= GPU->FrameBufferWidth || Y >= GPU->FrameBufferHeight)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    MaxX = X + Width;
    MaxY = Y + Height;

    if (MaxX > GPU->FrameBufferWidth || MaxX < X)
    {
        MaxX = GPU->FrameBufferWidth;
    }

    if (MaxY > GPU->FrameBufferHeight || MaxY < Y)
    {
        MaxY = GPU->FrameBufferHeight;
    }

    for (UINT32 Row = Y; Row < MaxY; Row++)
    {
        UINT32 *Line = &GPU->FrameBuffer[FrameBufferOffset(GPU, X, Row)];

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

    VirtIOGPUMarkDirtyLocked(X, Y, MaxX - X, MaxY - Y);
    VirtIOGPUReleaseRenderLock(Flags);
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

STATIC VOID DrawPixelWithCoverage(INT32 X, INT32 Y, UINT32 Color, UINT32 Coverage, UINT32 MaxCoverage)
{
    UINT32 Alpha;

    if (Coverage == 0 || MaxCoverage == 0 || X < 0 || Y < 0)
    {
        return;
    }

    Alpha = ((UINT32) GetColorAlpha(Color) * Coverage) / MaxCoverage;
    DrawPixel((UINT32) X, (UINT32) Y, (GetColorRGB(Color) << 8) | Alpha);
}

VOID DrawCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color)
{
    INT32 RadiusOuter = ((INT32) Radius * 4) + 2;
    INT32 RadiusInner = ((INT32) Radius * 4) - 2;
    INT32 OuterSquared = RadiusOuter * RadiusOuter;
    INT32 InnerSquared = RadiusInner * RadiusInner;
    INT32 SampleOffsets[4] = {-2, -1, 1, 2};

    if (Radius == 0)
    {
        DrawPixel((UINT32) CenterX, (UINT32) CenterY, Color);

        return;
    }

    for (INT32 Y = -(INT32) Radius - 1; Y <= (INT32) Radius + 1; Y++)
    {
        for (INT32 X = -(INT32) Radius - 1; X <= (INT32) Radius + 1; X++)
        {
            UINT32 Coverage = 0;

            for (UINT32 SampleY = 0; SampleY < 4; SampleY++)
            {
                for (UINT32 SampleX = 0; SampleX < 4; SampleX++)
                {
                    INT32 SubX = (X * 4) + SampleOffsets[SampleX];
                    INT32 SubY = (Y * 4) + SampleOffsets[SampleY];
                    INT32 Distance = (SubX * SubX) + (SubY * SubY);

                    if (Distance <= OuterSquared && Distance >= InnerSquared)
                    {
                        Coverage++;
                    }
                }
            }

            DrawPixelWithCoverage(CenterX + X, CenterY + Y, Color, Coverage, 16);
        }
    }
}

VOID FillCircle(INT32 CenterX, INT32 CenterY, UINT32 Radius, UINT32 Color)
{
    INT32 RadiusScaled = (INT32) Radius * 4;
    INT32 RadiusSquared = RadiusScaled * RadiusScaled;
    INT32 SampleOffsets[4] = {-2, -1, 1, 2};

    for (INT32 Y = -(INT32) Radius; Y <= (INT32) Radius; Y++)
    {
        for (INT32 X = -(INT32) Radius; X <= (INT32) Radius; X++)
        {
            UINT32 Coverage = 0;

            for (UINT32 SampleY = 0; SampleY < 4; SampleY++)
            {
                for (UINT32 SampleX = 0; SampleX < 4; SampleX++)
                {
                    INT32 SubX = (X * 4) + SampleOffsets[SampleX];
                    INT32 SubY = (Y * 4) + SampleOffsets[SampleY];

                    if ((SubX * SubX) + (SubY * SubY) <= RadiusSquared)
                    {
                        Coverage++;
                    }
                }
            }

            DrawPixelWithCoverage(CenterX + X, CenterY + Y, Color, Coverage, 16);
        }
    }
}

VOID VirtIOGPUBlendPixel(UINT32 X, UINT32 Y, UINT32 Color, UINT8 Alpha)
{
    DrawPixel(X, Y, (GetColorRGB(Color) << 8) | Alpha);
}

VOID CopyRect(UINT32 SourceX, UINT32 SourceY, UINT32 Width, UINT32 Height, UINT32 TargetX, UINT32 TargetY)
{
    CONST VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    UINTN                  CopySize;
    UINT64                 Flags;

    Flags = VirtIOGPUAcquireRenderLock();

    if (GPU->FrameBuffer == NULL || Width == 0 || Height == 0)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    if (SourceX >= GPU->FrameBufferWidth || SourceY >= GPU->FrameBufferHeight || TargetX >= GPU->FrameBufferWidth || TargetY >= GPU->FrameBufferHeight)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    if (SourceX + Width > GPU->FrameBufferWidth || SourceX + Width < SourceX)
    {
        Width = GPU->FrameBufferWidth - SourceX;
    }

    if (TargetX + Width > GPU->FrameBufferWidth || TargetX + Width < TargetX)
    {
        Width = GPU->FrameBufferWidth - TargetX;
    }

    if (SourceY + Height > GPU->FrameBufferHeight || SourceY + Height < SourceY)
    {
        Height = GPU->FrameBufferHeight - SourceY;
    }

    if (TargetY + Height > GPU->FrameBufferHeight || TargetY + Height < TargetY)
    {
        Height = GPU->FrameBufferHeight - TargetY;
    }

    if (Width == 0 || Height == 0)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    CopySize = (UINTN) Width * sizeof(UINT32);

    if (TargetY > SourceY)
    {
        for (UINT32 RowIndex = Height; RowIndex > 0; RowIndex--)
        {
            UINT32  CopyY = RowIndex - 1;
            UINT32 *Destination = &GPU->FrameBuffer[FrameBufferOffset(GPU, TargetX, TargetY + CopyY)];
            UINT32 *Source = &GPU->FrameBuffer[FrameBufferOffset(GPU, SourceX, SourceY + CopyY)];

            KMemMove(Destination, Source, CopySize);
        }

        VirtIOGPUMarkDirtyLocked(TargetX, TargetY, Width, Height);
        VirtIOGPUReleaseRenderLock(Flags);

        return;
    }

    for (UINT32 RowIndex = 0; RowIndex < Height; RowIndex++)
    {
        UINT32 *Destination = &GPU->FrameBuffer[FrameBufferOffset(GPU, TargetX, TargetY + RowIndex)];
        UINT32 *Source = &GPU->FrameBuffer[FrameBufferOffset(GPU, SourceX, SourceY + RowIndex)];

        if (Destination == Source)
        {
            continue;
        }

        KMemMove(Destination, Source, CopySize);
    }

    VirtIOGPUMarkDirtyLocked(TargetX, TargetY, Width, Height);
    VirtIOGPUReleaseRenderLock(Flags);
}

UINT32 ReadPixel(UINT32 X, UINT32 Y)
{
    CONST VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    UINT32                 Pixel;
    UINT64                 Flags;

    Flags = VirtIOGPUAcquireRenderLock();

    if (GPU->FrameBuffer == NULL || X >= GPU->FrameBufferWidth || Y >= GPU->FrameBufferHeight)
    {
        VirtIOGPUReleaseRenderLock(Flags);

        return 0;
    }

    Pixel = GPU->FrameBuffer[FrameBufferOffset(GPU, X, Y)];
    VirtIOGPUReleaseRenderLock(Flags);

    return Pixel;
}
