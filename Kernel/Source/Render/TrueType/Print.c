// Kernel/Source/Render/TrueType/Print.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/GPU/VirtIOGPU.h>
#include <Render/TrueType/Print.h>
#include <Render/TrueType/TrueTypeEngine.h>
#include <stdarg.h>

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

STATIC UINT32 KPrintPadding(CHAR16 Padding, UINT32 Count, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    while (Count > 0)
    {
        X = KPrintChar(Padding, X, Baseline, Color, PixelHeight, Font);
        Count--;
    }

    return X;
}

STATIC UINT32 KPrintUnsigned(UINT64 Value, UINT32 Base, BOOLEAN UpperCase, UINT32 Width, CHAR16 Padding, BOOLEAN LeftAlign, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    CHAR16             Buffer[32];
    STATIC CONST CHAR8 UpperDigits[] = "0123456789ABCDEF";
    STATIC CONST CHAR8 LowerDigits[] = "0123456789abcdef";
    CONST CHAR8       *Digits = UpperCase ? UpperDigits : LowerDigits;
    UINT32             Index = 0;
    UINT32             PaddingCount;

    do
    {
        Buffer[Index++] = (CHAR16) Digits[Value % Base];
        Value /= Base;
    } while (Value != 0 && Index < sizeof(Buffer) / sizeof(Buffer[0]));

    PaddingCount = Width > Index ? Width - Index : 0;

    if (!LeftAlign)
    {
        X = KPrintPadding(Padding, PaddingCount, X, Baseline, Color, PixelHeight, Font);
    }

    while (Index > 0)
    {
        Index--;
        X = KPrintChar(Buffer[Index], X, Baseline, Color, PixelHeight, Font);
    }

    if (LeftAlign)
    {
        X = KPrintPadding(' ', PaddingCount, X, Baseline, Color, PixelHeight, Font);
    }

    return X;
}

STATIC UINT32 KPrintSigned(INT64 Value, UINT32 Width, CHAR16 Padding, BOOLEAN LeftAlign, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font)
{
    UINT64 Magnitude;
    UINT64 Temp;
    UINT32 Digits = 1;
    UINT32 PaddingCount;

    Magnitude = Value < 0 ? (UINT64) (-(Value + 1)) + 1 : (UINT64) Value;
    Temp = Magnitude;

    while (Temp >= 10)
    {
        Temp /= 10;
        Digits++;
    }

    if (Value < 0)
    {
        Digits++;
    }

    PaddingCount = Width > Digits ? Width - Digits : 0;

    if (!LeftAlign && Padding == ' ')
    {
        X = KPrintPadding(' ', PaddingCount, X, Baseline, Color, PixelHeight, Font);
    }

    if (Value < 0)
    {
        X = KPrintChar('-', X, Baseline, Color, PixelHeight, Font);
    }

    if (!LeftAlign && Padding == '0')
    {
        X = KPrintPadding('0', PaddingCount, X, Baseline, Color, PixelHeight, Font);
    }

    X = KPrintUnsigned(Magnitude, 10, FALSE, 0, ' ', FALSE, X, Baseline, Color, PixelHeight, Font);

    if (LeftAlign)
    {
        X = KPrintPadding(' ', PaddingCount, X, Baseline, Color, PixelHeight, Font);
    }

    return X;
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
            Baseline += PixelHeight + PixelHeight / 4;
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
        BOOLEAN LeftAlign = FALSE;
        CHAR16  Padding = ' ';
        UINT32  Width = 0;

        if (Ch == '-')
        {
            LeftAlign = TRUE;
            Ch = *Msg++;
        }

        if (Ch == '0')
        {
            Padding = '0';
            Ch = *Msg++;
        }

        while (Ch >= '0' && Ch <= '9')
        {
            UINT32 Digit = (UINT32) (Ch - '0');

            if (Width > (KPRINT_MAX_WIDTH - Digit) / 10)
            {
                Width = KPRINT_MAX_WIDTH;
            }
            else
            {
                Width = Width * 10 + Digit;
            }

            Ch = *Msg++;
        }

        if (LeftAlign)
        {
            Padding = ' ';
        }

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
                if (LongLongValue || LongValue)
                {
                    X = KPrintSigned((INT64) va_arg(Args, INT64), Width, Padding, LeftAlign, X, Baseline, Color, PixelHeight, Font);
                }
                else
                {
                    X = KPrintSigned((INT32) va_arg(Args, INT32), Width, Padding, LeftAlign, X, Baseline, Color, PixelHeight, Font);
                }

                break;

            case 'u':
                if (LongLongValue || LongValue)
                {
                    X = KPrintUnsigned(va_arg(Args, UINT64), 10, FALSE, Width, Padding, LeftAlign, X, Baseline, Color, PixelHeight, Font);
                }
                else
                {
                    X = KPrintUnsigned(va_arg(Args, UINT32), 10, FALSE, Width, Padding, LeftAlign, X, Baseline, Color, PixelHeight, Font);
                }

                break;

            case 'x':
            case 'X':
                if (LongLongValue || LongValue)
                {
                    X = KPrintUnsigned(va_arg(Args, UINT64), 16, Ch == 'X', Width, Padding, LeftAlign, X, Baseline, Color, PixelHeight, Font);
                }
                else
                {
                    X = KPrintUnsigned(va_arg(Args, UINT32), 16, Ch == 'X', Width, Padding, LeftAlign, X, Baseline, Color, PixelHeight, Font);
                }

                break;

            case 'p':
                X = KPrintChar('0', X, Baseline, Color, PixelHeight, Font);
                X = KPrintChar('x', X, Baseline, Color, PixelHeight, Font);
                X = KPrintUnsigned((UINT64) va_arg(Args, VOID *), 16, FALSE, 0, ' ', FALSE, X, Baseline, Color, PixelHeight, Font);
                break;

            default:
                X = KPrintChar('%', X, Baseline, Color, PixelHeight, Font);
                X = KPrintChar(Ch, X, Baseline, Color, PixelHeight, Font);
                break;
        }
    }

    va_end(Args);
}