// UserPrograms/Terminal/Terminal/Source/TerminalRender.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <string.h>

TERMINAL_GLYPH Glyphs[128];
TERMINAL_CELL LineCells[TERMINAL_MAX_WIDTH];
UINT32 CellAscent = TERMINAL_FONT_HEIGHT;

VOID TerminalNewLine(DISPLAY_INFO *Info, UINT32 *X, UINT32 *Baseline, UINT32 Background)
{
    *X = TERMINAL_MARGIN;
    *Baseline += TERMINAL_LINE_HEIGHT;

    if (*Baseline + TERMINAL_LINE_HEIGHT > Info->Height)
    {
        UINTN Rows = Info->Height - TERMINAL_LINE_HEIGHT;

        memmove(FrameBuffer, FrameBuffer + TERMINAL_LINE_HEIGHT * Info->Width, Rows * Info->Width * sizeof(UINT32));

        for (UINTN Index = Rows * Info->Width; Index < (UINTN) Info->Height * Info->Width; Index++)
        {
            FrameBuffer[Index] = Background;
        }

        *Baseline -= TERMINAL_LINE_HEIGHT;
    }
}

VOID TerminalErase(DISPLAY_INFO *Info, TERMINAL_GLYPH *Glyph, UINT32 X, UINT32 Baseline, UINT32 Background)
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
    INT64 CellTop = (INT64) Baseline - CellAscent;
    INT64 CellBottom = CellTop + TERMINAL_LINE_HEIGHT;
    if (Top < CellTop)
    {
        Top = CellTop;
    }
    if (Bottom > CellBottom)
    {
        Bottom = CellBottom;
    }
    if (Top < 0)
    {
        Top = 0;
    }
    if (Bottom > Info->Height)
    {
        Bottom = Info->Height;
    }
    for (INT64 Row = Top; Row < Bottom; Row++)
    {
        for (INT64 Column = Left; Column < Right; Column++)
        {
            FrameBuffer[(UINTN) Row * Info->Width + Column] = Background;
        }
    }
}

TRUE_TYPE_FONT *TerminalMeasure(TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 Codepoint, TERMINAL_GLYPH *Glyph)
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

