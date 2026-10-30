// UserPrograms/Shell/lsh/Source/ShellHighlight.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

STATIC CONST char *HighlightHome;

STATIC UINTN ShellKeywordLength(CONST char *Line)
{
    UINTN Length = 0;
    while (Line[Length] != 0 && Line[Length] != ' ')
    {
        Length++;
    }
    return Length;
}

SHELL_INPUT_STYLE ShellInputStyle(CONST char *Line, IME_STATE *IME, char **Envp)
{
    HighlightHome = ShellGetEnvironment(Envp, "HOME");
    char  Command[260];
    UINTN Length = ShellKeywordLength(Line);
    memcpy(Command, Line, Length);
    if (Line[Length] == 0 && IMECodepoint(IME) != 0)
    {
        Length += ShellEncode(IMECodepoint(IME), Command + Length);
    }
    Command[Length] = 0;
    if (Length == 0)
    {
        return SHELL_INPUT_EMPTY;
    }
    if (strchr(Command, '/') != NULL || strcmp(Command, "~") == 0)
    {
        return SHELL_INPUT_PATH;
    }
    switch (ShellGetCommand(Command))
    {
        case SHELL_COMMAND_HELP:
        case SHELL_COMMAND_ECHO:
        case SHELL_COMMAND_CD:
        case SHELL_COMMAND_TYPE:
        case SHELL_COMMAND_EXIT:
        case SHELL_COMMAND_LOGOUT:
        case SHELL_COMMAND_PWD:
        {
            return SHELL_INPUT_BUILTIN;
        }
        default:
        {
            char Path[4096];
            return ShellFindCommand(Command, Envp, Path) ? SHELL_INPUT_EXTERNAL : SHELL_INPUT_UNKNOWN;
        }
    }
}

STATIC CONST char *ShellStyleColor(SHELL_INPUT_STYLE Style)
{
    switch (Style)
    {
        case SHELL_INPUT_BUILTIN:
        {
            return "\033[0;38;2;214;122;122m";
        }
        case SHELL_INPUT_EXTERNAL:
        {
            return "\033[0;38;2;104;203;97m";
        }
        case SHELL_INPUT_UNKNOWN:
        {
            return "\033[0;38;2;255;255;255;48;2;255;95;95m";
        }
        default:
        {
            return "\033[0m";
        }
    }
}

STATIC CONST char *ShellPathColor(UINT8 Color)
{
    switch (Color)
    {
        case 1:
            return "\033[0;38;2;0;134;255m";
        case 2:
            return "\033[0;38;2;104;203;97m";
        case 4:
            return "\033[0;38;2;215;205;111m";
        case 3:
            return "\033[0;38;2;80;227;194m";
        default:
            return "\033[0m";
    }
}

VOID ShellInputColors(CONST char *Line, SHELL_INPUT_STYLE Style, UINT8 Colors[260])
{
    memset(Colors, 0, 260);
    if (Style != SHELL_INPUT_BUILTIN && Style != SHELL_INPUT_EXTERNAL && Style != SHELL_INPUT_PATH)
    {
        return;
    }
    UINTN Position = Style == SHELL_INPUT_PATH ? 0 : ShellKeywordLength(Line);
    while (Line[Position] != 0)
    {
        if (Line[Position] == ' ')
        {
            Position++;
            continue;
        }
        UINTN Start = Position;
        while (Line[Position] != 0 && Line[Position] != ' ')
        {
            Position++;
        }
        UINTN Length = Position - Start;
        if (Line[Start] == '-')
        {
            memset(Colors + Start, 3, Length);
            continue;
        }
        char Path[4096];
        UINTN Prefix = 0;
        UINTN Offset = 0;
        if (Line[Start] == '~' && (Length == 1 || Line[Start + 1] == '/'))
        {
            if (HighlightHome == NULL || *HighlightHome == 0)
            {
                continue;
            }
            Prefix = strlen(HighlightHome);
            if (Prefix + Length >= sizeof(Path))
            {
                continue;
            }
            memcpy(Path, HighlightHome, Prefix);
            Offset = 1;
        }
        memcpy(Path + Prefix, Line + Start + Offset, Length - Offset);
        Path[Prefix + Length - Offset] = 0;
        UINTN Component = 0;
        BOOLEAN Missing = FALSE;
        for (UINTN End = 0; End < Length; End++)
        {
            UINTN PathEnd = Prefix + End + 1 - Offset;
            if (Line[Start + End] != '/' && End + 1 != Length)
            {
                continue;
            }
            char Saved = Path[PathEnd];
            Path[PathEnd] = 0;
            struct stat Info;
            if (!Missing && stat(Path, &Info) == 0)
            {
                UINT8 Color = S_ISDIR(Info.st_mode) ? 1 : S_ISREG(Info.st_mode) ? 2 : (S_ISCHR(Info.st_mode) || (Info.st_mode & S_IFMT) == 0060000U) ? 4 : 0;
                memset(Colors + Start + Component, Color, End + 1 - Component);
                if (!S_ISDIR(Info.st_mode) && End + 1 < Length)
                {
                    Missing = TRUE;
                }
            }
            else
            {
                Missing = TRUE;
            }
            Path[PathEnd] = Saved;
            Component = End + 1;
        }
    }
}

VOID ShellApplyInputColor(CONST char *Line, SHELL_INPUT_STYLE Style)
{
    UINTN Length = strlen(Line);
    UINT8 Colors[260];
    ShellInputColors(Line, Style, Colors);
    CONST char *Color = Style != SHELL_INPUT_PATH && strchr(Line, ' ') == NULL ? ShellStyleColor(Style) : ShellPathColor(Length != 0 ? Colors[Length - 1] : 0);
    write(1, Color, strlen(Color));
}

VOID ShellRenderInput(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style)
{
    char Text[260];
    UINTN Length = strlen(Line);
    memcpy(Text, Line, Length + 1);
    ShellAppend(Text, &Length, IMECodepoint(IME));
    UINT8 Colors[260];
    ShellInputColors(Text, Style, Colors);
    UINTN Keyword = Style == SHELL_INPUT_PATH ? 0 : ShellKeywordLength(Text);
    printf("\r\033[0m\033[2K┗━> %s", ShellStyleColor(Style));
    write(1, Text, Keyword);
    UINTN Position = Keyword;
    while (Position < Length)
    {
        UINTN Start = Position++;
        while (Position < Length && Colors[Position] == Colors[Start])
        {
            Position++;
        }
        CONST char *Color = ShellPathColor(Colors[Start]);
        write(1, Color, strlen(Color));
        write(1, Text + Start, Position - Start);
    }
    ShellApplyInputColor(Text, Style);
}
