// LOSLibC/Source/Runtime/strtol.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

static int DigitValue(unsigned char Character)
{
    if (Character >= '0' && Character <= '9')
    {
        return Character - '0';
    }

    Character |= 32;

    return Character >= 'a' && Character <= 'z' ? Character - 'a' + 10 : 36;
}

long strtol(const char *Text, char **End, int Base)
{
    if (End != NULL)
    {
        *End = (char *) Text;
    }

    if (Base != 0 && (Base < 2 || Base > 36))
    {
        errno = EINVAL;

        return 0;
    }

    while (*Text == ' ' || (*Text >= '\t' && *Text <= '\r'))
    {
        Text++;
    }

    int Negative = *Text == '-';

    if (Negative || *Text == '+')
    {
        Text++;
    }

    if ((Base == 0 || Base == 16) && Text[0] == '0' && (Text[1] == 'x' || Text[1] == 'X') && DigitValue(Text[2]) < 16)
    {
        Text += 2;
        Base = 16;
    }

    if (Base == 0)
    {
        Base = *Text == '0' ? 8 : 10;
    }

    const char *Start = Text;
    unsigned long Value = 0;
    unsigned long Limit = (unsigned long) LONG_MAX + Negative;
    int Overflow = 0;

    for (int Digit; (Digit = DigitValue(*Text)) < Base; Text++)
    {
        if (Value > (Limit - Digit) / Base)
        {
            Overflow = 1;
        }
        else if (!Overflow)
        {
            Value = Value * Base + Digit;
        }
    }

    if (End != NULL && Text != Start)
    {
        *End = (char *) Text;
    }

    if (Overflow)
    {
        errno = ERANGE;

        return Negative ? LONG_MIN : LONG_MAX;
    }

    return Negative && Value != 0 ? -(long) (Value - 1) - 1 : (long) Value;
}

int atoi(const char *Text)
{
    return (int) strtol(Text, NULL, 10);
}

unsigned long long strtoull(const char *Text, char **End, int Base)
{
    if (End != NULL)
    {
        *End = (char *) Text;
    }

    if (Base != 0 && (Base < 2 || Base > 36))
    {
        errno = EINVAL;

        return 0;
    }

    while (*Text == ' ' || (*Text >= '\t' && *Text <= '\r'))
    {
        Text++;
    }

    int Negative = *Text == '-';

    if (Negative || *Text == '+')
    {
        Text++;
    }

    if ((Base == 0 || Base == 16) && Text[0] == '0' && (Text[1] == 'x' || Text[1] == 'X') && DigitValue(Text[2]) < 16)
    {
        Text += 2;
        Base = 16;
    }

    if (Base == 0)
    {
        Base = *Text == '0' ? 8 : 10;
    }

    const char *Start = Text;
    unsigned long long Value = 0;
    unsigned long long Limit = ULLONG_MAX;
    int Overflow = 0;

    for (int Digit; (Digit = DigitValue(*Text)) < Base; Text++)
    {
        if (Value > (Limit - Digit) / Base)
        {
            Overflow = 1;
        }
        else if (!Overflow)
        {
            Value = Value * Base + Digit;
        }
    }

    if (End != NULL && Text != Start)
    {
        *End = (char *) Text;
    }

    if (Overflow)
    {
        errno = ERANGE;

        return ULLONG_MAX;
    }

    return Negative ? 0ULL - Value : Value;
}

long long strtoll(const char *Text, char **End, int Base)
{
    _Static_assert(LONG_MAX == LLONG_MAX && LONG_MIN == LLONG_MIN, "LineOS LP64");

    return strtol(Text, End, Base);
}
