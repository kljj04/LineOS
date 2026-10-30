// UserPrograms/Terminal/Terminal/Source/Terminal.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Display.h>
#include <ANSI.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <Asset.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

INT32 main(VOID)
{
    DISPLAY_INFO      Info;
    LOS_ASSET         FontAsset = {0};
    LOS_ASSET         FallbackAsset = {0};
    TRUE_TYPE_FONT    Font = {0};
    TRUE_TYPE_FONT    Fallback = {0};
    TRUE_TYPE_SURFACE Surface;
    TTF_ALLOCATOR     Allocator = {TerminalAllocate, TerminalFree, NULL};
    UINT32            X = TERMINAL_MARGIN;
    UINT32            Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT;
    FB_VAR_SCREENINFO Variable;
    FB_FIX_SCREENINFO Fixed;
    FramebufferFD = open("/dev/fb0", O_RDWR);
    if (FramebufferFD < 0 || ioctl(FramebufferFD, FBIOGET_VSCREENINFO, &Variable) < 0 || ioctl(FramebufferFD, FBIOGET_FSCREENINFO, &Fixed) < 0 || Variable.BitsPerPixel != 32 || Variable.Red.Offset != 16 || Variable.Green.Offset != 8 || Variable.Blue.Offset != 0 || Variable.Red.Length != 8 || Variable.Green.Length != 8 || Variable.Blue.Length != 8 || Fixed.LineLength != (UINT64) Variable.XRes * sizeof(UINT32) || Fixed.SmemLen != (UINT64) Fixed.LineLength * Variable.YRes)
    {
        puts("Terminal: framebuffer initialization failed");
        return 1;
    }
    Info.Width = Variable.XRes;
    Info.Height = Variable.YRes;
    Info.Pitch = Fixed.LineLength;
    Info.Format = DISPLAY_FORMAT_BGRA8888;

    if (Info.Width < TERMINAL_MARGIN * 2U + TERMINAL_FONT_HEIGHT || Info.Height < TERMINAL_LINE_HEIGHT * 2U || Info.Width > TERMINAL_MAX_WIDTH || Info.Height > TERMINAL_MAX_HEIGHT || Info.Format != DISPLAY_FORMAT_BGRA8888 || LOSAssetAcquire("Fonts/JetBrainsMono", &FontAsset) != 0 || LOSAssetAcquire("Fonts/Pretendard", &FallbackAsset) != 0)
    {
        puts("Terminal: initialization failed");
        return 1;
    }

    MappedBuffer = mmap(NULL, Fixed.SmemLen, PROT_READ | PROT_WRITE, MAP_SHARED, FramebufferFD, 0);
    if (MappedBuffer == MAP_FAILED)
    {
        puts("Terminal: framebuffer mapping failed");
        return 1;
    }

    Surface.FrameBuffer = FrameBuffer;
    Surface.FrameBufferWidth = Info.Width;
    Surface.FrameBufferHeight = Info.Height;

    if (!TrueTypeInit(&Allocator, &Surface) || !TrueTypeRegisterFont(&Font, FontAsset.Data) || !TrueTypeRegisterFont(&Fallback, FallbackAsset.Data))
    {
        puts("Terminal: font initialization failed");
        return 1;
    }

    FLOAT32 Scale = ScaleForPixelHeight(&Font.Info, TERMINAL_FONT_HEIGHT);
    INT32   Ascent;
    INT32   Descent;
    INT32   LineGap;
    GetFontVMetrics(&Font.Info, &Ascent, &Descent, &LineGap);
    CellAscent = (UINT32) TTFFloor(Ascent * Scale + 0.5f);

    for (UINT32 Character = 32; Character < 127; Character++)
    {
        INT32 Advance;
        INT32 Bearing;
        GetCodepointHMetrics(&Font.Info, Character, &Advance, &Bearing);
        INT32 Width = TTFFloor((Advance * Scale) + 0.5f);
        if (Width <= 0)
        {
            return 1;
        }
        Glyphs[Character].Advance = (UINT32) Width;
        GetCodepointBitmapBox(&Font.Info, Character, Scale, Scale, &Glyphs[Character].Left, &Glyphs[Character].Top, &Glyphs[Character].Right, &Glyphs[Character].Bottom);
    }

    for (UINTN Index = 0; Index < (UINTN) Info.Width * Info.Height; Index++)
    {
        FrameBuffer[Index] = TERMINAL_BACKGROUND;
    }

    if (TerminalPresent(&Info, X, Baseline, TRUE) < 0)
    {
        return 1;
    }

    return TerminalRun(&Info, &Font, &Fallback);
}
