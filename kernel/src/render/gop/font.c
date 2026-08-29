// kernel/src/render/gop/font.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <lineos/typeinfo.h>
#include <render/gop/font.h>
#include <render/fontinfo.h>
#include <render/gop/framebuffer.h>

INT32 GOPFindGlyph(UINT16 Unicode)
{
    INT32 Low = 0;
    INT32 High = (INT32) GlyphCount - 1;

    while (Low <= High)
    {
        INT32 Mid = Low + ((High - Low) / 2);

        if (GlyphUnicode[Mid] == Unicode)
        {
            return Mid;
        }

        if (GlyphUnicode[Mid] < Unicode)
        {
            Low = Mid + 1;
        }
        else
        {
            High = Mid - 1;
        }
    }

    return -1;
}

UINT16 GOPDrawGlyph(UINT16 Unicode, UINT32 x, UINT32 BaseLine, UINT32 Color)
{
    INT32 Index = GOPFindGlyph(Unicode);

    if (Index < 0)
    {
        return 0;
    }

    CONST LINEOS_GLYPH *Glyph = &GlyphDsc[Index];
    CONST UINT8        *Bitmap = &GlyphBitmap[Glyph->BitmapOffset];

    INT32 StartX = (INT32) x + Glyph->OffsetX;
    INT32 StartY = (INT32) BaseLine - Glyph->OffsetY;

    for (UINT16 GlyphY = 0; GlyphY < Glyph->Height; GlyphY++)
    {
        for (UINT16 GlyphX = 0; GlyphX < Glyph->Width; GlyphX++)
        {
            UINT8 Alpha = Bitmap[((UINT32) GlyphY * Glyph->Width) + GlyphX];

            if (Alpha == 0)
            {
                continue;
            }

            INT32 ScreenX = StartX + GlyphX;
            INT32 ScreenY = StartY + GlyphY;

            if (ScreenX < 0 || ScreenY < 0)
            {
                continue;
            }

            if (Alpha == 255)
            {
                GOPDrawPixel((UINT32) ScreenX, (UINT32) ScreenY, Color);
                continue;
            }

            GOPBlendPixel((UINT32) ScreenX, (UINT32) ScreenY, Color, Alpha);
        }
    }

    return Glyph->Advance;
}
