// kernel/src/render/truetype/print.c
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
    return color;
}

STATIC UINT32 KPrintChar(UINT16 unicode, UINT32 x, UINT32 baseline, UINT32 color, UINT32 PixelHeight, TRUE_TYPE_FONT font)
{
    return DrawTrueTypeCodepoint(font, unicode, x, baseline, KPrintColor(color), PixelHeight);
}

STATIC UINT32 KPrintString(CONST CHAR16 *msg, UINT32 x, UINT32 baseline, UINT32 color, UINT32 PixelHeight, TRUE_TYPE_FONT font)
{
    return DrawTrueTypeText(font, msg, x, baseline, KPrintColor(color), PixelHeight);
}

STATIC UINT32 KPrintUnsigned(UINT64 value, UINT32 base, BOOLEAN UpperCase, UINT32 x, UINT32 baseline, UINT32 color, UINT32 PixelHeight, TRUE_TYPE_FONT font)
{
    CHAR16             buffer[32];
    STATIC CONST CHAR8 UpperDigits[] = "0123456789ABCDEF";
    STATIC CONST CHAR8 LowerDigits[] = "0123456789abcdef";
    CONST CHAR8       *digits = UpperCase ? UpperDigits : LowerDigits;
    UINT32             index = 0;

    if (value == 0)
    {
        return KPrintChar('0', x, baseline, color, PixelHeight, font);
    }

    while (value != 0 && index < sizeof(buffer) / sizeof(buffer[0]))
    {
        buffer[index++] = (CHAR16) digits[value % base];
        value /= base;
    }

    while (index > 0)
    {
        index--;
        x = KPrintChar(buffer[index], x, baseline, color, PixelHeight, font);
    }

    return x;
}

STATIC UINT32 KPrintSigned(INT64 value, UINT32 x, UINT32 baseline, UINT32 color, UINT32 PixelHeight, TRUE_TYPE_FONT font)
{
    if (value < 0)
    {
        x = KPrintChar('-', x, baseline, color, PixelHeight, font);
        return KPrintUnsigned((UINT64) (-value), 10, FALSE, x, baseline, color, PixelHeight, font);
    }

    return KPrintUnsigned((UINT64) value, 10, FALSE, x, baseline, color, PixelHeight, font);
}

VOID KPrint(CONST CHAR16 *msg, UINT32 x, UINT32 baseline, UINT32 color, UINT32 PixelHeight, TRUE_TYPE_FONT font, ...)
{
    va_list args;
    UINT32  OriginX = x;

    if (msg == NULL)
    {
        return;
    }

    va_start(args, font);

    while (*msg != 0)
    {
        CHAR16 ch = *msg++;

        if (ch == '\n')
        {
            x = OriginX;
            baseline += KPRINT_LINE_HEIGHT;
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
                x = KPrintChar(' ', x, baseline, color, PixelHeight, font);
            }

            continue;
        }

        if (ch == '\b')
        {
            if (x >= OriginX + KPRINT_BACKSPACE_WIDTH)
            {
                x -= KPRINT_BACKSPACE_WIDTH;
                FillRect(x, baseline - KPRINT_BACKSPACE_HEIGHT, KPRINT_BACKSPACE_WIDTH, KPRINT_BACKSPACE_HEIGHT, 0x000000);
            }

            continue;
        }

        if (ch != '%')
        {
            x = KPrintChar(ch, x, baseline, color, PixelHeight, font);
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
            x = KPrintChar('%', x, baseline, color, PixelHeight, font);
            break;

        case 'c':
            x = KPrintChar((UINT16) va_arg(args, UINT32), x, baseline, color, PixelHeight, font);
            break;

        case 's':
        {
            CONST CHAR16 *String = va_arg(args, CONST CHAR16 *);

            if (String == NULL)
            {
                String = (CONST CHAR16 *) L"(null)";
            }

            x = KPrintString(String, x, baseline, color, PixelHeight, font);
            break;
        }

        case 'd':
        case 'i':
            if (LongLongValue)
            {
                x = KPrintSigned((INT64) va_arg(args, UINT64), x, baseline, color, PixelHeight, font);
            }
            else if (LongValue)
            {
                x = KPrintSigned((INT64) va_arg(args, UINT64), x, baseline, color, PixelHeight, font);
            }
            else
            {
                x = KPrintSigned((INT32) va_arg(args, INT32), x, baseline, color, PixelHeight, font);
            }

            break;

        case 'u':
            if (LongLongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 10, FALSE, x, baseline, color, PixelHeight, font);
            }
            else if (LongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 10, FALSE, x, baseline, color, PixelHeight, font);
            }
            else
            {
                x = KPrintUnsigned(va_arg(args, UINT32), 10, FALSE, x, baseline, color, PixelHeight, font);
            }

            break;

        case 'x':
        case 'X':
            if (LongLongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 16, ch == 'X', x, baseline, color, PixelHeight, font);
            }
            else if (LongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 16, ch == 'X', x, baseline, color, PixelHeight, font);
            }
            else
            {
                x = KPrintUnsigned(va_arg(args, UINT32), 16, ch == 'X', x, baseline, color, PixelHeight, font);
            }

            break;

        case 'p':
            x = KPrintChar('0', x, baseline, color, PixelHeight, font);
            x = KPrintChar('x', x, baseline, color, PixelHeight, font);
            x = KPrintUnsigned((UINT64) va_arg(args, VOID *), 16, FALSE, x, baseline, color, PixelHeight, font);
            break;

        default:
            x = KPrintChar('%', x, baseline, color, PixelHeight, font);
            x = KPrintChar(ch, x, baseline, color, PixelHeight, font);
            break;
        }
    }

    va_end(args);
}
