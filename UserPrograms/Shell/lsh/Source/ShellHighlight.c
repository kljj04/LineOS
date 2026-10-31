// UserPrograms/Shell/lsh/Source/ShellHighlight.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

STATIC CONST char *HighlightHome;
STATIC char      **HighlightEnvironment;

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
    HighlightEnvironment = Envp;
    HighlightHome = ShellGetEnvironment(Envp, "HOME");

    char  Command[SHELL_RENDER_CAPACITY];
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

    if (ShellAssignmentLength(Line) != 0)
    {
        return SHELL_INPUT_ASSIGNMENT;
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
        case 5:
            return "\033[0;38;2;183;111;183m";

        case 6:
            return "\033[0;38;2;251;83;13m";

        case 7:
            return "\033[0;38;2;242;4;207m";

        case 8:
            return "\033[0;38;2;119;119;120m";

        case 9:
            return "\033[0;38;2;0;173;0m";

        case 10:
            return ShellStyleColor(SHELL_INPUT_BUILTIN);

        case 11:
            return ShellStyleColor(SHELL_INPUT_EXTERNAL);

        case 12:
            return ShellStyleColor(SHELL_INPUT_UNKNOWN);

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

STATIC VOID ShellPathColors(CONST char *Line, SHELL_INPUT_STYLE Style, UINT8 Colors[SHELL_RENDER_CAPACITY])
{
    memset(Colors, 0, SHELL_RENDER_CAPACITY);

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

        char  Path[4096];
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

        if (Prefix + Length - Offset >= sizeof(Path))
        {
            continue;
        }

        memcpy(Path + Prefix, Line + Start + Offset, Length - Offset);
        Path[Prefix + Length - Offset] = 0;

        UINTN   Component = 0;
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

VOID ShellInputColors(CONST char *Line, SHELL_INPUT_STYLE Style, UINT8 Colors[SHELL_RENDER_CAPACITY])
{
    ShellPathColors(Line, Style, Colors);

    UINTN Assignment = ShellAssignmentLength(Line);

    if (Assignment != 0)
    {
        memset(Colors, 0, SHELL_RENDER_CAPACITY);

        char Name[SHELL_RENDER_CAPACITY];

        memcpy(Name, Line, Assignment);
        Name[Assignment] = 0;

        UINT8 Color = ShellGetEnvironment(HighlightEnvironment, Name) != NULL ? 6 : 9;

        memset(Colors, Color, Assignment);
        Colors[Assignment] = 1;
    }
    else if (Style != SHELL_INPUT_PATH)
    {
        UINT8 Color = Style == SHELL_INPUT_BUILTIN ? 10 : Style == SHELL_INPUT_EXTERNAL ? 11 : Style == SHELL_INPUT_UNKNOWN ? 12 : 0;

        memset(Colors, Color, ShellKeywordLength(Line));
    }

    char Quote = 0;

    for (UINTN Index = 0; Line[Index] != 0; Index++)
    {
        char Character = Line[Index];

        if (Quote == '\'')
        {
            if (Character == '\'')
            {
                Quote = 0;
            }

            continue;
        }

        if (Character == '\\')
        {
            if (Line[Index + 1] != 0)
            {
                Index++;
            }

            continue;
        }

        if (Character == '\'' && Quote == 0)
        {
            Quote = Character;
            continue;
        }

        if (Character == '"')
        {
            Quote = Quote == '"' ? 0 : '"';
            continue;
        }

        if (Character != '$')
        {
            continue;
        }

        Colors[Index] = 5;

        UINTN   Position = Index + 1;
        BOOLEAN Braces = Line[Position] == '{';

        if (Braces)
        {
            Colors[Position++] = 5;
        }

        UINTN Start = Position;
        UINT8 Color = 8;

        if (Line[Position] == '?' || Line[Position] == '$')
        {
            Color = 6;
            Position++;
        }
        else if (ShellVariableStart(Line[Position]))
        {
            char Name[SHELL_RENDER_CAPACITY];

            while (ShellVariablePart(Line[Position]))
            {
                Position++;
            }

            UINTN Length = Position - Start;

            memcpy(Name, Line + Start, Length);
            Name[Length] = 0;
            Color = ShellGetVariable(Name) != NULL ? 6 : ShellGetEnvironment(HighlightEnvironment, Name) != NULL ? 7 : 8;
        }

        memset(Colors + Start, Color, Position - Start);

        if (Braces && Line[Position] == '}')
        {
            Colors[Position++] = 5;
        }

        Index = Position - 1;
    }
}

VOID ShellApplyInputColor(CONST char *Line, SHELL_INPUT_STYLE Style)
{
    UINTN Length = strlen(Line);
    UINT8 Colors[SHELL_RENDER_CAPACITY];

    ShellInputColors(Line, Style, Colors);

    CONST char *Color = ShellPathColor(Length != 0 ? Colors[Length - 1] : 0);

    write(1, Color, strlen(Color));
}

VOID ShellRenderInputSpan(CONST char *Text, CONST UINT8 *Colors, UINTN Start, UINTN End, BOOLEAN Prompt)
{
    if (Prompt)
    {
        printf("\033[38;2;80;227;194m┗━\033[38;2;215;122;215m>\033[0m ");
    }

    UINTN Position = Start;

    while (Position < End)
    {
        UINTN Begin = Position++;

        while (Position < End && Colors[Position] == Colors[Begin])
        {
            Position++;
        }

        CONST char *Color = ShellPathColor(Colors[Begin]);

        write(1, Color, strlen(Color));
        write(1, Text + Begin, Position - Begin);
    }

    write(1, "\033[0m", 4);
}

VOID ShellRenderInputRange(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style, UINTN Start, UINTN End)
{
    char  Text[SHELL_RENDER_CAPACITY];
    UINTN Length = strlen(Line);

    memcpy(Text, Line, Length + 1);
    ShellAppend(Text, &Length, IMECodepoint(IME));

    UINT8 Colors[SHELL_RENDER_CAPACITY];

    ShellInputColors(Text, Style, Colors);

    if (End > Length)
    {
        End = Length;
    }

    write(1, "\r\033[0m\033[2K", 9);
    ShellRenderInputSpan(Text, Colors, Start, End, TRUE);
}

VOID ShellRenderInput(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style)
{
    ShellRenderInputRange(Line, IME, Style, 0, strlen(Line) + (IMECodepoint(IME) != 0 ? 3 : 0));
}
