// UserPrograms/Terminal/Terminal.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeEngine.h>
#include <Display.h>
#include <unistd.h>
#include <Asset.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERMINAL_MAX_WIDTH          2560U
#define TERMINAL_MAX_HEIGHT         1440U
#define TERMINAL_ALLOCATOR_CAPACITY (8U * 1024U * 1024U)
#define TERMINAL_FONT_HEIGHT        36U
#define TERMINAL_LINE_HEIGHT        40U
#define TERMINAL_CURSOR_WIDTH       5U
#define TERMINAL_CURSOR_HEIGHT      30U
#define TERMINAL_MARGIN             8U
#define TERMINAL_BACKGROUND         0x00101820U
#define TERMINAL_FOREGROUND         0x00FFFFFFU

typedef struct
{
    UINT32 Advance;
    INT32  Left;
    INT32  Top;
    INT32  Right;
    INT32  Bottom;
} TERMINAL_GLYPH;

typedef struct
{
    UINT32         X;
    TERMINAL_GLYPH Glyph;
} TERMINAL_CELL;

STATIC TERMINAL_GLYPH Glyphs[128];
STATIC TERMINAL_CELL  LineCells[TERMINAL_MAX_WIDTH];

STATIC UINT32 FrameBuffer[TERMINAL_MAX_WIDTH * TERMINAL_MAX_HEIGHT];
STATIC UINT8  AllocatorBuffer[TERMINAL_ALLOCATOR_CAPACITY];
STATIC UINTN  AllocatorUsed;

STATIC VOID *TerminalAllocate(UINTN Size, VOID *Context)
{
    UINTN Aligned;
    VOID *Result;

    (VOID) Context;

    if (Size > TERMINAL_ALLOCATOR_CAPACITY - 15U)
    {
        return NULL;
    }

    Aligned = (Size + 15U) & ~(UINTN) 15U;

    if (AllocatorUsed > TERMINAL_ALLOCATOR_CAPACITY - Aligned)
    {
        return NULL;
    }

    Result = &AllocatorBuffer[AllocatorUsed];
    AllocatorUsed += Aligned;

    return Result;
}

STATIC VOID TerminalFree(VOID *Pointer, VOID *Context)
{
    (VOID) Pointer;
    (VOID) Context;
}

STATIC VOID TerminalNewLine(DISPLAY_INFO *Info, UINT32 *X, UINT32 *Baseline)
{
    *X = TERMINAL_MARGIN;
    *Baseline += TERMINAL_LINE_HEIGHT;

    if (*Baseline + TERMINAL_LINE_HEIGHT > Info->Height)
    {
        UINTN Rows = Info->Height - TERMINAL_LINE_HEIGHT;

        memmove(FrameBuffer, FrameBuffer + TERMINAL_LINE_HEIGHT * Info->Width, Rows * Info->Width * sizeof(UINT32));

        for (UINTN Index = Rows * Info->Width; Index < (UINTN) Info->Height * Info->Width; Index++)
        {
            FrameBuffer[Index] = TERMINAL_BACKGROUND;
        }

        *Baseline -= TERMINAL_LINE_HEIGHT;
    }
}

STATIC VOID TerminalErase(DISPLAY_INFO *Info, TERMINAL_GLYPH *Glyph, UINT32 X, UINT32 Baseline)
{
    INT64 Left = (INT64) X + Glyph->Left;
    INT64 Right = (INT64) X + Glyph->Right;
    INT64 Top = (INT64) Baseline + Glyph->Top;
    INT64 Bottom = (INT64) Baseline + Glyph->Bottom;

    if (Left < 0)
    {
        Left = 0;
    }
    if (Top < 0)
    {
        Top = 0;
    }
    if (Right > Info->Width)
    {
        Right = Info->Width;
    }
    if (Bottom > Info->Height)
    {
        Bottom = Info->Height;
    }
    for (INT64 Row = Top; Row < Bottom; Row++)
    {
        for (INT64 Column = Left; Column < Right; Column++)
        {
            FrameBuffer[(UINTN) Row * Info->Width + Column] = TERMINAL_BACKGROUND;
        }
    }
}

typedef struct
{
    UINT32 Value;
    UINT32 Minimum;
    UINT32 Remaining;
} TERMINAL_UTF8;

