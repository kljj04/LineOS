// UserPrograms/Terminal/Terminal/Source/TerminalMouse.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

VOID TerminalMousePoll(DISPLAY_INFO *Info, UINT32 Baseline, UINT32 LineLength, BOOLEAN Enabled)
{
    POINTER_INFO Pointer;

    if (ioctl(3, LOS_TIO_GET_POINTER, &Pointer) == 0)
    {
        TerminalMouseEvent(Info, Baseline, LineLength, Enabled, &Pointer);
    }
}

VOID TerminalMouseEvent(DISPLAY_INFO *Info, UINT32 Baseline, UINT32 LineLength, BOOLEAN Enabled, CONST POINTER_INFO *Pointer)
{
    STATIC UINT32 PreviousButtons;
    STATIC INT32  PreviousWheel;
    INT64         Difference = (INT64) Pointer->Wheel - PreviousWheel;

    if ((Pointer->Wheel > 0 && PreviousWheel < 0) || (Pointer->Wheel < 0 && PreviousWheel > 0))
    {
        Difference = Pointer->Wheel;
    }

    PreviousWheel = Pointer->Wheel;

    if (Difference != 0)
    {
        TerminalScrollWheel(Difference > 0 ? 3 : -3);
    }

    BOOLEAN Clicked = (Pointer->Buttons & 1) != 0 && (PreviousButtons & 1) == 0;

    PreviousButtons = Pointer->Buttons;

    if (TerminalScrollPointer(Info, Pointer->X, Pointer->Y, (Pointer->Buttons & 1) != 0) || !Enabled || !Clicked || Pointer->X >= Info->Width || TerminalScrollActive())
    {
        return;
    }

    INT64 Top = (INT64) Baseline - CellAscent;

    if ((INT64) Pointer->Y < Top || (INT64) Pointer->Y >= Top + TERMINAL_LINE_HEIGHT)
    {
        return;
    }

    UINT32 Column = LineLength;

    for (UINT32 Index = 0; Index < LineLength; Index++)
    {
        UINT32 Middle = LineCells[Index].X + LineCells[Index].Glyph.Advance / 2;

        if (Pointer->X < Middle)
        {
            Column = Index;
            break;
        }
    }

    char   Report[32] = "\033[<0;";
    UINTN  Length = strlen(Report);
    char   Digits[10];
    UINTN  Count = 0;
    UINT32 Value = Column + 1;

    do
    {
        Digits[Count++] = '0' + Value % 10;
        Value /= 10;
    } while (Value != 0);

    while (Count != 0)
    {
        Report[Length++] = Digits[--Count];
    }

    memcpy(Report + Length, ";1M", 3);
    write(3, Report, Length + 3);
}
