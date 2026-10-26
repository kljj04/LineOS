// UserPrograms/Shell/Shell.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <string.h>
#include <IME.h>

STATIC UINTN ShellEncode(UINT32 Codepoint, char *Bytes)
{
    if (Codepoint < 0x80)
    {
        Bytes[0] = (char) Codepoint;
        return 1;
    }
    if (Codepoint < 0x800)
    {
        Bytes[0] = (char) (0xC0 | (Codepoint >> 6));
        Bytes[1] = (char) (0x80 | (Codepoint & 63));
        return 2;
    }
    Bytes[0] = (char) (0xE0 | (Codepoint >> 12));
    Bytes[1] = (char) (0x80 | ((Codepoint >> 6) & 63));
    Bytes[2] = (char) (0x80 | (Codepoint & 63));
    return 3;
}

STATIC VOID ShellEcho(UINT32 Codepoint)
{
    char Bytes[3];
    if (Codepoint != 0)
    {
        write(1, Bytes, ShellEncode(Codepoint, Bytes));
    }
}

STATIC VOID ShellAppend(char *Line, UINTN *Length, UINT32 Codepoint)
{
    if (Codepoint != 0)
    {
        *Length += ShellEncode(Codepoint, Line + *Length);
        Line[*Length] = 0;
    }
}

INT32 _Start(VOID)
{
    char      Line[256] = {0};
    UINTN     Length = 0;
    UINT8     Byte;
    BOOLEAN   Korean = FALSE;
    IME_STATE IME;

    IMEReset(&IME);
    puts("LineOS Shell: help, echo, or an absolute program path");
    puts("Hangul / Right Alt: Korean input");
    printf("lsh> ");
    for (;;)
    {
        if (read(0, &Byte, 1) != 1)
        {
            exit(1);
        }
        UINT32 Preedit = IMECodepoint(&IME);
        if (Byte == 14)
        {
            ShellAppend(Line, &Length, Preedit);
            IMEReset(&IME);
            Korean = !Korean;
        }
        else if (Byte == '\b' || Byte == 127)
        {
            if (Preedit != 0)
            {
                write(1, "\b \b", 3);
                IMEBackspace(&IME);
                ShellEcho(IMECodepoint(&IME));
            }
            else if (Length != 0)
            {
                do
                {
                    Length--;
                } while (Length != 0 && ((UINT8) Line[Length] & 0xC0) == 0x80);
                Line[Length] = 0;
                write(1, "\b \b", 3);
            }
        }
        else if (Byte == '\n' || Byte == '\r')
        {
            ShellAppend(Line, &Length, Preedit);
            IMEReset(&IME);
            write(1, "\n", 1);
            if (strcmp(Line, "help") == 0)
            {
                puts("help | echo TEXT | /bin/Program.lix (asynchronous)");
            }
            else if (strncmp(Line, "echo ", 5) == 0)
            {
                puts(Line + 5);
            }
            else if (Length != 0 && (Line[0] != '/' || spawn(Line) < 0))
            {
                puts("Shell: cannot launch program");
            }
            Length = 0;
            Line[0] = 0;
            printf("lsh> ");
        }
        else if (Byte >= 32 && Byte < 127 && Length + 6 < sizeof(Line))
        {
            IME_STATE Next = IME;
            UINT32    Committed;
            if (Korean && IMEFeed(&Next, Byte, &Committed))
            {
                if (Preedit != 0)
                {
                    write(1, "\b \b", 3);
                }
                ShellAppend(Line, &Length, Committed);
                ShellEcho(Committed);
                IME = Next;
                ShellEcho(IMECodepoint(&IME));
            }
            else
            {
                ShellAppend(Line, &Length, Preedit);
                IMEReset(&IME);
                Line[Length++] = (char) Byte;
                Line[Length] = 0;
                write(1, &Byte, 1);
            }
        }
    }
}
