// LOSLibC/Source/stdio.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

static void FlushOutput(STDIO_OUTPUT *Output)
{
    size_t  Offset;
    ssize_t Written;

    if (Output->Failed || Output->Length == 0)
    {
        return;
    }

    Offset = 0;

    while (Offset < Output->Length)
    {
        Written = write(1, Output->Buffer + Offset, Output->Length - Offset);

        if (Written <= 0 || (size_t) Written > Output->Length - Offset)
        {
            Output->Failed = 1;
            return;
        }

        Offset += (size_t) Written;
    }

    Output->Length = 0;
}

static void OutputCharacter(STDIO_OUTPUT *Output, char Character)
{
    if (Output->Failed)
    {
        return;
    }

    if (Output->Length == STDIO_BUFFER_SIZE)
    {
        FlushOutput(Output);
    }

    if (Output->Failed)
    {
        return;
    }

    Output->Buffer[Output->Length++] = Character;
    Output->Total++;
}

static void OutputString(STDIO_OUTPUT *Output, const char *String)
{
    if (String == NULL)
    {
        String = "(null)";
    }

    while (*String != '\0')
    {
        OutputCharacter(Output, *String++);
    }
}

static void OutputUnsigned(STDIO_OUTPUT *Output, uint64_t Value, unsigned int Base, int Uppercase)
{
    char        Digits[32];
    const char *Alphabet;
    size_t      Count;

    Alphabet = Uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    Count = 0;

    do
    {
        Digits[Count++] = Alphabet[Value % Base];
        Value /= Base;
    } while (Value != 0);

    while (Count > 0)
    {
        OutputCharacter(Output, Digits[--Count]);
    }
}

static void OutputSigned(STDIO_OUTPUT *Output, int64_t Value)
{
    uint64_t Magnitude;

    if (Value < 0)
    {
        OutputCharacter(Output, '-');
        Magnitude = (uint64_t) (-(Value + 1)) + 1;
    }
    else
    {
        Magnitude = (uint64_t) Value;
    }

    OutputUnsigned(Output, Magnitude, 10, 0);
}

int putchar(int Character)
{
    char Byte;

    Byte = (char) Character;

    if (write(1, &Byte, 1) != 1)
    {
        return EOF;
    }

    return (unsigned char) Byte;
}

int puts(const char *String)
{
    size_t  Length;
    size_t  Offset;
    ssize_t Written;
    char    Newline;

    if (String == NULL)
    {
        return EOF;
    }

    Length = 0;

    while (String[Length] != '\0')
    {
        Length++;
    }

    Offset = 0;

    while (Offset < Length)
    {
        Written = write(1, String + Offset, Length - Offset);

        if (Written <= 0 || (size_t) Written > Length - Offset)
        {
            return EOF;
        }

        Offset += (size_t) Written;
    }

    Newline = '\n';

    if (write(1, &Newline, 1) != 1)
    {
        return EOF;
    }

    return 0;
}

int printf(const char *Format, ...)
{
    STDIO_OUTPUT Output = {0};
    va_list      Arguments;
    unsigned int Length;
    char         Specifier;

    if (Format == NULL)
    {
        return -1;
    }

    va_start(Arguments, Format);

    while (*Format != '\0')
    {
        if (*Format != '%')
        {
            OutputCharacter(&Output, *Format++);
            continue;
        }

        Format++;

        if (*Format == '%')
        {
            OutputCharacter(&Output, '%');
            Format++;
            continue;
        }

        Length = 0;

        if (*Format == 'l')
        {
            Length = 1;
            Format++;

            if (*Format == 'l')
            {
                Length = 2;
                Format++;
            }
        }
        else if (*Format == 'z')
        {
            Length = 3;
            Format++;
        }

        Specifier = *Format;

        if (Specifier == '\0')
        {
            break;
        }

        Format++;

        switch (Specifier)
        {
            case 'c':
            {
                OutputCharacter(&Output, (char) va_arg(Arguments, int));
                break;
            }

            case 's':
            {
                OutputString(&Output, va_arg(Arguments, const char *));
                break;
            }

            case 'd':
            case 'i':
            {
                int64_t Value;

                if (Length == 2)
                {
                    Value = va_arg(Arguments, long long);
                }
                else if (Length == 1)
                {
                    Value = va_arg(Arguments, long);
                }
                else if (Length == 3)
                {
                    Value = va_arg(Arguments, ptrdiff_t);
                }
                else
                {
                    Value = va_arg(Arguments, int);
                }

                OutputSigned(&Output, Value);
                break;
            }

            case 'u':
            case 'x':
            case 'X':
            {
                uint64_t     Value;
                unsigned int Base;

                if (Length == 2)
                {
                    Value = va_arg(Arguments, unsigned long long);
                }
                else if (Length == 1)
                {
                    Value = va_arg(Arguments, unsigned long);
                }
                else if (Length == 3)
                {
                    Value = va_arg(Arguments, size_t);
                }
                else
                {
                    Value = va_arg(Arguments, unsigned int);
                }

                Base = Specifier == 'u' ? 10 : 16;

                OutputUnsigned(&Output, Value, Base, Specifier == 'X');
                break;
            }

            case 'p':
            {
                uintptr_t Value;

                Value = (uintptr_t) va_arg(Arguments, void *);

                OutputString(&Output, "0x");
                OutputUnsigned(&Output, Value, 16, 0);
                break;
            }

            default:
            {
                OutputCharacter(&Output, '%');
                OutputCharacter(&Output, Specifier);
                break;
            }
        }
    }

    va_end(Arguments);

    FlushOutput(&Output);

    if (Output.Failed || Output.Total > 0x7FFFFFFF)
    {
        return -1;
    }

    return (int) Output.Total;
}