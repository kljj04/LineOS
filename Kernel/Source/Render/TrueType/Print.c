// Kernel/Source/Render/TrueType/Print.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/GPU/VirtIOGPU.h>
#include <Render/TrueType/Print.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <stdarg.h>

#define KPRINT_TAB_SIZE         4
#define KPRINT_PIXEL_HEIGHT     24
#define KPRINT_LINE_HEIGHT      32
#define KPRINT_BACKSPACE_WIDTH  12
#define KPRINT_BACKSPACE_HEIGHT 24

STATIC UINT32 KPrintColor(UINT32 Color)
{
    return Color;
}

STATIC UINT32 KPrintChar(UINT16 Unicode, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    return DrawTrueTypeCodepoint(Font, Unicode, X, Baseline, KPrintColor(Color), PixelHeight);
}

STATIC UINT32 KPrintString(CONST CHAR16 *Msg, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    return DrawTrueTypeText(Font, Msg, X, Baseline, KPrintColor(Color), PixelHeight);
}

STATIC UINT32 KPrintUnsigned(UINT64 Value, UINT32 Base, BOOLEAN UpperCase, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    CHAR16             Buffer[32];
    STATIC CONST CHAR8 UpperDigits[] = "0123456789ABCDEF";
    STATIC CONST CHAR8 LowerDigits[] = "0123456789abcdef";
    CONST CHAR8       *Digits = UpperCase ? UpperDigits : LowerDigits;
    UINT32             Index = 0;

    if (Value == 0)
    {
        return KPrintChar('0', X, Baseline, Color, PixelHeight, Font);
    }

    while (Value != 0 && Index < sizeof(Buffer) / sizeof(Buffer[0]))
    {
        Buffer[Index++] = (CHAR16) Digits[Value % Base];
        Value /= Base;
    }

    while (Index > 0)
    {
        Index--;
        X = KPrintChar(Buffer[Index], X, Baseline, Color, PixelHeight, Font);
    }

    return X;
}

STATIC UINT32 KPrintSigned(INT64 Value, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    if (Value < 0)
    {
        X = KPrintChar('-', X, Baseline, Color, PixelHeight, Font);
        return KPrintUnsigned((UINT64) (-Value), 10, FALSE, X, Baseline, Color, PixelHeight, Font);
    }

    return KPrintUnsigned((UINT64) Value, 10, FALSE, X, Baseline, Color, PixelHeight, Font);
}

VOID KPrint(CONST CHAR16 *Msg, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font, ...)
{
    va_list Args;
    UINT32  OriginX = X;

    if (Msg == NULL)
    {
        return;
    }

    va_start(Args, Font);

    while (*Msg != 0)
    {
        CHAR16 Ch = *Msg++;

        if (Ch == '\n')
        {
            X = OriginX;
            Baseline += KPRINT_LINE_HEIGHT;
            continue;
        }

        if (Ch == '\r')
        {
            X = OriginX;
            continue;
        }

        if (Ch == '\t')
        {
            for (UINT32 I = 0; I < KPRINT_TAB_SIZE; I++)
            {
                X = KPrintChar(' ', X, Baseline, Color, PixelHeight, Font);
            }

            continue;
        }

        if (Ch == '\b')
        {
            if (X >= OriginX + KPRINT_BACKSPACE_WIDTH)
            {
                X -= KPRINT_BACKSPACE_WIDTH;
                FillRect(X, Baseline - KPRINT_BACKSPACE_HEIGHT, KPRINT_BACKSPACE_WIDTH, KPRINT_BACKSPACE_HEIGHT, 0x000000);
            }

            continue;
        }

        if (Ch != '%')
        {
            X = KPrintChar(Ch, X, Baseline, Color, PixelHeight, Font);
            continue;
        }

        Ch = *Msg++;

        if (Ch == 0)
        {
            break;
        }

        BOOLEAN LongValue = FALSE;
        BOOLEAN LongLongValue = FALSE;

        if (Ch == 'l')
        {
            LongValue = TRUE;
            Ch = *Msg++;

            if (Ch == 'l')
            {
                LongLongValue = TRUE;
                Ch = *Msg++;
            }
        }

        switch (Ch)
        {
        case '%':
            X = KPrintChar('%', X, Baseline, Color, PixelHeight, Font);
            break;

        case 'c':
            X = KPrintChar((UINT16) va_arg(Args, UINT32), X, Baseline, Color, PixelHeight, Font);
            break;

        case 's':
        {
            CONST CHAR16 *String = va_arg(Args, CONST CHAR16 *);

            if (String == NULL)
            {
                String = (CONST CHAR16 *) L"(null)";
            }

            X = KPrintString(String, X, Baseline, Color, PixelHeight, Font);
            break;
        }

        case 'd':
        case 'i':
            if (LongLongValue)
            {
                X = KPrintSigned((INT64) va_arg(Args, UINT64), X, Baseline, Color, PixelHeight, Font);
            }
            else if (LongValue)
            {
                X = KPrintSigned((INT64) va_arg(Args, UINT64), X, Baseline, Color, PixelHeight, Font);
            }
            else
            {
                X = KPrintSigned((INT32) va_arg(Args, INT32), X, Baseline, Color, PixelHeight, Font);
            }

            break;

        case 'u':
            if (LongLongValue)
            {
                X = KPrintUnsigned(va_arg(Args, UINT64), 10, FALSE, X, Baseline, Color, PixelHeight, Font);
            }
            else if (LongValue)
            {
                X = KPrintUnsigned(va_arg(Args, UINT64), 10, FALSE, X, Baseline, Color, PixelHeight, Font);
            }
            else
            {
                X = KPrintUnsigned(va_arg(Args, UINT32), 10, FALSE, X, Baseline, Color, PixelHeight, Font);
            }

            break;

        case 'x':
        case 'X':
            if (LongLongValue)
            {
                X = KPrintUnsigned(va_arg(Args, UINT64), 16, Ch == 'X', X, Baseline, Color, PixelHeight, Font);
            }
            else if (LongValue)
            {
                X = KPrintUnsigned(va_arg(Args, UINT64), 16, Ch == 'X', X, Baseline, Color, PixelHeight, Font);
            }
            else
            {
                X = KPrintUnsigned(va_arg(Args, UINT32), 16, Ch == 'X', X, Baseline, Color, PixelHeight, Font);
            }

            break;

        case 'p':
            X = KPrintChar('0', X, Baseline, Color, PixelHeight, Font);
            X = KPrintChar('x', X, Baseline, Color, PixelHeight, Font);
            X = KPrintUnsigned((UINT64) va_arg(Args, VOID *), 16, FALSE, X, Baseline, Color, PixelHeight, Font);
            break;

        default:
            X = KPrintChar('%', X, Baseline, Color, PixelHeight, Font);
            X = KPrintChar(Ch, X, Baseline, Color, PixelHeight, Font);
            break;
        }
    }

    va_end(Args);
}
