// truetype_engine.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/truetype/font_assets.h>
#include <debug/debug.h>
#include <render/gpu/virtio_gpu.h>
#include <render/truetype/truetype.h>
#include <render/truetype/truetype_engine.h>
#include <render/truetype/truetype_runtime.h>

#define TRUE_TYPE_TAB_WIDTH 4

STATIC FONT_INFO  PretendardFont;
STATIC FONT_INFO  JetBrainsMonoFont;
STATIC FONT_INFO *CurrentFont = NULL;
STATIC UINT32     DebugGlyphCount = 0;

STATIC VOID DebugWriteSigned(INT32 Value)
{
    if (Value < 0)
    {
        DebugWrite("-");
        DebugWriteDec((UINT64) -Value);
        return;
    }

    DebugWriteDec((UINT64) Value);
}

STATIC VOID DebugWriteTrueTypeFont(CONST char *Name, BOOLEAN OK, CONST UINT8 *Start, CONST UINT8 *End)
{
    DebugWrite("ttf font ");
    DebugWrite(Name);
    DebugWrite(" ok=");
    DebugWriteDec(OK);
    DebugWrite(" start=");
    DebugWriteHex((UINT64) Start);
    DebugWrite(" size=");
    DebugWriteDec(FontAssetSize(Start, End));
    DebugWrite("\n");
}

STATIC VOID DebugWriteGlyph(UINT32 Codepoint, INT32 GlyphIndex, INT32 Width, INT32 Height, INT32 XOff, INT32 YOff, UINT32 AlphaSum)
{
    if (DebugGlyphCount >= 32)
    {
        return;
    }

    DebugWrite("ttf glyph cp=");
    DebugWriteHex(Codepoint);
    DebugWrite(" glyph=");
    DebugWriteDec((UINT64) GlyphIndex);
    DebugWrite(" w=");
    DebugWriteDec((UINT64) Width);
    DebugWrite(" h=");
    DebugWriteDec((UINT64) Height);
    DebugWrite(" xoff=");
    DebugWriteSigned(XOff);
    DebugWrite(" yoff=");
    DebugWriteSigned(YOff);
    DebugWrite(" alpha=");
    DebugWriteDec(AlphaSum);
    DebugWrite("\n");
    DebugGlyphCount++;
}

STATIC FLOAT32 GetPixelScale(FONT_INFO *Font, UINT32 PixelHeight)
{
    INT32 Ascent = 0;
    INT32 Descent = 0;
    INT32 LineGap = 0;
    INT32 Units;

    GetFontVMetrics(Font, &Ascent, &Descent, &LineGap);
    Units = Ascent - Descent;

    if (DebugGlyphCount == 0)
    {
        DebugWrite("ttf metrics ascent=");
        DebugWriteDec((UINT64) Ascent);
        DebugWrite(" descent=");
        DebugWriteDec((UINT64) Descent);
        DebugWrite(" linegap=");
        DebugWriteDec((UINT64) LineGap);
        DebugWrite(" units=");
        DebugWriteDec((UINT64) Units);
        DebugWrite(" pixel=");
        DebugWriteDec((UINT64) PixelHeight);
        DebugWrite("\n");
    }

    if (Units == 0)
    {
        return 0.0f;
    }

    return (FLOAT32) PixelHeight / (FLOAT32) Units;
}

STATIC UINT32 BlendColor(UINT32 Background, UINT32 Foreground, UINT8 Alpha)
{
    UINT32 InverseAlpha = 255 - Alpha;
    UINT32 Red = ((((Foreground >> 16) & 0xFF) * Alpha) + (((Background >> 16) & 0xFF) * InverseAlpha)) / 255;
    UINT32 Green = ((((Foreground >> 8) & 0xFF) * Alpha) + (((Background >> 8) & 0xFF) * InverseAlpha)) / 255;
    UINT32 Blue = (((Foreground & 0xFF) * Alpha) + ((Background & 0xFF) * InverseAlpha)) / 255;

    return 0xFF000000 | (Red << 16) | (Green << 8) | Blue;
}

