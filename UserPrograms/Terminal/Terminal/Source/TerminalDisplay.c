// UserPrograms/Terminal/Terminal/Source/TerminalDisplay.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <sys/ioctl.h>
#include <string.h>

INT32 FramebufferFD = -1;
UINT32 *MappedBuffer;
UINT32 FrameBuffer[TERMINAL_MAX_WIDTH * TERMINAL_MAX_HEIGHT];

INT64 TerminalPresent(DISPLAY_INFO *Info, UINT32 X, UINT32 Baseline, BOOLEAN CursorVisible)
{
    UINT32 Saved[TERMINAL_CURSOR_WIDTH * TERMINAL_CURSOR_HEIGHT];
    UINT32 Top = Baseline - TERMINAL_CURSOR_HEIGHT + 12U;
    UINTN  Count = 0;

    for (UINT32 Row = Top; CursorVisible && Row < Top + TERMINAL_CURSOR_HEIGHT && Row < Info->Height; Row++)
    {
        for (UINT32 Column = X; Column < X + TERMINAL_CURSOR_WIDTH && Column < Info->Width; Column++)
        {
            UINTN Pixel = (UINTN) Row * Info->Width + Column;
            Saved[Count++] = FrameBuffer[Pixel];
            FrameBuffer[Pixel] = TERMINAL_FOREGROUND;
        }
    }

    memcpy(MappedBuffer, FrameBuffer, (UINT64) Info->Width * Info->Height * sizeof(UINT32));
    INT64 Result = ioctl(FramebufferFD, LOS_FBIO_FLUSH);
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

