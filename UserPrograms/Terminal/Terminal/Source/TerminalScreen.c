// UserPrograms/Terminal/Terminal/Source/TerminalScreen.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TerminalScreen.h>
#include <string.h>

STATIC TERMINAL_TEXT_CELL  Cells[TERMINAL_SCREEN_ROWS][TERMINAL_MAX_WIDTH];
STATIC TERMINAL_TEXT_LINE  Lines[TERMINAL_SCREEN_ROWS];
STATIC TERMINAL_TEXT_LINE *Rows[TERMINAL_SCREEN_ROWS];
STATIC TERMINAL_TEXT_LINE *Rendered[TERMINAL_SCREEN_ROWS];
STATIC UINT64              Revisions[TERMINAL_SCREEN_ROWS];
STATIC TRUE_TYPE_FONT     *Primary;
STATIC TRUE_TYPE_FONT     *Secondary;
STATIC UINT32              RowCount;
STATIC BOOLEAN             Healthy = TRUE;

UINT32 TerminalScreenRows(VOID)
{
    return RowCount;
}

TERMINAL_TEXT_LINE *TerminalScreenLine(UINT32 Row)
{
    return Row < RowCount ? Rows[Row] : NULL;
}

BOOLEAN TerminalScreenHealthy(VOID)
{
    return Healthy;
}

VOID TerminalScreenInit(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback)
{
    UINT32 Top = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT - CellAscent;

    RowCount = (Info->Height - TERMINAL_MARGIN - Top) / TERMINAL_LINE_HEIGHT;
    Primary = Font;
    Secondary = Fallback;

    for (UINT32 Index = 0; Index < RowCount; Index++)
    {
        Lines[Index].Cells = Cells[Index];
        Rows[Index] = &Lines[Index];
    }

    TerminalScreenClear(TERMINAL_BACKGROUND);
}

VOID TerminalScreenClear(UINT32 Background)
{
    TerminalScrollReset();

    for (UINT32 Index = 0; Index < RowCount; Index++)
    {
        Rows[Index]->Length = 0;
        Rows[Index]->Background = Background;
        Rows[Index]->Revision++;
        Rendered[Index] = NULL;
    }
}

VOID TerminalScreenNewLine(UINT32 Background)
{
    TERMINAL_TEXT_LINE *Line = Rows[0];

    if (!TerminalHistorySave(Line))
    {
        Healthy = FALSE;

        return;
    }

    memmove(Rows, Rows + 1, (RowCount - 1) * sizeof(Rows[0]));
    Rows[RowCount - 1] = Line;
    Line->Length = 0;
    Line->Background = Background;
    Line->Revision++;
}

STATIC TERMINAL_TEXT_LINE *TerminalScreenAt(UINT32 Baseline)
{
    UINT32 First = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT;
    UINT32 Row = Baseline >= First ? (Baseline - First) / TERMINAL_LINE_HEIGHT : 0;

    return Rows[Row < RowCount ? Row : RowCount - 1];
}

VOID TerminalScreenLoad(UINT32 Baseline, UINT32 *Length)
{
    TERMINAL_TEXT_LINE *Line = TerminalScreenAt(Baseline);

    *Length = Line->Length;

    for (UINT32 Index = 0; Index < Line->Length; Index++)
    {
        LineCells[Index].X = Line->Cells[Index].X;
        LineCells[Index].Glyph.Advance = Line->Cells[Index].Advance;
    }
}

VOID TerminalScreenErase(UINT32 Baseline, UINT32 Background)
{
    TERMINAL_TEXT_LINE *Line = TerminalScreenAt(Baseline);

    Line->Length = 0;
    Line->Background = Background;
    Line->Revision++;
}

VOID TerminalScreenPut(UINT32 Baseline, UINT32 Cursor, UINT32 Codepoint, UINT32 X, UINT32 Advance, UINT32 Foreground, UINT32 Background)
{
    TERMINAL_TEXT_LINE *Line = TerminalScreenAt(Baseline);

    while (Line->Length < Cursor)
    {
        UINT32 Index = Line->Length++;
        UINT32 Start = Index == 0 ? TERMINAL_MARGIN : Line->Cells[Index - 1].X + Line->Cells[Index - 1].Advance;

        Line->Cells[Index] = (TERMINAL_TEXT_CELL){' ', Foreground, Line->Background, Start, Glyphs[' '].Advance};
    }

    Line->Cells[Cursor] = (TERMINAL_TEXT_CELL){Codepoint, Foreground, Background, X, Advance};

    if (Line->Length <= Cursor)
    {
        Line->Length = Cursor + 1;
    }

    Line->Revision++;
}

