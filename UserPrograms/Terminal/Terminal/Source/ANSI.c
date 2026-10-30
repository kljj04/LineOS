// UserPrograms/Terminal/Terminal/Source/ANSI.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <ANSI.h>

STATIC CONST UINT32 ANSIPalette[16] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0, 0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF};

STATIC UINT32 ANSIIndexedColor(UINT32 Index)
{
    STATIC CONST UINT32 Levels[6] = {0, 95, 135, 175, 215, 255};
    if (Index < 16)
    {
        return ANSIPalette[Index];
    }
    if (Index < 232)
    {
        Index -= 16;
        return (Levels[Index / 36] << 16) | (Levels[(Index / 6) % 6] << 8) | Levels[Index % 6];
    }
    UINT32 Level = 8 + (Index - 232) * 10;
    return Level * 0x010101;
}

STATIC VOID ANSIReset(TERMINAL_ANSI *ANSI)
{
    ANSI->Foreground = ANSI->DefaultForeground;
    ANSI->Background = ANSI->DefaultBackground;
    ANSI->ForegroundIndex = 16;
    ANSI->Bold = FALSE;
    ANSI->Inverse = FALSE;
}

VOID TerminalANSIInit(TERMINAL_ANSI *ANSI, UINT32 Foreground, UINT32 Background)
{
    *ANSI = (TERMINAL_ANSI){0};
    ANSI->DefaultForeground = Foreground;
    ANSI->DefaultBackground = Background;
    ANSIReset(ANSI);
}

STATIC VOID ANSIApply(TERMINAL_ANSI *ANSI)
{
    for (UINT32 Index = 0; Index < ANSI->Count; Index++)
    {
        UINT32 Parameter = ANSI->Parameters[Index];
        if (Parameter == 0)
        {
            ANSIReset(ANSI);
        }
        else if (Parameter == 1 || Parameter == 22)
        {
            ANSI->Bold = Parameter == 1;
        }
        else if (Parameter == 7 || Parameter == 27)
        {
            ANSI->Inverse = Parameter == 7;
        }
        else if (Parameter == 39)
        {
            ANSI->Foreground = ANSI->DefaultForeground;
            ANSI->ForegroundIndex = 16;
        }
        else if (Parameter == 49)
        {
            ANSI->Background = ANSI->DefaultBackground;
        }
        else if ((Parameter >= 30 && Parameter <= 37) || (Parameter >= 90 && Parameter <= 97))
        {
            ANSI->ForegroundIndex = Parameter >= 90 ? Parameter - 90 + 8 : Parameter - 30;
            ANSI->Foreground = ANSIPalette[ANSI->ForegroundIndex];
        }
        else if ((Parameter >= 40 && Parameter <= 47) || (Parameter >= 100 && Parameter <= 107))
        {
            ANSI->Background = ANSIPalette[Parameter >= 100 ? Parameter - 100 + 8 : Parameter - 40];
        }
        else if (Parameter == 38 || Parameter == 48)
        {
            if (Index + 1 >= ANSI->Count)
            {
                break;
            }
            UINT32 Mode = ANSI->Parameters[++Index];
            UINT32 Color;
            if (Mode == 5)
            {
                if (Index + 1 >= ANSI->Count)
                {
                    break;
                }
                UINT32 PaletteIndex = ANSI->Parameters[++Index];
                if (PaletteIndex > 255)
                {
                    continue;
                }
                Color = ANSIIndexedColor(PaletteIndex);
            }
            else if (Mode == 2)
            {
                if (Index + 3 >= ANSI->Count)
                {
                    break;
                }
                UINT32 Red = ANSI->Parameters[++Index];
                UINT32 Green = ANSI->Parameters[++Index];
                UINT32 Blue = ANSI->Parameters[++Index];
                if (Red > 255 || Green > 255 || Blue > 255)
                {
                    continue;
                }
                Color = (Red << 16) | (Green << 8) | Blue;
            }
            else
            {
                break;
            }
            if (Parameter == 38)
            {
                ANSI->Foreground = Color;
                ANSI->ForegroundIndex = 16;
            }
            else
            {
                ANSI->Background = Color;
            }
        }
    }
}

