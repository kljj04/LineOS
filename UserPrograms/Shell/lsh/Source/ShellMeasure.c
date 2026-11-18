// UserPrograms/Shell/lsh/Source/ShellMeasure.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <Terminal/Text.h>
#include <Asset.h>
#include <TrueType/TrueType.h>
#include <TrueType/TrueTypeRuntime.h>

#define SHELL_METRIC_CACHE_SIZE 512U

typedef struct
{
    UINT32  Codepoint;
    UINT32  Advance;
    UINT32  Extent;
    BOOLEAN Valid;
} SHELL_TEXT_METRIC;

STATIC FONT_INFO         Primary;
STATIC FONT_INFO         Fallback;
STATIC LOS_ASSET         PrimaryAsset;
STATIC LOS_ASSET         FallbackAsset;
STATIC BOOLEAN           Attempted;
STATIC BOOLEAN           Ready;
STATIC SHELL_TEXT_METRIC Metrics[SHELL_METRIC_CACHE_SIZE];

BOOLEAN ShellTextReady(VOID)
{
    if (Attempted)
    {
        return Ready;
    }

    Attempted = TRUE;

    if (LOSAssetAcquire("Fonts/JetBrainsMono", &PrimaryAsset) != 0)
    {
        return FALSE;
    }

    if (LOSAssetAcquire("Fonts/Pretendard", &FallbackAsset) != 0)
    {
        LOSAssetRelease(&PrimaryAsset);
        return FALSE;
    }

    INT32 PrimaryOffset = GetFontOffsetForIndex(PrimaryAsset.Data, 0);
    INT32 FallbackOffset = GetFontOffsetForIndex(FallbackAsset.Data, 0);

    Ready = PrimaryOffset >= 0 && FallbackOffset >= 0 && InitFont(&Primary, PrimaryAsset.Data, PrimaryOffset) && InitFont(&Fallback, FallbackAsset.Data, FallbackOffset);

    if (!Ready)
    {
        LOSAssetRelease(&FallbackAsset);
        LOSAssetRelease(&PrimaryAsset);
    }

    return Ready;
}

VOID ShellTextMeasure(CONST char *Text, UINT32 *Advance, UINT32 *Extent)
{
    UINT8  First = (UINT8) *Text++;
    UINT32 Codepoint = First;
    UINT32 Remaining = 0;

    if (First >= 0xC2 && First <= 0xDF)
    {
        Codepoint = First & 31U;
        Remaining = 1;
    }
    else if (First >= 0xE0 && First <= 0xEF)
    {
        Codepoint = First & 15U;
        Remaining = 2;
    }
    else if (First >= 0xF0 && First <= 0xF4)
    {
        Codepoint = First & 7U;
        Remaining = 3;
    }

    while (Remaining != 0)
    {
        UINT8 Byte = (UINT8) *Text;

        if ((Byte & 0xC0) != 0x80)
        {
            Codepoint = 0xFFFD;
            break;
        }

        Codepoint = (Codepoint << 6) | (Byte & 63U);
        Text++;
        Remaining--;
    }

    SHELL_TEXT_METRIC *Metric = &Metrics[(Codepoint * 2654435761U) % SHELL_METRIC_CACHE_SIZE];

    if (!Metric->Valid || Metric->Codepoint != Codepoint)
    {
        FONT_INFO *Font = Codepoint < 128 || FindGlyphIndex(&Primary, (INT32) Codepoint) != 0 ? &Primary : &Fallback;
        FLOAT32    Scale = ScaleForPixelHeight(Font, TERMINAL_FONT_HEIGHT);
        INT32      Width;
        INT32      Bearing;
        INT32      Left;
        INT32      Top;
        INT32      Right;
        INT32      Bottom;

        GetCodepointHMetrics(Font, (INT32) Codepoint, &Width, &Bearing);
        GetCodepointBitmapBox(Font, (INT32) Codepoint, Scale, Scale, &Left, &Top, &Right, &Bottom);

        INT32 Pixels = TTFFloor(Width * Scale + 0.5f);

        Metric->Codepoint = Codepoint;
        Metric->Advance = Pixels > 0 ? (UINT32) Pixels : 1U;
        Metric->Extent = Right > (INT32) Metric->Advance ? (UINT32) Right : Metric->Advance;
        Metric->Valid = TRUE;
    }

    *Advance = Metric->Advance;
    *Extent = Metric->Extent;
}
