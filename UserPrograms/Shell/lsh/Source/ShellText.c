// UserPrograms/Shell/lsh/Source/ShellText.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

UINTN ShellEncode(UINT32 Codepoint, char *Bytes)
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

VOID ShellEcho(UINT32 Codepoint)
{
    char Bytes[3];

    if (Codepoint != 0)
    {
        write(1, Bytes, ShellEncode(Codepoint, Bytes));
    }
}

VOID ShellAppend(char *Line, UINTN *Length, UINT32 Codepoint)
{
    if (Codepoint != 0)
    {
        *Length += ShellEncode(Codepoint, Line + *Length);
        Line[*Length] = 0;
    }
}

VOID ShellPrompt(CONST char *Username)
{
    char Path[4096];

    printf("\033[0m\033[38;2;80;227;194m┏━[\033[38;2;104;203;97m%s\033[38;2;80;227;194m]━(\033[38;2;215;205;111m%s\033[38;2;80;227;194m)\n┗━\033[38;2;215;122;215m>\033[0m ", getcwd(Path, sizeof(Path)) != NULL ? Path : "?", Username);
}