STATIC VOID BlendPixel(VIRTIO_GPU_INFO *GPU, INT32 x, INT32 y, UINT32 Color, UINT8 Alpha)
{
    UINT32 *Pixel;
    UINT32  Background;

    if (Alpha == 0 || GPU == NULL || GPU->FrameBuffer == NULL || x < 0 || y < 0 || (UINT32) x >= GPU->FrameBufferWidth || (UINT32) y >= GPU->FrameBufferHeight)
    {
        return;
    }

    Pixel = &GPU->FrameBuffer[((UINT32) y * GPU->FrameBufferWidth) + (UINT32) x];
    if (Alpha == 255)
    {
        *Pixel = Color | 0xFF000000;
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
    return (UINT32) KFloor((AdvanceWidth * Scale) + 0.5f);
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

BOOLEAN TrueTypeInit(VOID)
{
    BOOLEAN PretendardOK = InitFont(&PretendardFont, LineOSPretendardFontStart, 0) != 0;
    BOOLEAN JetBrainsMonoOK = InitFont(&JetBrainsMonoFont, LineOSJetBrainsMonoFontStart, 0) != 0;

    DebugGlyphCount = 0;
    DebugWriteTrueTypeFont("Pretendard", PretendardOK, LineOSPretendardFontStart, LineOSPretendardFontEnd);
    DebugWriteTrueTypeFont("JetBrainsMono", JetBrainsMonoOK, LineOSJetBrainsMonoFontStart, LineOSJetBrainsMonoFontEnd);

    if (PretendardOK)
    {
        CurrentFont = &PretendardFont;
    }
    else if (JetBrainsMonoOK)
    {
        CurrentFont = &JetBrainsMonoFont;
    }
    else
    {
        CurrentFont = NULL;
    }

    return CurrentFont != NULL;
}

BOOLEAN SelectFont(TRUE_TYPE_FONT Font)
{
    switch (Font)
    {
    case PRETENDARD:
        CurrentFont = &PretendardFont;
        return TRUE;

    case JETBRAINS_MONO:
        CurrentFont = &JetBrainsMonoFont;
        return TRUE;
    }

    return FALSE;
}

UINT32 DrawTrueTypeCodepoint(UINT32 Codepoint, UINT32 x, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight)
{
    VIRTIO_GPU_INFO *GPU = VirtIOGPUGetInfo();
    FLOAT32          Scale;
    INT32            GlyphIndex;
    INT32            Width;
    INT32            Height;
    INT32            XOff;
    INT32            YOff;
    INT32            BoxX0;
    INT32            BoxY0;
    INT32            BoxX1;
    INT32            BoxY1;
    UINT8           *Bitmap;
    UINT32           AlphaSum = 0;

    if (CurrentFont == NULL || GPU == NULL || GPU->FrameBuffer == NULL || Codepoint == 0)
    {
        DebugWrite("ttf skip cp=");
        DebugWriteHex(Codepoint);
        DebugWrite(" current=");
        DebugWriteHex((UINT64) CurrentFont);
        DebugWrite(" gpu=");
        DebugWriteHex((UINT64) GPU);
        DebugWrite(" fb=");
        DebugWriteHex(GPU == NULL ? 0 : (UINT64) GPU->FrameBuffer);
        DebugWrite("\n");
        return x;
    }

    Scale = GetPixelScale(CurrentFont, PixelHeight);
    GlyphIndex = FindGlyphIndex(CurrentFont, (INT32) Codepoint);
    GetCodepointBitmapBox(CurrentFont, (INT32) Codepoint, Scale, Scale, &BoxX0, &BoxY0, &BoxX1, &BoxY1);
    Width = 0;
    Height = 0;
    XOff = 0;
    YOff = 0;
    Bitmap = GetCodepointBitmap(CurrentFont, Scale, Scale, (INT32) Codepoint, &Width, &Height, &XOff, &YOff);
    if (Bitmap == NULL)
    {
        if (Width == 0 && Height == 0)
        {
            return x + CodepointAdvance(CurrentFont, Codepoint, Scale);
        }

        DebugWrite("ttf glyph null cp=");
        DebugWriteHex(Codepoint);
        DebugWrite(" glyph=");
        DebugWriteDec((UINT64) GlyphIndex);
        DebugWrite(" scale=");
        DebugWriteHex((UINT64) (Scale * 1000000.0f));
        DebugWrite(" box=");
        DebugWriteSigned(BoxX0);
        DebugWrite(",");
        DebugWriteSigned(BoxY0);
        DebugWrite("-");
        DebugWriteSigned(BoxX1);
        DebugWrite(",");
        DebugWriteSigned(BoxY1);
        DebugWrite(" wh=");
        DebugWriteDec((UINT64) Width);
        DebugWrite("x");
        DebugWriteDec((UINT64) Height);
        DebugWrite("\n");
        return x + CodepointAdvance(CurrentFont, Codepoint, Scale);
    }

    for (INT32 Row = 0; Row < Height; Row++)
    {
        for (INT32 Column = 0; Column < Width; Column++)
        {
            UINT8 Alpha = Bitmap[(Row * Width) + Column];
            AlphaSum += Alpha;
            BlendPixel(GPU, (INT32) x + XOff + Column, (INT32) Baseline + YOff + Row, Color, Alpha);
        }
    }

    DebugWriteGlyph(Codepoint, GlyphIndex, Width, Height, XOff, YOff, AlphaSum);

    FreeBitmap(Bitmap, CurrentFont->userdata);
    return x + CodepointAdvance(CurrentFont, Codepoint, Scale);
}

UINT32 DrawTrueTypeText(CONST CHAR16 *Text, UINT32 x, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight)
{
    UINT32 OriginX = x;

    if (Text == NULL)
    {
        return x;
    }

    for (UINT32 Index = 0; Text[Index] != 0; Index++)
    {
        UINT32 Codepoint = DecodeSurrogate(Text, &Index);

        if (Codepoint == '\n')
        {
            x = OriginX;
            Baseline += (UINT32) KCeil(PixelHeight * 1.35f);
            continue;
        }

        if (Codepoint == '\r')
        {
            x = OriginX;
            continue;
        }

        if (Codepoint == '\t')
        {
            UINT32 SpaceAdvance = DrawTrueTypeCodepoint(' ', x, Baseline, Color, PixelHeight);
            x += (SpaceAdvance - x) * TRUE_TYPE_TAB_WIDTH;
            continue;
        }

        x = DrawTrueTypeCodepoint(Codepoint, x, Baseline, Color, PixelHeight);
    }

    return x;
}
