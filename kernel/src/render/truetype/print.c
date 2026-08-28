// print.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/gpu/virtio_gpu.h>
#include <render/truetype/print.h>
#include <render/truetype/truetype_engine.h>
#include <stdarg.h>

#define KPRINT_TAB_SIZE         4
#define KPRINT_PIXEL_HEIGHT     24
#define KPRINT_LINE_HEIGHT      32
#define KPRINT_BACKSPACE_WIDTH  12
#define KPRINT_BACKSPACE_HEIGHT 24

STATIC UINT32 KPrintColor(UINT32 color)
{
    return color & 0xFFFFFF;
}

STATIC UINT32 KPrintChar(UINT16 unicode, UINT32 x, UINT32 Baseline, UINT32 color, UINT32 PixelHeight)
{
    return DrawTrueTypeCodepoint(unicode, x, Baseline, KPrintColor(color), PixelHeight);
}

STATIC UINT32 KPrintString(CONST CHAR16 *msg, UINT32 x, UINT32 Baseline, UINT32 color, UINT32 PixelHeight)
{
    return DrawTrueTypeText(msg, x, Baseline, KPrintColor(color), PixelHeight);
}

STATIC UINT32 KPrintUnsigned(UINT64 value, UINT32 base, BOOLEAN UpperCase, UINT32 x, UINT32 Baseline, UINT32 color, UINT32 PixelHeight)
{
    CHAR16             buffer[32];
    STATIC CONST CHAR8 UpperDigits[] = "0123456789ABCDEF";
    STATIC CONST CHAR8 LowerDigits[] = "0123456789abcdef";
    CONST CHAR8       *digits = UpperCase ? UpperDigits : LowerDigits;
    UINT32             index = 0;

    if (value == 0)
    {
        return KPrintChar('0', x, Baseline, color, PixelHeight);
    }

    while (value != 0 && index < sizeof(buffer) / sizeof(buffer[0]))
    {
        buffer[index++] = (CHAR16) digits[value % base];
        value /= base;
    }

    while (index > 0)
    {
        index--;
        x = KPrintChar(buffer[index], x, Baseline, color, PixelHeight);
    }

    return x;
}

STATIC UINT32 KPrintSigned(INT64 value, UINT32 x, UINT32 Baseline, UINT32 color, UINT32 PixelHeight)
{
    if (value < 0)
    {
        x = KPrintChar('-', x, Baseline, color, PixelHeight);
        return KPrintUnsigned((UINT64) (-value), 10, FALSE, x, Baseline, color, PixelHeight);
    }

    return KPrintUnsigned((UINT64) value, 10, FALSE, x, Baseline, color, PixelHeight);
}

VOID KPrint(CONST CHAR16 *msg, UINT32 x, UINT32 Baseline, UINT32 color, UINT32 PixelHeight, ...)
{
    va_list args;
    UINT32  OriginX = x;

    if (msg == NULL)
    {
        return;
    }

    va_start(args, PixelHeight);

    while (*msg != 0)
    {
        CHAR16 ch = *msg++;

        if (ch == '\n')
        {
            x = OriginX;
            Baseline += KPRINT_LINE_HEIGHT;
            continue;
        }

        if (ch == '\r')
        {
            x = OriginX;
            continue;
        }

        if (ch == '\t')
        {
            for (UINT32 i = 0; i < KPRINT_TAB_SIZE; i++)
            {
                x = KPrintChar(' ', x, Baseline, color, PixelHeight);
            }

            continue;
        }

        if (ch == '\b')
        {
            if (x >= OriginX + KPRINT_BACKSPACE_WIDTH)
            {
                x -= KPRINT_BACKSPACE_WIDTH;
                FillRect(x, Baseline - KPRINT_BACKSPACE_HEIGHT, KPRINT_BACKSPACE_WIDTH, KPRINT_BACKSPACE_HEIGHT, 0x000000);
            }

            continue;
        }

        if (ch != '%')
        {
            x = KPrintChar(ch, x, Baseline, color, PixelHeight);
            continue;
        }

        ch = *msg++;

        if (ch == 0)
        {
            break;
        }

        BOOLEAN LongValue = FALSE;
        BOOLEAN LongLongValue = FALSE;

        if (ch == 'l')
        {
            LongValue = TRUE;
            ch = *msg++;

            if (ch == 'l')
            {
                LongLongValue = TRUE;
                ch = *msg++;
            }
        }

        switch (ch)
        {
        case '%':
            x = KPrintChar('%', x, Baseline, color, PixelHeight);
            break;

        case 'c':
            x = KPrintChar((UINT16) va_arg(args, UINT32), x, Baseline, color, PixelHeight);
            break;

        case 's':
        {
            CONST CHAR16 *String = va_arg(args, CONST CHAR16 *);

            if (String == NULL)
            {
                String = (CONST CHAR16 *) L"(null)";
            }

            x = KPrintString(String, x, Baseline, color, PixelHeight);
            break;
        }

        case 'd':
        case 'i':
            if (LongLongValue)
            {
                x = KPrintSigned((INT64) va_arg(args, UINT64), x, Baseline, color, PixelHeight);
            }
            else if (LongValue)
            {
                x = KPrintSigned((INT64) va_arg(args, UINT64), x, Baseline, color, PixelHeight);
            }
            else
            {
                x = KPrintSigned((INT32) va_arg(args, INT32), x, Baseline, color, PixelHeight);
            }

            break;

        case 'u':
            if (LongLongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 10, FALSE, x, Baseline, color, PixelHeight);
            }
            else if (LongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 10, FALSE, x, Baseline, color, PixelHeight);
            }
            else
            {
                x = KPrintUnsigned(va_arg(args, UINT32), 10, FALSE, x, Baseline, color, PixelHeight);
            }

            break;

        case 'x':
        case 'X':
            if (LongLongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 16, ch == 'X', x, Baseline, color, PixelHeight);
            }
            else if (LongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 16, ch == 'X', x, Baseline, color, PixelHeight);
            }
            else
            {
                x = KPrintUnsigned(va_arg(args, UINT32), 16, ch == 'X', x, Baseline, color, PixelHeight);
            }

            break;

        case 'p':
            x = KPrintChar('0', x, Baseline, color, PixelHeight);
            x = KPrintChar('x', x, Baseline, color, PixelHeight);
            x = KPrintUnsigned((UINT64) va_arg(args, VOID *), 16, FALSE, x, Baseline, color, PixelHeight);
            break;

        default:
            x = KPrintChar('%', x, Baseline, color, PixelHeight);
            x = KPrintChar(ch, x, Baseline, color, PixelHeight);
            break;
        }
    }

    va_end(args);
}
