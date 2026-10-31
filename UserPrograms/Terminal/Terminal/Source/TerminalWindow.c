// UserPrograms/Terminal/Terminal/Source/TerminalWindow.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TerminalScreen.h>
#include <Window.h>
#include <sys/ioctl.h>
#include <unistd.h>

LOS_WINDOW          TerminalWindow;
BOOLEAN             TerminalWindowFailed;
STATIC UINT64       FailedServer;
STATIC DISPLAY_INFO Physical;
STATIC BOOLEAN      SavedPhysical;

BOOLEAN TerminalWindowUpdate(DISPLAY_INFO *Info, TRUE_TYPE_FONT *Font, TRUE_TYPE_FONT *Fallback, UINT32 *X, UINT32 *Baseline, UINT32 LineLength, BOOLEAN Mouse)
{
    (VOID) Font;
    (VOID) Fallback;
    (VOID) X;

    UINT64 Server = 0;

    if (ioctl(3, LOS_TIO_GET_COMPOSITOR, &Server) < 0)
    {
        Server = 0;
    }

    if (FailedServer != Server)
    {
        FailedServer = 0;
    }

    if (TerminalWindowFailed)
    {
        FailedServer = TerminalWindow.Server;
        TerminalWindowFailed = FALSE;
    }

    if (TerminalWindow.Endpoint != 0 && (FailedServer == TerminalWindow.Server || Server != TerminalWindow.Server || LOSIPCAlive(TerminalWindow.Endpoint, TerminalWindow.Server) < 0))
    {
        LOSWindowClose(&TerminalWindow);

        if (SavedPhysical)
        {
            *Info = Physical;
            TerminalSurface.FrameBufferWidth = Info->Width;
            TerminalSurface.FrameBufferHeight = Info->Height;
            TerminalScreenResize(Info, Baseline);
        }
    }

    if (Server != 0 && Server != FailedServer && TerminalWindow.Endpoint == 0)
    {
        if (!SavedPhysical)
        {
            Physical = *Info;
            SavedPhysical = TRUE;
        }

        UINT32 Width = Physical.Width;
        UINT32 Height = Physical.Height - LOS_WINDOW_DOCK_HEIGHT;

        if (LOSWindowOpen(&TerminalWindow, Server, "Terminal", Width, Height, LOS_WINDOW_BACKGROUND) == 0)
        {
            Info->Width = Width;
            Info->Height = Height;
            Info->Pitch = Width * sizeof(UINT32);
            TerminalSurface.FrameBufferWidth = Width;
            TerminalSurface.FrameBufferHeight = Height;
            TerminalScreenResize(Info, Baseline);
        }
    }

    STATIC UINT32 ReportedWidth;
    STATIC UINT32 ReportedHeight;

    if (ReportedWidth != Info->Width || ReportedHeight != Info->Height)
    {
        struct winsize Size;

        Size.ws_row = TerminalScreenRows();
        Size.ws_col = (Info->Width - TERMINAL_MARGIN * 2 - TERMINAL_SCROLLBAR_WIDTH) / Glyphs[' '].Advance;
        Size.ws_xpixel = Info->Width;
        Size.ws_ypixel = Info->Height;
        ioctl(3, TIOCSWINSZ, &Size);

        ReportedWidth = Info->Width;
        ReportedHeight = Info->Height;
    }

    if (TerminalWindow.Endpoint == 0)
    {
        return FALSE;
    }

    for (UINT32 Index = 0; Index < 64; Index++)
    {
        LOS_WINDOW_MESSAGE Event;
        INT32              Result = LOSWindowPoll(&TerminalWindow, &Event);

        if (Result <= 0)
        {
            break;
        }

        if (Event.Type == LOS_WINDOW_KEY)
        {
            UINT8 Key = Event.Key;

            write(3, &Key, 1);
        }
        else if (Event.Type == LOS_WINDOW_POINTER)
        {
            POINTER_INFO Pointer = {Event.X, Event.Y, Event.Buttons, Event.Wheel};

            TerminalMouseEvent(Info, *Baseline, LineLength, Mouse, &Pointer);
        }
    }

    return TRUE;
}
