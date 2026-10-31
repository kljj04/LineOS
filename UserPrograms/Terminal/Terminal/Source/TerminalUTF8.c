// UserPrograms/Terminal/Terminal/Source/TerminalUTF8.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Terminal.h>

BOOLEAN TerminalDecode(TERMINAL_UTF8 *Decoder, UINT8 Byte, UINT32 *Codepoint, BOOLEAN *Retry)
{
    *Retry = FALSE;

    if (Decoder->Remaining != 0)
    {
        if ((Byte & 0xC0) != 0x80)
        {
            Decoder->Remaining = 0;
            *Retry = TRUE;
            *Codepoint = 0xFFFD;

            return TRUE;
        }

        Decoder->Value = (Decoder->Value << 6) | (Byte & 63);

        if (--Decoder->Remaining != 0)
        {
            return FALSE;
        }

        *Codepoint = Decoder->Value;

        if (*Codepoint < Decoder->Minimum || *Codepoint > 0x10FFFF || (*Codepoint >= 0xD800 && *Codepoint <= 0xDFFF))
        {
            *Codepoint = 0xFFFD;
        }

        return TRUE;
    }

    if (Byte < 0x80)
    {
        *Codepoint = Byte;

        return TRUE;
    }

    if (Byte >= 0xC2 && Byte <= 0xDF)
    {
        Decoder->Value = Byte & 31;
        Decoder->Remaining = 1;
        Decoder->Minimum = 0x80;
    }
    else if (Byte >= 0xE0 && Byte <= 0xEF)
    {
        Decoder->Value = Byte & 15;
        Decoder->Remaining = 2;
        Decoder->Minimum = 0x800;
    }
    else if (Byte >= 0xF0 && Byte <= 0xF4)
    {
        Decoder->Value = Byte & 7;
        Decoder->Remaining = 3;
        Decoder->Minimum = 0x10000;
    }
    else
    {
        *Codepoint = 0xFFFD;

        return TRUE;
    }

    return FALSE;
}
