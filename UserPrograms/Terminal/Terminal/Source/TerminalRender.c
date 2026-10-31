// UserPrograms/Terminal/Terminal/Source/TerminalRender.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TerminalScreen.h>

TERMINAL_GLYPH Glyphs[128];
TERMINAL_CELL  LineCells[TERMINAL_MAX_WIDTH];
UINT32         CellAscent = TERMINAL_FONT_HEIGHT;

VOID TerminalNewLine(DISPLAY_INFO *Info, UINT32 *X, UINT32 *Baseline, UINT32 Background)
{
    (VOID) Info;
    *X = TERMINAL_MARGIN;
    *Baseline += TERMINAL_LINE_HEIGHT;

    if (*Baseline > TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT + (TerminalScreenRows() - 1) * TERMINAL_LINE_HEIGHT)
    {
        TerminalScreenNewLine(Background);
        *Baseline -= TERMINAL_LINE_HEIGHT;
    }
}

TRUE_TYPE_FONT *TerminalMeasure(TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 Codepoint, TERMINAL_GLYPH *Glyph)
{
    if (Codepoint < 128)
    {
        *Glyph = Glyphs[Codepoint];

        return Font;
    }

    typedef struct
    {
        UINT32          Codepoint;
        TRUE_TYPE_FONT *Primary;
        TRUE_TYPE_FONT *Fallback;
        TRUE_TYPE_FONT *Selected;
        TERMINAL_GLYPH  Glyph;
    } TERMINAL_METRIC_CACHE;

    STATIC TERMINAL_METRIC_CACHE Cache[512];
    TERMINAL_METRIC_CACHE       *Entry = &Cache[(Codepoint * 2654435761U) % 512U];

    if (Entry->Codepoint == Codepoint && Entry->Primary == Font && Entry->Fallback == Fallback)
    {
        *Glyph = Entry->Glyph;

        return Entry->Selected;
    }

    TRUE_TYPE_FONT *Selected = FindGlyphIndex(&Font->Info, (INT32) Codepoint) != 0 ? Font : Fallback;
    FLOAT32         Scale = ScaleForPixelHeight(&Selected->Info, TERMINAL_FONT_HEIGHT);
    INT32           Advance;
    INT32           Bearing;

    GetCodepointHMetrics(&Selected->Info, (INT32) Codepoint, &Advance, &Bearing);

    INT32 Width = TTFFloor((Advance * Scale) + 0.5f);

    Glyph->Advance = Width > 0 ? (UINT32) Width : 1U;
    GetCodepointBitmapBox(&Selected->Info, (INT32) Codepoint, Scale, Scale, &Glyph->Left, &Glyph->Top, &Glyph->Right, &Glyph->Bottom);
    Entry->Codepoint = Codepoint;
    Entry->Primary = Font;
    Entry->Fallback = Fallback;
    Entry->Selected = Selected;
    Entry->Glyph = *Glyph;

    return Selected;
}
