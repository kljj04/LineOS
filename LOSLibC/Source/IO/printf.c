// LOSLibC/Source/IO/printf.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>

#define PRINTF_FLAG_LEFT      (1U << 0)
#define PRINTF_FLAG_PLUS      (1U << 1)
#define PRINTF_FLAG_SPACE     (1U << 2)
#define PRINTF_FLAG_ALTERNATE (1U << 3)
#define PRINTF_FLAG_ZERO      (1U << 4)

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

static void OutputRepeat(STDIO_OUTPUT *Output, char Character, size_t Count)
{
    while (Count-- != 0)
    {
        OutputCharacter(Output, Character);
    }
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

static void OutputStringField(STDIO_OUTPUT *Output, const char *String, unsigned int Flags, unsigned int Width, int Precision)
{
    size_t Length = 0;
    size_t Padding;

    if (String == NULL)
    {
        String = "(null)";
    }

    while (String[Length] != '\0' && (Precision < 0 || Length < (size_t) Precision))
    {
        Length++;
    }

    Padding = Width > Length ? Width - Length : 0;

    if ((Flags & PRINTF_FLAG_LEFT) == 0)
    {
        OutputRepeat(Output, ' ', Padding);
    }

    for (size_t Index = 0; Index < Length; Index++)
    {
        OutputCharacter(Output, String[Index]);
    }

    if (Flags & PRINTF_FLAG_LEFT)
    {
        OutputRepeat(Output, ' ', Padding);
    }
}

static void OutputUnsigned(STDIO_OUTPUT *Output, uint64_t Value, unsigned int Base, int Uppercase, unsigned int Flags, unsigned int Width, int Precision, const char *Prefix, char Sign)
{
    char        Digits[32];
    const char *Alphabet;
    size_t      Count;
    size_t      Zeroes;
    size_t      PrefixLength;
    size_t      Padding;
    size_t      ContentLength;

    Alphabet = Uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    Count = 0;

    if (Value != 0 || Precision != 0)
    {
        do
        {
            Digits[Count++] = Alphabet[Value % Base];
            Value /= Base;
        } while (Value != 0);
    }

    Zeroes = Precision >= 0 && (size_t) Precision > Count ? (size_t) Precision - Count : 0;
    PrefixLength = Prefix != NULL ? (Sign != 0 ? 1 : 0) + strlen(Prefix) : (Sign != 0 ? 1 : 0);
    ContentLength = PrefixLength + Zeroes + Count;
    Padding = Width > ContentLength ? Width - ContentLength : 0;

    if ((Flags & PRINTF_FLAG_ZERO) && !(Flags & PRINTF_FLAG_LEFT) && Precision < 0)
    {
        Zeroes += Padding;
        Padding = 0;
    }

    if ((Flags & PRINTF_FLAG_LEFT) == 0)
    {
        OutputRepeat(Output, ' ', Padding);
    }

    if (Sign != 0)
    {
        OutputCharacter(Output, Sign);
    }

    if (Prefix != NULL)
    {
        OutputString(Output, Prefix);
    }

    OutputRepeat(Output, '0', Zeroes);

    while (Count != 0)
    {
        OutputCharacter(Output, Digits[--Count]);
    }

    if (Flags & PRINTF_FLAG_LEFT)
    {
        OutputRepeat(Output, ' ', Padding);
    }
}

static void OutputSigned(STDIO_OUTPUT *Output, int64_t Value, unsigned int Flags, unsigned int Width, int Precision)
{
    uint64_t Magnitude;

    char     Sign;

    if (Value < 0)
    {
        Sign = '-';
        Magnitude = (uint64_t) (-(Value + 1)) + 1;
    }
    else
    {
        Sign = (Flags & PRINTF_FLAG_PLUS) ? '+' : ((Flags & PRINTF_FLAG_SPACE) ? ' ' : 0);
        Magnitude = (uint64_t) Value;
    }

    OutputUnsigned(Output, Magnitude, 10, 0, Flags, Width, Precision, NULL, Sign);
}

static unsigned int ParseNumber(const char **Format)
{
    unsigned int Value = 0;

    while (**Format >= '0' && **Format <= '9')
    {
        unsigned int Digit = (unsigned int) (**Format - '0');

        if (Value > (UINT_MAX - Digit) / 10)
        {
            Value = UINT_MAX;
        }
        else if (Value != UINT_MAX)
        {
            Value = Value * 10 + Digit;
        }

        (*Format)++;
    }

    return Value;
}

int printf(const char *Format, ...)
{
    STDIO_OUTPUT Output = {0};

    va_list      Arguments;

    unsigned int Flags;
    unsigned int Width;
    unsigned int Length;
    int          Precision;
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

        Flags = 0;

        while (1)
        {
            switch (*Format)
            {
                case '-':
                {
                    Flags |= PRINTF_FLAG_LEFT;
                    break;
                }

                case '+':
                {
                    Flags |= PRINTF_FLAG_PLUS;
                    break;
                }

                case ' ':
                {
                    Flags |= PRINTF_FLAG_SPACE;
                    break;
                }

                case '#':
                {
                    Flags |= PRINTF_FLAG_ALTERNATE;
                    break;
                }

                case '0':
                {
                    Flags |= PRINTF_FLAG_ZERO;
                    break;
                }

                default:
                {
                    goto ParseWidth;
                }
            }

            Format++;
        }

    ParseWidth:
        Width = ParseNumber(&Format);
        Precision = -1;

        if (*Format == '.')
        {
            Format++;
            Precision = (int) ParseNumber(&Format);

            if (Precision == 0 && *Format != '\0')
            {
                // Zero precision is valid.
            }
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
            OutputCharacter(&Output, '%');
            break;
        }

        Format++;

        if (Flags & PRINTF_FLAG_LEFT)
        {
            Flags &= ~PRINTF_FLAG_ZERO;
        }

        switch (Specifier)
        {
            case 'c':
            {
                char         Character = (char) va_arg(Arguments, int);
                unsigned int Padding = Width > 1 ? Width - 1 : 0;

                if (!(Flags & PRINTF_FLAG_LEFT))
                {
                    OutputRepeat(&Output, ' ', Padding);
                }

                OutputCharacter(&Output, Character);

                if (Flags & PRINTF_FLAG_LEFT)
                {
                    OutputRepeat(&Output, ' ', Padding);
                }

                break;
            }

            case 's':
            {
                OutputStringField(&Output, va_arg(Arguments, const char *), Flags, Width, Precision);
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

                OutputSigned(&Output, Value, Flags, Width, Precision);
                break;
            }

            case 'u':
            case 'x':
            case 'X':
            {
                uint64_t     Value;

                unsigned int Base;
                const char  *Prefix = NULL;

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

                if ((Flags & PRINTF_FLAG_ALTERNATE) && Value != 0 && Base == 16)
                {
                    Prefix = Specifier == 'X' ? "0X" : "0x";
                }

                OutputUnsigned(&Output, Value, Base, Specifier == 'X', Flags, Width, Precision, Prefix, 0);
                break;
            }

            case 'p':
            {
                uintptr_t Value = (uintptr_t) va_arg(Arguments, void *);

                OutputUnsigned(&Output, Value, 16, 0, Flags, Width, Precision, "0x", 0);
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
