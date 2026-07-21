// print.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdarg.h>
#include <lineos/bootinfo.h>
#include <render/framebuffer.h>
#include <render/font.h>
#include <render/print.h>

#define KPRINT_TAB_SIZE 4
#define KPRINT_LINE_HEIGHT 24
#define KPRINT_BACKSPACE_WIDTH 12
#define KPRINT_BACKSPACE_HEIGHT 24

STATIC UINT32 KPrintChar(UINT16 Unicode, UINT32 x, UINT32 BaseLine, UINT32 color)
{
    UINT16 advance = DrawGlyph(Unicode, x, BaseLine, color);

    if (advance == 0)
    {
        return KPRINT_BACKSPACE_WIDTH;
    }

    return advance;
}

STATIC UINT32 KPrintString(CONST CHAR16* msg, UINT32 x, UINT32 BaseLine, UINT32 color)
{
    while (*msg != 0)
    {
        x += KPrintChar(*msg, x, BaseLine, color);
        msg++;
    }

    return x;
}

STATIC UINT32 KPrintUnsigned(UINT64 value, UINT32 base, BOOLEAN UpperCase, UINT32 x, UINT32 BaseLine, UINT32 color)
{
    CHAR8 buffer[32];
    STATIC CONST CHAR8 UpperDigits[] = "0123456789ABCDEF";
    STATIC CONST CHAR8 LowerDigits[] = "0123456789abcdef";
    CONST CHAR8* digits = UpperCase ? UpperDigits : LowerDigits;
    UINT32 index = 0;

    if (value == 0)
    {
        return x + KPrintChar('0', x, BaseLine, color);
    }

    while (value != 0 && index < sizeof(buffer))
    {
        buffer[index++] = digits[value % base];
        value /= base;
    }

    while (index > 0)
    {
        index--;
        x += KPrintChar(buffer[index], x, BaseLine, color);
    }

    return x;
}

STATIC UINT32 KPrintSigned(INT64 value, UINT32 x, UINT32 BaseLine, UINT32 color)
{
    if (value < 0)
    {
        x += KPrintChar('-', x, BaseLine, color);
        return KPrintUnsigned((UINT64)(-value), 10, FALSE, x, BaseLine, color);
    }

    return KPrintUnsigned((UINT64)value, 10, FALSE, x, BaseLine, color);
}

VOID KPrint(CONST CHAR16* msg, UINT32 x, UINT32 BaseLine, UINT32 color, ...)
{
    va_list args;
    UINT32 OriginX = x;

    va_start(args, color);

    while (*msg != 0)
    {
        CHAR16 ch = *msg++;

        if (ch == '\n')
        {
            x = OriginX;
            BaseLine += KPRINT_LINE_HEIGHT;
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
                x += KPrintChar(' ', x, BaseLine, color);
            }

            continue;
        }

        if (ch == '\b')
        {
            if (x >= OriginX + KPRINT_BACKSPACE_WIDTH)
            {
                x -= KPRINT_BACKSPACE_WIDTH;
                FillRect(x, BaseLine - KPRINT_BACKSPACE_HEIGHT, KPRINT_BACKSPACE_WIDTH, KPRINT_BACKSPACE_HEIGHT,
                         0x000000);
            }

            continue;
        }

        if (ch != '%')
        {
            x += KPrintChar(ch, x, BaseLine, color);
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
            x += KPrintChar('%', x, BaseLine, color);
            break;

        case 'c':
            x += KPrintChar((UINT16)va_arg(args, UINT32), x, BaseLine, color);
            break;

        case 's':
            {
                CONST CHAR16* String = va_arg(args, CONST CHAR16*);

                if (String == 0)
                {
                    String = (CONST CHAR16*)L"(null)";
                }

                x = KPrintString(String, x, BaseLine, color);
                break;
            }

        case 'd':
        case 'i':
            if (LongLongValue)
            {
                x = KPrintSigned((INT64)va_arg(args, UINT64), x, BaseLine, color);
            }
            else if (LongValue)
            {
                x = KPrintSigned((INT64)va_arg(args, UINT64), x, BaseLine, color);
            }
            else
            {
                x = KPrintSigned((INT32)va_arg(args, INT32), x, BaseLine, color);
            }

            break;

        case 'u':
            if (LongLongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 10, FALSE, x, BaseLine, color);
            }
            else if (LongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 10, FALSE, x, BaseLine, color);
            }
            else
            {
                x = KPrintUnsigned(va_arg(args, UINT32), 10, FALSE, x, BaseLine, color);
            }

            break;

        case 'x':
        case 'X':
            if (LongLongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 16, ch == 'X', x, BaseLine, color);
            }
            else if (LongValue)
            {
                x = KPrintUnsigned(va_arg(args, UINT64), 16, ch == 'X', x, BaseLine, color);
            }
            else
            {
                x = KPrintUnsigned(va_arg(args, UINT32), 16, ch == 'X', x, BaseLine, color);
            }

            break;

        case 'p':
            x += KPrintChar('0', x, BaseLine, color);
            x += KPrintChar('x', x, BaseLine, color);
            x = KPrintUnsigned((UINT64)va_arg(args, VOID*), 16, FALSE, x, BaseLine, color);
            break;

        default:
            x += KPrintChar('%', x, BaseLine, color);
            x += KPrintChar(ch, x, BaseLine, color);
            break;
        }
    }

    va_end(args);
}
