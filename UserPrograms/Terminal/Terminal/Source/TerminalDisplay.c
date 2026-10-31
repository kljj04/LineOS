// UserPrograms/Terminal/Terminal/Source/TerminalDisplay.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TerminalScreen.h>
#include <sys/ioctl.h>
#include <string.h>

INT32   FramebufferFD = -1;
UINT32 *MappedBuffer;
UINT32  FrameBuffer[TERMINAL_MAX_WIDTH * TERMINAL_MAX_HEIGHT];

UINT32 TerminalDirtyFirst;
UINT32 TerminalDirtyLast;

VOID TerminalDamage(UINT32 First, UINT32 Last)
{
    if (First < TerminalDirtyFirst)
    {
        TerminalDirtyFirst = First;
    }

    if (Last > TerminalDirtyLast)
    {
        TerminalDirtyLast = Last;
    }
}

INT64 TerminalPresent(DISPLAY_INFO *Info, UINT32 X, UINT32 Baseline, BOOLEAN CursorVisible)
{
    STATIC UINT32 PreviousTop;
    STATIC UINT32 PreviousBottom;

    TerminalDirtyFirst = Info->Height;
    TerminalDirtyLast = 0;
    TerminalScreenRender(Info);

    if (PreviousTop < PreviousBottom)
    {
        TerminalDamage(PreviousTop, PreviousBottom < Info->Height ? PreviousBottom : Info->Height);
    }
    CursorVisible = CursorVisible && !TerminalScrollActive();

    UINT32 Saved[TERMINAL_CURSOR_WIDTH * TERMINAL_CURSOR_HEIGHT];
    UINT32 Top = Baseline - TERMINAL_CURSOR_HEIGHT + 8U;
    UINTN  Count = 0;

    PreviousTop = CursorVisible ? Top : 0;
    PreviousBottom = CursorVisible ? Top + TERMINAL_CURSOR_HEIGHT : 0;

    if (CursorVisible)
    {
        TerminalDamage(Top, PreviousBottom < Info->Height ? PreviousBottom : Info->Height);
    }

    for (UINT32 Row = Top; CursorVisible && Row < Top + TERMINAL_CURSOR_HEIGHT && Row < Info->Height; Row++)
    {
        for (UINT32 Column = X; Column < X + TERMINAL_CURSOR_WIDTH && Column < Info->Width; Column++)
        {
            UINTN Pixel = (UINTN) Row * Info->Width + Column;

            Saved[Count++] = FrameBuffer[Pixel];
            FrameBuffer[Pixel] = TERMINAL_FOREGROUND;
        }
    }

    UINT32 *View = TerminalScrollView(Info);
    INT64   Result = 0;

    if (TerminalWindow.Endpoint != 0)
    {
        Result = LOSWindowPresentRows(&TerminalWindow, View, TerminalDirtyFirst < TerminalDirtyLast ? TerminalDirtyFirst : 0, TerminalDirtyLast);

        if (Result < 0)
        {
            TerminalWindowFailed = TRUE;
            Result = 0;
        }
    }
    else
    {
        UINT32 First = Info->Height;
        UINT32 Last = 0;
        UINTN  RowBytes = (UINTN) Info->Width * sizeof(UINT32);

        for (UINT32 Row = TerminalDirtyFirst; Row < TerminalDirtyLast; Row++)
        {
            UINTN Offset = (UINTN) Row * Info->Width;

            if (memcmp(MappedBuffer + Offset, View + Offset, RowBytes) != 0)
            {
                if (First == Info->Height)
                {
                    First = Row;
                }

                Last = Row + 1;
            }
        }

        if (First < Last)
        {
            LOS_FB_RECT Rect = {0, First, Info->Width, Last - First, (UINT64) (View + (UINTN) First * Info->Width)};

            Result = ioctl(FramebufferFD, LOS_FBIO_PRESENT_RECT, &Rect);
        }
    }

    Count = 0;

    for (UINT32 Row = Top; CursorVisible && Row < Top + TERMINAL_CURSOR_HEIGHT && Row < Info->Height; Row++)
    {
        for (UINT32 Column = X; Column < X + TERMINAL_CURSOR_WIDTH && Column < Info->Width; Column++)
        {
            FrameBuffer[(UINTN) Row * Info->Width + Column] = Saved[Count++];
        }
    }

    return Result;
}