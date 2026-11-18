// Libraries/Source/TrueType/TrueTypeEngine.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TrueType/TrueType.h>
#include <TrueType/TrueTypeEngine.h>
#include <TrueType/TrueTypeRuntime.h>

#define TRUE_TYPE_TAB_WIDTH 4

STATIC TRUE_TYPE_SURFACE *DrawSurface;
STATIC TTF_ALLOCATOR     *FontAllocator;

STATIC FLOAT32 GetPixelScale(FONT_INFO *Font, UINT32 PixelHeight)
{
    INT32 Ascent = 0;
    INT32 Descent = 0;
    INT32 LineGap = 0;
    INT32 Units;

    GetFontVMetrics(Font, &Ascent, &Descent, &LineGap);
    Units = Ascent - Descent;

    if (Units == 0)
    {
        return 0.0f;
    }

    return (FLOAT32) PixelHeight / (FLOAT32) Units;
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

STATIC UINT32 BlendColor(UINT32 Background, UINT32 Foreground, UINT8 Alpha)
{
    UINT32 RGB = GetColorRGB(Foreground);
    UINT32 ColorAlpha = GetColorAlpha(Foreground);
    UINT32 EffectiveAlpha = ((UINT32) Alpha * ColorAlpha) / 255;
    UINT32 InverseAlpha = 255 - EffectiveAlpha;
    UINT32 Red = ((((RGB >> 16) & 0xFF) * EffectiveAlpha) + (((Background >> 16) & 0xFF) * InverseAlpha)) / 255;
    UINT32 Green = ((((RGB >> 8) & 0xFF) * EffectiveAlpha) + (((Background >> 8) & 0xFF) * InverseAlpha)) / 255;
    UINT32 Blue = (((RGB & 0xFF) * EffectiveAlpha) + ((Background & 0xFF) * InverseAlpha)) / 255;

    return 0xFF000000 | (Red << 16) | (Green << 8) | Blue;
}

STATIC VOID BlendPixel(TRUE_TYPE_SURFACE *GPU, INT32 X, INT32 Y, UINT32 Color, UINT8 Alpha)
{
    UINT32 *Pixel;
    UINT32  Background;

    if (Alpha == 0 || GetColorAlpha(Color) == 0 || GPU == NULL || GPU->FrameBuffer == NULL || X < 0 || Y < 0 || (UINT32) X >= GPU->FrameBufferWidth || (UINT32) Y >= GPU->FrameBufferHeight)
    {
        return;
    }

    Pixel = &GPU->FrameBuffer[((UINT32) Y * GPU->FrameBufferWidth) + (UINT32) X];

    if (Alpha == 255 && GetColorAlpha(Color) == 255)
    {
        *Pixel = GetColorRGB(Color) | 0xFF000000;

        return;
    }

    Background = *Pixel;
    *Pixel = BlendColor(Background, Color, Alpha);
}

STATIC UINT32 CodepointAdvance(FONT_INFO *Font, UINT32 Codepoint, FLOAT32 Scale)
{
    INT32 AdvanceWidth = 0;
    INT32 LeftSideBearing = 0;

    GetCodepointHMetrics(Font, (INT32) Codepoint, &AdvanceWidth, &LeftSideBearing);

    return (UINT32) TTFFloor((AdvanceWidth * Scale) + 0.5f);
}

STATIC UINT32 DecodeSurrogate(CONST CHAR16 *Text, UINT32 *Index)
{
    UINT32 High = Text[*Index];

    if (High >= 0xD800 && High <= 0xDBFF)
    {
        UINT32 Low = Text[*Index + 1];

        if (Low >= 0xDC00 && Low <= 0xDFFF)
        {
            *Index += 1;

            return 0x10000 + ((High - 0xD800) << 10) + (Low - 0xDC00);
        }
    }

    return High;
}

BOOLEAN TrueTypeInit(TTF_ALLOCATOR *Allocator, TRUE_TYPE_SURFACE *Surface)
{
    if (Allocator == NULL || Allocator->Allocate == NULL || Allocator->Free == NULL || Surface == NULL || Surface->FrameBuffer == NULL || Surface->FrameBufferWidth == 0 || Surface->FrameBufferHeight == 0)
    {
        return FALSE;
    }

    FontAllocator = Allocator;
    DrawSurface = Surface;

    return TRUE;
}

BOOLEAN TrueTypeRegisterFont(TRUE_TYPE_FONT *Font, CONST UINT8 *Data)
{
    if (FontAllocator == NULL || Font == NULL || Data == NULL)
    {
        return FALSE;
    }

    Font->Ready = FALSE;

    if (!InitFont(&Font->Info, Data, 0))
    {
        return FALSE;
    }

    Font->Info.UserData = FontAllocator;
    Font->Ready = TRUE;

    return TRUE;
}

UINT32 DrawTrueTypeCodepoint(TRUE_TYPE_FONT *Font, UINT32 Codepoint, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight)
{
    TRUE_TYPE_SURFACE *GPU = DrawSurface;
    FONT_INFO         *DrawFont;
    FLOAT32            Scale;
    INT32              Width;
    INT32              Height;
    INT32              XOff;
    INT32              YOff;
    UINT8             *Bitmap;

    DrawFont = Font != NULL && Font->Ready ? &Font->Info : NULL;

    if (DrawFont == NULL || GPU == NULL || GPU->FrameBuffer == NULL || Codepoint == 0)
    {
        return X;
    }

    Scale = GetPixelScale(DrawFont, PixelHeight);
    Width = 0;
    Height = 0;
    XOff = 0;
    YOff = 0;
    Bitmap = GetCodepointBitmap(DrawFont, Scale, Scale, (INT32) Codepoint, &Width, &Height, &XOff, &YOff);

    if (Bitmap == NULL)
    {
        if (Width == 0 && Height == 0)
        {
            return X + CodepointAdvance(DrawFont, Codepoint, Scale);
        }

        return X + CodepointAdvance(DrawFont, Codepoint, Scale);
    }

    for (INT32 Row = 0; Row < Height; Row++)
    {
        for (INT32 Column = 0; Column < Width; Column++)
        {
            UINT8 Alpha = Bitmap[(Row * Width) + Column];

            BlendPixel(GPU, (INT32) X + XOff + Column, (INT32) Baseline + YOff + Row, Color, Alpha);
        }
    }

    FreeBitmap(Bitmap, DrawFont->UserData);

    return X + CodepointAdvance(DrawFont, Codepoint, Scale);
}

UINT32 DrawTrueTypeText(TRUE_TYPE_FONT *Font, CONST CHAR16 *Text, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight)
{
    UINT32 OriginX = X;

    if (Text == NULL)
    {
        return X;
    }

    for (UINT32 Index = 0; Text[Index] != 0; Index++)
    {
        UINT32 Codepoint = DecodeSurrogate(Text, &Index);

        if (Codepoint == '\n')
        {
            X = OriginX;
            Baseline += (UINT32) TTFCeil(PixelHeight * 1.35f);
            continue;
        }

        if (Codepoint == '\r')
        {
            X = OriginX;
            continue;
        }

        if (Codepoint == '\t')
        {
            UINT32 SpaceAdvance = DrawTrueTypeCodepoint(Font, ' ', X, Baseline, Color, PixelHeight);

            X += (SpaceAdvance - X) * TRUE_TYPE_TAB_WIDTH;
            continue;
        }

        X = DrawTrueTypeCodepoint(Font, Codepoint, X, Baseline, Color, PixelHeight);
    }

    return X;
}

BOOLEAN MeasureTrueTypeText(TRUE_TYPE_FONT *Font, CONST CHAR16 *Text, UINT32 PixelHeight, INT32 *Left, INT32 *Top, INT32 *Right, INT32 *Bottom)
{
    INT32   CursorX = 0;
    BOOLEAN HasBounds = FALSE;

    if (DrawSurface == NULL || Text == NULL || Left == NULL || Top == NULL || Right == NULL || Bottom == NULL)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Text[Index] != 0; Index++)
    {
        UINT32     Codepoint = DecodeSurrogate(Text, &Index);
        FONT_INFO *FontInfo = Font != NULL && Font->Ready ? &Font->Info : NULL;
        FLOAT32    Scale;
        INT32      BoxX0;
        INT32      BoxY0;
        INT32      BoxX1;
        INT32      BoxY1;

        if (FontInfo == NULL || Codepoint == '\n' || Codepoint == '\r' || Codepoint == '\t')
        {
            continue;
        }

        Scale = GetPixelScale(FontInfo, PixelHeight);
        GetCodepointBitmapBox(FontInfo, (INT32) Codepoint, Scale, Scale, &BoxX0, &BoxY0, &BoxX1, &BoxY1);
        BoxX0 += CursorX;
        BoxX1 += CursorX;

        if (!HasBounds)
        {
            *Left = BoxX0;
            *Top = BoxY0;
            *Right = BoxX1;
            *Bottom = BoxY1;
            HasBounds = TRUE;
        }
        else
        {
            if (BoxX0 < *Left)
            {
                *Left = BoxX0;
            }

            if (BoxY0 < *Top)
            {
                *Top = BoxY0;
            }

            if (BoxX1 > *Right)
            {
                *Right = BoxX1;
            }

            if (BoxY1 > *Bottom)
            {
                *Bottom = BoxY1;
            }
        }

        CursorX += (INT32) CodepointAdvance(FontInfo, Codepoint, Scale);
    }

    return HasBounds;
}
