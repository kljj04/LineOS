// UserPrograms/Terminal/Terminal/Source/TerminalInput.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

INT32 TerminalRun(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback)
{
    UINT32 X = TERMINAL_MARGIN;
    UINT32 Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT;
    CHAR8 Buffer[8192];
    UINT32 Cursor = 0;
    UINT32 LineLength = 0;
    TERMINAL_UTF8 Decoder = {0};
    BOOLEAN CursorVisible = TRUE;
    BOOLEAN GraphicsActive = FALSE;
    UINT64 LastBlink = 0;
    TERMINAL_ANSI ANSI;
    TerminalANSIInit(&ANSI, TERMINAL_FOREGROUND, TERMINAL_BACKGROUND);

    for (;;)
    {
        INT32 Ready = ioctl(3, LOS_TIO_WAIT_READ, (UINT64) 20);

        if (Ready < 0)
        {
            return 1;
        }
        if (Ready == 2)
        {
            GraphicsActive = TRUE;
            continue;
        }
        struct timespec Time;
        UINT64 Now = LastBlink;
        if (clock_gettime(CLOCK_MONOTONIC, &Time) == 0)
        {
            Now = (UINT64) Time.tv_sec * 1000 + Time.tv_nsec / 1000000;
        }
        if (GraphicsActive)
        {
            GraphicsActive = FALSE;
            CursorVisible = TRUE;
            if (TerminalPresent(Info, X, Baseline, !ANSI.CursorHidden) < 0)
            {
                return 1;
            }
        }
        TerminalMousePoll(Info, Baseline, LineLength, ANSI.MouseReporting);
        if (Ready == 0)
        {
            if (!ANSI.Synchronized && !ANSI.CursorHidden && Now - LastBlink >= 500)
            {
                LastBlink = Now;
                CursorVisible = !CursorVisible;
                if (TerminalPresent(Info, X, Baseline, CursorVisible && !ANSI.CursorHidden) < 0)
                {
                    return 1;
                }
            }
            continue;
        }

        LastBlink = Now;
        CursorVisible = TRUE;
        ssize_t Count = read(3, Buffer, sizeof(Buffer));

        if (Count <= 0)
        {
            return 1;
        }

        for (ssize_t Index = 0; Index < Count; Index++)
        {
            if (TerminalANSIConsume(&ANSI, (UINT8) Buffer[Index]))
            {
                Decoder.Remaining = 0;
                if (ANSI.EraseScreen)
                {
                    UINT32 Background = TerminalANSIBackground(&ANSI);
                    for (UINTN Pixel = 0; Pixel < (UINTN) Info->Width * Info->Height; Pixel++)
                    {
                        FrameBuffer[Pixel] = Background;
                    }
                    Cursor = 0;
                    LineLength = 0;
                    ANSI.EraseScreen = FALSE;
                }
                if (ANSI.CursorHome)
                {
                    X = TERMINAL_MARGIN;
                    Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT;
                    Cursor = 0;
                    LineLength = 0;
                    ANSI.CursorHome = FALSE;
                }
                if (ANSI.CursorPosition)
                {
                    UINT32 Row = ANSI.CursorRow - 1;
                    UINT32 Maximum = (Info->Height - TERMINAL_MARGIN - TERMINAL_FONT_HEIGHT) / TERMINAL_LINE_HEIGHT;
                    if (Row > Maximum)
                    {
                        Row = Maximum;
                    }
                    UINT32 Target = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT + Row * TERMINAL_LINE_HEIGHT;
                    if (Target != Baseline)
                    {
                        Baseline = Target;
                        LineLength = 0;
                        Cursor = 0;
                    }
                    ANSI.CursorPosition = FALSE;
                }
                if (ANSI.CursorColumn != 0)
                {
                    Cursor = ANSI.CursorColumn - 1;
                    if (Cursor > LineLength)
                    {
                        Cursor = LineLength;
                    }
                    if (Cursor < LineLength)
                    {
                        X = LineCells[Cursor].X;
                    }
                    else if (Cursor != 0)
                    {
                        X = LineCells[Cursor - 1].X + LineCells[Cursor - 1].Glyph.Advance;
                    }
                    else
                    {
                        X = TERMINAL_MARGIN;
                    }
                    ANSI.CursorColumn = 0;
                }
                if (ANSI.EraseLine)
                {
                    TERMINAL_GLYPH Row = {0, 0, -(INT32) CellAscent, (INT32) Info->Width, (INT32) TERMINAL_LINE_HEIGHT - (INT32) CellAscent};
                    TerminalErase(Info, &Row, 0, Baseline, TerminalANSIBackground(&ANSI));
                    LineLength = Cursor;
                    ANSI.EraseLine = FALSE;
                }
                continue;
            }
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
                TerminalNewLine(Info, &X, &Baseline, TerminalANSIBackground(&ANSI));
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
                TRUE_TYPE_FONT *DrawFont = TerminalMeasure(Font, Fallback, Character, &Measured);
                TERMINAL_GLYPH *Glyph = &Measured;
                UINT32          Extent = Glyph->Right > (INT32) Glyph->Advance ? (UINT32) Glyph->Right : Glyph->Advance;

                if ((X + Extent > Info->Width - TERMINAL_MARGIN && X != TERMINAL_MARGIN) || Cursor == TERMINAL_MAX_WIDTH)
                {
                    TerminalNewLine(Info, &X, &Baseline, TerminalANSIBackground(&ANSI));
                    Cursor = 0;
                    LineLength = 0;
                }
                if (Cursor < LineLength)
                {
                    TERMINAL_CELL *Cell = &LineCells[Cursor];
                    TerminalErase(Info, &Cell->Glyph, Cell->X, Baseline, TerminalANSIBackground(&ANSI));
                }
                if (Measured.Left > 0)
                {
                    Measured.Left = 0;
                }
                if (Measured.Right < (INT32) Measured.Advance)
                {
                    Measured.Right = (INT32) Measured.Advance;
                }
                if (Measured.Top > -(INT32) CellAscent)
                {
                    Measured.Top = -(INT32) CellAscent;
                }
                if (Measured.Bottom < (INT32) TERMINAL_LINE_HEIGHT - (INT32) CellAscent)
                {
                    Measured.Bottom = (INT32) TERMINAL_LINE_HEIGHT - (INT32) CellAscent;
                }
                TerminalErase(Info, &Measured, X, Baseline, TerminalANSIBackground(&ANSI));
                LineCells[Cursor].X = X;
                LineCells[Cursor].Glyph = Measured;
                Cursor++;
                if (Cursor > LineLength)
                {
                    LineLength = Cursor;
                }
                AllocatorUsed = 0;
                X = DrawTrueTypeCodepoint(DrawFont, Character, X, Baseline, TerminalANSIForeground(&ANSI), TERMINAL_FONT_HEIGHT);
            }
        }

        if (!ANSI.Synchronized && TerminalPresent(Info, X, Baseline, CursorVisible && !ANSI.CursorHidden) < 0)
        {
            return 1;
        }
    }
}

