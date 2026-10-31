// UserPrograms/Terminal/Terminal/Source/TerminalInput.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TerminalScreen.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <sys/mman.h>

INT32 TerminalRun(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback)
{
    UINT32        X = TERMINAL_MARGIN;
    UINT32        Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT;
    CHAR8        *Buffer = NULL;
    UINTN         BufferCapacity = 0;
    UINT32        Cursor = 0;
    UINT32        LineLength = 0;
    TERMINAL_UTF8 Decoder = {0};
    BOOLEAN       CursorVisible = TRUE;
    BOOLEAN       GraphicsActive = FALSE;
    UINT64        LastBlink = 0;
    UINT64        LastPresent = 0;
    BOOLEAN       Dirty = FALSE;
    TERMINAL_ANSI ANSI;

    TerminalANSIInit(&ANSI, TERMINAL_FOREGROUND, TERMINAL_BACKGROUND);

    for (;;)
    {
        INT32 Ready = ioctl(3, LOS_TIO_WAIT_READ, (UINT64) 8);

        if (Ready < 0)
        {
            return 1;
        }

        UINT64 PreviousEndpoint = TerminalWindow.Endpoint;

        BOOLEAN Windowed = TerminalWindowUpdate(Info, Font, Fallback, &X, &Baseline, LineLength, ANSI.MouseReporting);

        if (PreviousEndpoint != TerminalWindow.Endpoint)
        {
            TerminalScreenLoad(Baseline, &LineLength);

            if (Cursor > LineLength)
            {
                Cursor = LineLength;
            }

            Decoder.Remaining = 0;
            Dirty = TRUE;
            GraphicsActive = !Windowed;
        }

        if (Ready == 2 && !Windowed)
        {
            GraphicsActive = TRUE;

            struct timespec Delay = {0, 1000000};

            nanosleep(&Delay, NULL);
            continue;
        }

        if (Windowed)
        {
            INT32 Pending = 0;

            ioctl(3, FIONREAD, &Pending);
            Ready = Pending > 0 ? 1 : 0;
            GraphicsActive = FALSE;

            if (Ready == 0)
            {
                struct timespec Delay = {0, 1000000};

                nanosleep(&Delay, NULL);
            }
        }

        struct timespec Time;
        UINT64          Now = LastBlink;

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

        if (!Windowed)
        {
            TerminalMousePoll(Info, Baseline, LineLength, ANSI.MouseReporting);
        }
        Dirty = TerminalScrollTick(Now) || Dirty;

        if (Ready == 0)
        {
            if (!ANSI.Synchronized && Dirty && Now - LastPresent >= 8)
            {
                if (TerminalPresent(Info, X, Baseline, CursorVisible && !ANSI.CursorHidden) < 0)
                {
                    return 1;
                }

                Dirty = FALSE;
                LastPresent = Now;
            }

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

        INT32 Pending = 0;

        if (ioctl(3, FIONREAD, &Pending) < 0)
        {
            return 1;
        }

        if (Pending <= 0)
        {
            continue;
        }

        UINTN Wanted = (UINTN) Pending;

        if (Wanted > 16384U)
        {
            Wanted = 16384U;
        }

        if (Wanted > BufferCapacity)
        {
            UINTN  Capacity = (Wanted + 4095U) & ~(UINTN) 4095U;
            CHAR8 *Replacement = mmap(NULL, Capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

            if (Replacement == MAP_FAILED)
            {
                return 1;
            }

            if (Buffer != NULL)
            {
                munmap(Buffer, BufferCapacity);
            }

            Buffer = Replacement;
            BufferCapacity = Capacity;
        }

        UINTN Length = 0;

        while (Length < Wanted)
        {
            ssize_t Received = read(3, Buffer + Length, Wanted - Length);

            if (Received <= 0)
            {
                return 1;
            }

            Length += (UINTN) Received;
        }

        ssize_t Count = (ssize_t) Length;

        for (ssize_t Index = 0; Index < Count; Index++)
        {
            if (TerminalANSIConsume(&ANSI, (UINT8) Buffer[Index]))
            {
                Decoder.Remaining = 0;

                if (ANSI.EraseScreen)
                {
                    TerminalScreenClear(TerminalANSIBackground(&ANSI));
                    Cursor = 0;
                    LineLength = 0;
                    ANSI.EraseScreen = FALSE;
                }

                if (ANSI.CursorHome)
                {
                    X = TERMINAL_MARGIN;
                    Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT;
                    Cursor = 0;
                    TerminalScreenLoad(Baseline, &LineLength);
                    ANSI.CursorHome = FALSE;
                }

                if (ANSI.CursorPosition)
                {
                    UINT32 Row = ANSI.CursorRow - 1;
                    UINT32 Maximum = TerminalScreenRows() - 1;

                    if (Row > Maximum)
                    {
                        Row = Maximum;
                    }

                    UINT32 Target = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT + Row * TERMINAL_LINE_HEIGHT;

                    if (Target != Baseline)
                    {
                        Baseline = Target;
                        TerminalScreenLoad(Baseline, &LineLength);
                        Cursor = 0;
                    }

                    ANSI.CursorPosition = FALSE;
                }

                if (ANSI.CursorDelta != 0)
                {
                    INT64 Row = ((INT64) Baseline - TERMINAL_MARGIN - TERMINAL_FONT_HEIGHT) / TERMINAL_LINE_HEIGHT + ANSI.CursorDelta;

                    if (Row < 0)
                    {
                        Row = 0;
                    }

                    if (Row >= TerminalScreenRows())
                    {
                        Row = TerminalScreenRows() - 1;
                    }

                    Baseline = TERMINAL_MARGIN + TERMINAL_FONT_HEIGHT + (UINT32) Row * TERMINAL_LINE_HEIGHT;
                    TerminalScreenLoad(Baseline, &LineLength);

                    if (Cursor > LineLength)
                    {
                        Cursor = LineLength;
                    }

                    X = Cursor < LineLength ? LineCells[Cursor].X : Cursor != 0 ? LineCells[Cursor - 1].X + LineCells[Cursor - 1].Glyph.Advance : TERMINAL_MARGIN;
                    ANSI.CursorDelta = 0;
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
                    TerminalScreenErase(Baseline, TerminalANSIBackground(&ANSI));
                    LineLength = 0;
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
                TerminalScreenLoad(Baseline, &LineLength);
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
                TERMINAL_GLYPH Measured;

                TerminalMeasure(Font, Fallback, Character, &Measured);

                TERMINAL_GLYPH *Glyph = &Measured;
                UINT32          Extent = Glyph->Right > (INT32) Glyph->Advance ? (UINT32) Glyph->Right : Glyph->Advance;

                if ((X + Extent > Info->Width - TERMINAL_MARGIN - TERMINAL_SCROLLBAR_WIDTH && X != TERMINAL_MARGIN) || Cursor == TERMINAL_MAX_WIDTH)
                {
                    TerminalNewLine(Info, &X, &Baseline, TerminalANSIBackground(&ANSI));
                    Cursor = 0;
                    TerminalScreenLoad(Baseline, &LineLength);
                }

                TerminalScreenPut(Baseline, Cursor, Character, X, Measured.Advance, TerminalANSIForeground(&ANSI), TerminalANSIBackground(&ANSI));
                LineCells[Cursor].X = X;
                LineCells[Cursor].Glyph = Measured;
                Cursor++;

                if (Cursor > LineLength)
                {
                    LineLength = Cursor;
                }

                X += Measured.Advance;
            }
        }

        if (!TerminalScreenHealthy())
        {
            return 1;
        }

        Dirty = TRUE;

        if (clock_gettime(CLOCK_MONOTONIC, &Time) == 0)
        {
            Now = (UINT64) Time.tv_sec * 1000 + Time.tv_nsec / 1000000;
        }

        if (!ANSI.Synchronized && Now - LastPresent >= 8)
        {
            if (TerminalPresent(Info, X, Baseline, CursorVisible && !ANSI.CursorHidden) < 0)
            {
                return 1;
            }

            LastPresent = Now;
            Dirty = FALSE;
        }
    }
}
