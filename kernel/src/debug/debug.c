// debug.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <arch/x86_64/cpu.h>
#include <debug/debug.h>

#define DEBUGCON_PORT 0xE9

STATIC VOID DebugPutChar(CHAR8 Character)
{
    OUTB(DEBUGCON_PORT, (UINT8) Character);
}

VOID DebugWrite(CONST char *String)
{
    while (String != NULL && *String != 0)
    {
        DebugPutChar(*String);
        String++;
    }
}

VOID DebugWriteLine(CONST char *String)
{
    DebugWrite(String);
    DebugPutChar('\n');
}

VOID DebugWriteWide(CONST CHAR16 *String)
{
    while (String != NULL && *String != 0)
    {
        DebugPutChar((CHAR8) (*String & 0xFF));
        String++;
    }
}

VOID DebugWriteHex(UINT64 Value)
{
    STATIC CONST CHAR8 Digits[] = "0123456789abcdef";
    BOOLEAN            Started = FALSE;

    DebugWrite("0x");
    for (INT32 Shift = 60; Shift >= 0; Shift -= 4)
    {
        UINT8 Digit = (UINT8) ((Value >> Shift) & 0xF);

        if (Digit != 0 || Started || Shift == 0)
        {
            DebugPutChar(Digits[Digit]);
            Started = TRUE;
        }
    }
}

VOID DebugWriteDec(UINT64 Value)
{
    CHAR8  Buffer[32];
    UINT32 Index = 0;

    if (Value == 0)
    {
        DebugPutChar('0');
        return;
    }

    while (Value != 0 && Index < sizeof(Buffer))
    {
        Buffer[Index++] = (CHAR8) ('0' + (Value % 10));
        Value /= 10;
    }

    while (Index != 0)
    {
        DebugPutChar(Buffer[--Index]);
    }
}