UINT32 TerminalANSIForeground(CONST TERMINAL_ANSI *ANSI)
{
    UINT32 Foreground = ANSI->ForegroundIndex < 8 && ANSI->Bold ? ANSIPalette[ANSI->ForegroundIndex + 8] : ANSI->Foreground;
    return ANSI->Inverse ? ANSI->Background : Foreground;
}

UINT32 TerminalANSIBackground(CONST TERMINAL_ANSI *ANSI)
{
    if (ANSI->Inverse)
    {
        return ANSI->ForegroundIndex < 8 && ANSI->Bold ? ANSIPalette[ANSI->ForegroundIndex + 8] : ANSI->Foreground;
    }
    return ANSI->Background;
}

BOOLEAN TerminalANSIConsume(TERMINAL_ANSI *ANSI, UINT8 Byte)
{
    if (ANSI->State == TERMINAL_ANSI_OSC || ANSI->State == TERMINAL_ANSI_OSC_ESCAPE)
    {
        if (Byte == 7 || (ANSI->State == TERMINAL_ANSI_OSC_ESCAPE && Byte == '\\'))
        {
            ANSI->State = TERMINAL_ANSI_TEXT;
        }
        else
        {
            ANSI->State = Byte == 27 ? TERMINAL_ANSI_OSC_ESCAPE : TERMINAL_ANSI_OSC;
        }
        return TRUE;
    }
    if (Byte == 27)
    {
        ANSI->State = TERMINAL_ANSI_ESCAPE;
        return TRUE;
    }
    if (ANSI->State == TERMINAL_ANSI_TEXT)
    {
        return FALSE;
    }
    if (Byte == 24 || Byte == 26)
    {
        ANSI->State = TERMINAL_ANSI_TEXT;
        return TRUE;
    }
    if (ANSI->State == TERMINAL_ANSI_ESCAPE)
    {
        ANSI->State = Byte == '[' ? TERMINAL_ANSI_CSI : Byte == ']' ? TERMINAL_ANSI_OSC : TERMINAL_ANSI_TEXT;
        ANSI->Count = 1;
        ANSI->Parameters[0] = 0;
        ANSI->Invalid = FALSE;
        return TRUE;
    }
    if (Byte < 32)
    {
        return FALSE;
    }
    if (Byte >= 0x40 && Byte <= 0x7E)
    {
        if (Byte == 'm' && !ANSI->Invalid)
        {
            ANSIApply(ANSI);
        }
        else if (Byte == 'K' && !ANSI->Invalid && ANSI->Count == 1 && ANSI->Parameters[0] == 2)
        {
            ANSI->EraseLine = TRUE;
        }
        else if (Byte == 'J' && !ANSI->Invalid && ANSI->Count == 1 && ANSI->Parameters[0] == 2)
        {
            ANSI->EraseScreen = TRUE;
        }
        else if ((Byte == 'H' || Byte == 'f') && !ANSI->Invalid && ANSI->Count <= 2 && ANSI->Parameters[0] <= 1 && (ANSI->Count == 1 || ANSI->Parameters[1] <= 1))
        {
            ANSI->CursorHome = TRUE;
        }
        ANSI->State = TERMINAL_ANSI_TEXT;
    }
    else if (!ANSI->Invalid)
    {
        if (Byte >= '0' && Byte <= '9')
        {
            UINT32 *Value = &ANSI->Parameters[ANSI->Count - 1];
            if (*Value > (65535U - (Byte - '0')) / 10)
            {
                ANSI->Invalid = TRUE;
            }
            else
            {
                *Value = *Value * 10 + Byte - '0';
            }
        }
        else if (Byte == ';' && ANSI->Count < TERMINAL_ANSI_PARAMETERS)
        {
            ANSI->Parameters[ANSI->Count++] = 0;
        }
        else
        {
            ANSI->Invalid = TRUE;
        }
    }
    return TRUE;
}