STATIC BOOLEAN TerminalDecode(TERMINAL_UTF8 *Decoder, UINT8 Byte, UINT32 *Codepoint, BOOLEAN *Retry)
{
    *Retry = FALSE;
    if (Decoder->Remaining != 0)
    {
        if ((Byte & 0xC0) != 0x80)
        {
            Decoder->Remaining = 0;
            *Retry = TRUE;
            *Codepoint = 0xFFFD;
            return TRUE;
        }
        Decoder->Value = (Decoder->Value << 6) | (Byte & 63);
        if (--Decoder->Remaining != 0)
        {
            return FALSE;
        }
        *Codepoint = Decoder->Value;
        if (*Codepoint < Decoder->Minimum || *Codepoint > 0x10FFFF || (*Codepoint >= 0xD800 && *Codepoint <= 0xDFFF))
        {
            *Codepoint = 0xFFFD;
        }
        return TRUE;
    }
    if (Byte < 0x80)
    {
        *Codepoint = Byte;
        return TRUE;
    }
    if (Byte >= 0xC2 && Byte <= 0xDF)
    {
        Decoder->Value = Byte & 31;
        Decoder->Remaining = 1;
        Decoder->Minimum = 0x80;
    }
    else if (Byte >= 0xE0 && Byte <= 0xEF)
    {
        Decoder->Value = Byte & 15;
        Decoder->Remaining = 2;
        Decoder->Minimum = 0x800;
    }
    else if (Byte >= 0xF0 && Byte <= 0xF4)
    {
        Decoder->Value = Byte & 7;
        Decoder->Remaining = 3;
        Decoder->Minimum = 0x10000;
    }
    else
    {
        *Codepoint = 0xFFFD;
        return TRUE;
    }
    return FALSE;
}

STATIC TRUE_TYPE_FONT *TerminalMeasure(TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 Codepoint, TERMINAL_GLYPH *Glyph)
{
    if (Codepoint < 128)
    {
        *Glyph = Glyphs[Codepoint];
        return Font;
    }
    TRUE_TYPE_FONT *Selected = FindGlyphIndex(&Font->Info, (INT32) Codepoint) != 0 ? Font : Fallback;
    FLOAT32         Scale = ScaleForPixelHeight(&Selected->Info, TERMINAL_FONT_HEIGHT);
    INT32           Advance;
    INT32           Bearing;
    GetCodepointHMetrics(&Selected->Info, (INT32) Codepoint, &Advance, &Bearing);
    INT32 Width = TTFFloor((Advance * Scale) + 0.5f);
    Glyph->Advance = Width > 0 ? (UINT32) Width : 1U;
    GetCodepointBitmapBox(&Selected->Info, (INT32) Codepoint, Scale, Scale, &Glyph->Left, &Glyph->Top, &Glyph->Right, &Glyph->Bottom);
    return Selected;
}

STATIC INT64 TerminalPresent(DISPLAY_INFO *Info, UINT32 X, UINT32 Baseline)
{
    UINT32 Saved[TERMINAL_CURSOR_WIDTH * TERMINAL_CURSOR_HEIGHT];
    UINT32 Top = Baseline - TERMINAL_CURSOR_HEIGHT + 5U;
    UINTN  Count = 0;

    for (UINT32 Row = Top; Row < Top + TERMINAL_CURSOR_HEIGHT && Row < Info->Height; Row++)
    {
        for (UINT32 Column = X; Column < X + TERMINAL_CURSOR_WIDTH && Column < Info->Width; Column++)
        {
            UINTN Pixel = (UINTN) Row * Info->Width + Column;
            Saved[Count++] = FrameBuffer[Pixel];
            FrameBuffer[Pixel] = TERMINAL_FOREGROUND;
        }
    }

    INT64 Result = DisplayPresent(FrameBuffer, (UINT64) Info->Width * Info->Height * sizeof(UINT32));
    Count = 0;
    for (UINT32 Row = Top; Row < Top + TERMINAL_CURSOR_HEIGHT && Row < Info->Height; Row++)
    {
        for (UINT32 Column = X; Column < X + TERMINAL_CURSOR_WIDTH && Column < Info->Width; Column++)
        {
            FrameBuffer[(UINTN) Row * Info->Width + Column] = Saved[Count++];
        }
    }
    return Result;
}