VOID TerminalScreenRender(DISPLAY_INFO *Info)
{
    TerminalHistorySelect();

    for (UINT32 Row = 0; Row < RowCount; Row++)
    {
        TERMINAL_TEXT_LINE *Line = TerminalHistoryNext(Row);

        if (Rendered[Row] == Line && Revisions[Row] == Line->Revision)
        {
            continue;
        }

        UINT32 Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT + Row * TERMINAL_LINE_HEIGHT;
        INT32  Top = (INT32) Baseline - (INT32) CellAscent;
        UINT32 First = Row == 0 || Top < 0 ? 0 : (UINT32) Top;
        UINT32 CellBottom = (UINT32) (Top + TERMINAL_LINE_HEIGHT);

        if (CellBottom > Info->Height)
        {
            CellBottom = Info->Height;
        }

        UINT32 Bottom = Row + 1 == RowCount ? Info->Height : CellBottom;

        if (Bottom > Info->Height)
        {
            Bottom = Info->Height;
        }

        TerminalDamage(First, Bottom);

        for (UINT32 Y = First; Y < Bottom; Y++)
        {
            for (UINT32 X = 0; X < Info->Width - TERMINAL_SCROLLBAR_WIDTH; X++)
            {
                FrameBuffer[(UINTN) Y * Info->Width + X] = Y >= CellBottom ? TERMINAL_BACKGROUND : Line->Background;
            }
        }

        for (UINT32 Index = 0; Index < Line->Length; Index++)
        {
            TERMINAL_TEXT_CELL *Cell = &Line->Cells[Index];
            UINT32              End = Cell->X + Cell->Advance;

            if (End > Info->Width - TERMINAL_SCROLLBAR_WIDTH)
            {
                End = Info->Width - TERMINAL_SCROLLBAR_WIDTH;
            }

            for (UINT32 Y = Top < 0 ? 0 : (UINT32) Top; Y < CellBottom; Y++)
            {
                for (UINT32 X = Cell->X; X < End; X++)
                {
                    FrameBuffer[(UINTN) Y * Info->Width + X] = Cell->Background;
                }
            }
        }

        for (UINT32 Index = 0; Index < Line->Length; Index++)
        {
            TERMINAL_TEXT_CELL *Cell = &Line->Cells[Index];

            if (Cell->Codepoint == ' ')
            {
                continue;
            }

            TRUE_TYPE_FONT *Font = Cell->Codepoint < 128 || FindGlyphIndex(&Primary->Info, (INT32) Cell->Codepoint) != 0 ? Primary : Secondary;

            TerminalDrawGlyph(Info, Font, Cell->Codepoint, Cell->X, Baseline, Cell->Foreground, Cell->Advance);
        }

        Rendered[Row] = Line;
        Revisions[Row] = Line->Revision;
    }
}

VOID TerminalScreenResize(DISPLAY_INFO *Info, UINT32 *Baseline)
{
    UINT32 Top = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT - CellAscent;
    if (Info->Height <= TERMINAL_MARGIN + Top)
    {
        Healthy = FALSE;
        return;
    }

    UINT32 Count = (Info->Height - TERMINAL_MARGIN - Top) / TERMINAL_LINE_HEIGHT;

    if (Count == 0 || Count > TERMINAL_SCREEN_ROWS)
    {
        Healthy = FALSE;
        return;
    }

    UINT32 CursorRow = (*Baseline - TERMINAL_MARGIN - TERMINAL_FONT_HEIGHT) / TERMINAL_LINE_HEIGHT;

    while (CursorRow >= Count)
    {
        TerminalScreenNewLine(TERMINAL_BACKGROUND);
        *Baseline -= TERMINAL_LINE_HEIGHT;
        CursorRow--;
    }

    if (Count > RowCount)
    {
        for (UINT32 Row = RowCount; Row < Count; Row++)
        {
            Rows[Row]->Length = 0;
            Rows[Row]->Background = TERMINAL_BACKGROUND;
            Rows[Row]->Revision++;
        }
    }

    RowCount = Count;

    for (UINT32 Row = 0; Row < RowCount; Row++)
    {
        Rendered[Row] = NULL;
    }
}
