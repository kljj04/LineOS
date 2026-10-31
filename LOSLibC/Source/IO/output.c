// LOSLibC/Source/IO/output.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdio.h>
#include <unistd.h>
#include <stddef.h>

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