INT32 _Start(VOID)
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
    CHAR8             Buffer[256];
    UINT32            Cursor = 0;
    UINT32            LineLength = 0;
    TERMINAL_UTF8     Decoder = {0};

    if (DisplayGetInfo(&Info) < 0 || Info.Width < TERMINAL_MARGIN * 2U + TERMINAL_FONT_HEIGHT || Info.Height < TERMINAL_LINE_HEIGHT * 2U || Info.Width > TERMINAL_MAX_WIDTH || Info.Height > TERMINAL_MAX_HEIGHT || Info.Format != DISPLAY_FORMAT_BGRA8888 || LOSAssetAcquire("Fonts/JetBrainsMono", &FontAsset) != 0 || LOSAssetAcquire("Fonts/Pretendard", &FallbackAsset) != 0)
    {
        puts("Terminal: initialization failed");
        exit(1);
    }

    Surface.FrameBuffer = FrameBuffer;
    Surface.FrameBufferWidth = Info.Width;
    Surface.FrameBufferHeight = Info.Height;

    if (!TrueTypeInit(&Allocator, &Surface) || !TrueTypeRegisterFont(&Font, FontAsset.Data) || !TrueTypeRegisterFont(&Fallback, FallbackAsset.Data))
    {
        puts("Terminal: font initialization failed");
        exit(1);
    }

    FLOAT32 Scale = ScaleForPixelHeight(&Font.Info, TERMINAL_FONT_HEIGHT);

    for (UINT32 Character = 32; Character < 127; Character++)
    {
        INT32 Advance;
        INT32 Bearing;
        GetCodepointHMetrics(&Font.Info, Character, &Advance, &Bearing);
        INT32 Width = TTFFloor((Advance * Scale) + 0.5f);
        if (Width <= 0)
        {
            exit(1);
        }
        Glyphs[Character].Advance = (UINT32) Width;
        GetCodepointBitmapBox(&Font.Info, Character, Scale, Scale, &Glyphs[Character].Left, &Glyphs[Character].Top, &Glyphs[Character].Right, &Glyphs[Character].Bottom);
    }

    for (UINTN Index = 0; Index < (UINTN) Info.Width * Info.Height; Index++)
    {
        FrameBuffer[Index] = TERMINAL_BACKGROUND;
    }

    if (TerminalPresent(&Info, X, Baseline) < 0)
    {
        exit(1);
    }

    for (;;)
    {
        ssize_t Count = read(3, Buffer, sizeof(Buffer));

        if (Count <= 0)
        {
            exit(1);
        }

        for (ssize_t Index = 0; Index < Count; Index++)
        {
            UINT32  Character;
            BOOLEAN Retry;
            if (!TerminalDecode(&Decoder, (UINT8) Buffer[Index], &Character, &Retry))
            {
                continue;
            }
            if (Retry)
            {
                Index--;
            }

            if (Character == '\n')
            {
                TerminalNewLine(&Info, &X, &Baseline);
                Cursor = 0;
                LineLength = 0;
            }
            else if (Character == '\r')
            {
                X = TERMINAL_MARGIN;
                Cursor = 0;
            }
            else if (Character == '\b')
            {
                if (Cursor != 0)
                {
                    X = LineCells[--Cursor].X;
                }
            }
            else if (Character >= 32U && Character != 127U)
            {
                TERMINAL_GLYPH  Measured;
                TRUE_TYPE_FONT *DrawFont = TerminalMeasure(&Font, &Fallback, Character, &Measured);
                TERMINAL_GLYPH *Glyph = &Measured;
                UINT32          Extent = Glyph->Right > (INT32) Glyph->Advance ? (UINT32) Glyph->Right : Glyph->Advance;

                if ((X + Extent > Info.Width - TERMINAL_MARGIN && X != TERMINAL_MARGIN) || Cursor == TERMINAL_MAX_WIDTH)
                {
                    TerminalNewLine(&Info, &X, &Baseline);
                    Cursor = 0;
                    LineLength = 0;
                }
                if (Cursor < LineLength)
                {
                    TERMINAL_CELL *Cell = &LineCells[Cursor];
                    TerminalErase(&Info, &Cell->Glyph, Cell->X, Baseline);
                }
                LineCells[Cursor].X = X;
                LineCells[Cursor].Glyph = Measured;
                Cursor++;
                if (Cursor > LineLength)
                {
                    LineLength = Cursor;
                }
                AllocatorUsed = 0;
                X = DrawTrueTypeCodepoint(DrawFont, Character, X, Baseline, TERMINAL_FOREGROUND, TERMINAL_FONT_HEIGHT);
            }
        }

        if (TerminalPresent(&Info, X, Baseline) < 0)
        {
            exit(1);
        }
    }
}
