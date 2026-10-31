// UserPrograms/Terminal/Terminal/Source/TerminalGlyph.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <string.h>

#define TERMINAL_GLYPH_CACHE_COUNT 512U
#define TERMINAL_GLYPH_BITMAP_SIZE 4096U

typedef struct
{
    TRUE_TYPE_FONT *Font;
    UINT32          Codepoint;
    INT32           Width;
    INT32           Height;
    INT32           Left;
    INT32           Top;
    UINT8           Bitmap[TERMINAL_GLYPH_BITMAP_SIZE];
} TERMINAL_CACHED_GLYPH;

STATIC TERMINAL_CACHED_GLYPH Cache[TERMINAL_GLYPH_CACHE_COUNT];

UINT32 TerminalDrawGlyph(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, UINT32 Codepoint, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 Advance)
{
    UINT32                 Slot = (Codepoint * 2654435761U) % TERMINAL_GLYPH_CACHE_COUNT;
    TERMINAL_CACHED_GLYPH *Glyph = &Cache[Slot];

    if (Glyph->Font != Font || Glyph->Codepoint != Codepoint)
    {
        INT32   Width = 0;
        INT32   Height = 0;
        INT32   Left = 0;
        INT32   Top = 0;
        FLOAT32 Scale = ScaleForPixelHeight(&Font->Info, TERMINAL_FONT_HEIGHT);

        AllocatorUsed = 0;

        UINT8 *Bitmap = GetCodepointBitmap(&Font->Info, Scale, Scale, (INT32) Codepoint, &Width, &Height, &Left, &Top);

        if (Width < 0 || Height < 0 || (UINT64) Width * Height > TERMINAL_GLYPH_BITMAP_SIZE || (Bitmap == NULL && Width != 0 && Height != 0))
        {
            AllocatorUsed = 0;

            return DrawTrueTypeCodepoint(Font, Codepoint, X, Baseline, Color, TERMINAL_FONT_HEIGHT);
        }

        Glyph->Font = Font;
        Glyph->Codepoint = Codepoint;
        Glyph->Width = Width;
        Glyph->Height = Height;
        Glyph->Left = Left;
        Glyph->Top = Top;

        if (Bitmap != NULL)
        {
            memcpy(Glyph->Bitmap, Bitmap, (UINTN) Width * Height);
        }
    }

    for (INT32 Row = 0; Row < Glyph->Height; Row++)
    {
        INT64 Y = (INT64) Baseline + Glyph->Top + Row;
        INT64 CellTop = (INT64) Baseline - CellAscent;

        if (Y < 0 || Y >= Info->Height || Y < CellTop || Y >= CellTop + TERMINAL_LINE_HEIGHT)
        {
            continue;
        }

        for (INT32 Column = 0; Column < Glyph->Width; Column++)
        {
            INT64  PixelX = (INT64) X + Glyph->Left + Column;
            UINT32 Alpha = Glyph->Bitmap[(UINTN) Row * Glyph->Width + Column];

            if (Alpha == 0 || PixelX < 0 || PixelX >= Info->Width - TERMINAL_SCROLLBAR_WIDTH)
            {
                continue;
            }

            UINT32 *Pixel = FrameBuffer + (UINTN) Y * Info->Width + PixelX;
            UINT32  Background = *Pixel;
            UINT32  Inverse = 255 - Alpha;
            UINT32  Red = (((Color >> 16) & 255) * Alpha + ((Background >> 16) & 255) * Inverse) / 255;
            UINT32  Green = (((Color >> 8) & 255) * Alpha + ((Background >> 8) & 255) * Inverse) / 255;
            UINT32  Blue = ((Color & 255) * Alpha + (Background & 255) * Inverse) / 255;

            *Pixel = 0xFF000000U | (Red << 16) | (Green << 8) | Blue;
        }
    }

    return X + Advance;
}
