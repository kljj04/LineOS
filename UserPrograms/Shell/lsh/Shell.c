// UserPrograms/Shell/Shell.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IME.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>

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

STATIC VOID ShellPrompt(CONST char *Username)
{
    char Path[4096];
    printf("\033[0m┏━[%s]━(%s)\n┗━> ", getcwd(Path, sizeof(Path)) != NULL ? Path : "?", Username);
}

typedef enum
{
    SHELL_COMMAND_EMPTY,
    SHELL_COMMAND_HELP,
    SHELL_COMMAND_ECHO,
    SHELL_COMMAND_CD,
    SHELL_COMMAND_PWD,
    SHELL_COMMAND_EXTERNAL
} SHELL_COMMAND;

STATIC SHELL_COMMAND ShellGetCommand(CONST char *Line)
{
    if (*Line == 0)
    {
        return SHELL_COMMAND_EMPTY;
    }
    if (strcmp(Line, "help") == 0)
    {
        return SHELL_COMMAND_HELP;
    }
    if (strcmp(Line, "echo") == 0 || strncmp(Line, "echo ", 5) == 0)
    {
        return SHELL_COMMAND_ECHO;
    }
    if (strcmp(Line, "cd") == 0 || strncmp(Line, "cd ", 3) == 0)
    {
        return SHELL_COMMAND_CD;
    }
    if (strcmp(Line, "pwd") == 0)
    {
        return SHELL_COMMAND_PWD;
    }
    return SHELL_COMMAND_EXTERNAL;
}

STATIC VOID ShellCD(CONST char *Path)
{
    if (chdir(*Path == 0 ? "/" : Path) < 0)
    {
        puts("cd: cannot change directory");
    }
}

STATIC VOID ShellPWD(VOID)
{
    char Path[4096];
    if (getcwd(Path, sizeof(Path)) == NULL)
    {
        puts("pwd: cannot get current directory");
        return;
    }
    puts(Path);
}

STATIC CONST char *ShellGetEnvironment(char **Envp, CONST char *Name)
{
    UINTN Length = strlen(Name);
    for (UINTN Index = 0; Envp != NULL && Envp[Index] != NULL; Index++)
    {
        if (strncmp(Envp[Index], Name, Length) == 0 && Envp[Index][Length] == '=')
        {
            return Envp[Index] + Length + 1;
        }
    }
    return NULL;
}

STATIC BOOLEAN ShellExecutable(CONST char *Path)
{
    struct stat Info;
    if (stat(Path, &Info) != 0)
    {
        return FALSE;
    }
    if (!S_ISREG(Info.st_mode) || (Info.st_mode & 0111) == 0)
    {
        errno = EACCES;
        return FALSE;
    }
    return TRUE;
}

STATIC BOOLEAN ShellFindCommand(CONST char *Command, char **Envp, char Path[4096])
{
    UINTN CommandLength = strlen(Command);
    if (CommandLength >= 4096)
    {
        errno = ENAMETOOLONG;
        return FALSE;
    }
    if (strchr(Command, '/') != NULL)
    {
        memcpy(Path, Command, CommandLength + 1);
        return ShellExecutable(Path);
    }
    CONST char *Search = ShellGetEnvironment(Envp, "PATH");
    if (Search == NULL)
    {
        Search = "/bin/lsh";
    }
    BOOLEAN HasSuffix = CommandLength >= 4 && strcmp(Command + CommandLength - 4, ".lix") == 0;
    INT32   Error = ENOENT;
    for (;;)
    {
        UINTN Length = 0;
        while (Search[Length] != 0 && Search[Length] != ':')
        {
            Length++;
        }
        UINTN Separator = Length != 0 && Search[Length - 1] != '/' ? 1 : 0;
        if (Length < 4096 && CommandLength < 4096 - Length - Separator)
        {
            memcpy(Path, Search, Length);
            if (Separator != 0)
            {
                Path[Length] = '/';
            }
            UINTN End = Length + Separator + CommandLength;
            memcpy(Path + Length + Separator, Command, CommandLength + 1);
            for (UINT32 Attempt = 0; Attempt < (HasSuffix ? 1U : 2U); Attempt++)
            {
                if (Attempt != 0)
                {
                    if (End > 4091)
                    {
                        break;
                    }
                    memcpy(Path + End, ".lix", 5);
                }
                if (ShellExecutable(Path))
                {
                    return TRUE;
                }
                if (errno == EACCES)
                {
                    Error = EACCES;
                }
                else if (errno != ENOENT && errno != ENOTDIR)
                {
                    Error = errno;
                }
            }
        }
        else if (Error != EACCES)
        {
            Error = ENAMETOOLONG;
        }
        if (Search[Length] == 0)
        {
            break;
        }
        Search += Length + 1;
    }
    errno = Error;
    return FALSE;
}

STATIC VOID ShellExecute(char *Line, char **Envp)
{
    char  *Arguments[65];
    UINT32 Count = 0;
    char  *Position = Line;
    while (*Position != 0)
    {
        while (*Position == ' ')
        {
            Position++;
        }
        if (*Position == 0)
        {
            break;
        }
        if (Count == 64)
        {
            errno = E2BIG;
            return;
        }
        Arguments[Count++] = Position;
        while (*Position != 0 && *Position != ' ')
        {
            Position++;
        }
        if (*Position != 0)
        {
            *Position++ = 0;
        }
    }
    Arguments[Count] = NULL;
    char Path[4096];
    if (Count != 0 && ShellFindCommand(Arguments[0], Envp, Path))
    {
        execve(Path, Arguments, Envp);
    }
}

typedef enum
{
    SHELL_INPUT_EMPTY,
    SHELL_INPUT_BUILTIN,
    SHELL_INPUT_EXTERNAL,
    SHELL_INPUT_UNKNOWN
} SHELL_INPUT_STYLE;

STATIC UINTN ShellKeywordLength(CONST char *Line)
{
    UINTN Length = 0;
    while (Line[Length] != 0 && Line[Length] != ' ')
    {
        Length++;
    }
    return Length;
}

STATIC SHELL_INPUT_STYLE ShellInputStyle(CONST char *Line, IME_STATE *IME, char **Envp)
{
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
    switch (ShellGetCommand(Command))
    {
        case SHELL_COMMAND_HELP:
        case SHELL_COMMAND_ECHO:
        case SHELL_COMMAND_CD:
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

STATIC CONST char *ShellArgumentColor(CONST char *Argument, SHELL_INPUT_STYLE Style)
{
    if (Style == SHELL_INPUT_BUILTIN || Style == SHELL_INPUT_EXTERNAL)
    {
        if (Argument[0] == '-')
        {
            return "\033[0;38;2;80;227;194m";
        }
        char Path[256];
        UINTN Length = 0;
        while (Argument[Length] != 0 && Argument[Length] != ' ' && Length + 1 < sizeof(Path))
        {
            Path[Length] = Argument[Length];
            Length++;
        }
        Path[Length] = 0;
        struct stat Info;
        if (Length != 0 && (Argument[Length] == 0 || Argument[Length] == ' ') && stat(Path, &Info) == 0)
        {
            if (S_ISDIR(Info.st_mode))
            {
                return "\033[0;38;2;0;134;255m";
            }
            if (S_ISREG(Info.st_mode))
            {
                return "\033[0;38;2;104;203;97m";
            }
        }
    }
    return "\033[0m";
}

STATIC VOID ShellApplyInputColor(CONST char *Line, SHELL_INPUT_STYLE Style)
{
    CONST char *Argument = strrchr(Line, ' ');
    CONST char *Color = Argument != NULL ? ShellArgumentColor(Argument + 1, Style) : ShellStyleColor(Style);
    write(1, Color, strlen(Color));
}

STATIC VOID ShellRenderInput(CONST char *Line, IME_STATE *IME, SHELL_INPUT_STYLE Style)
{
    UINTN Length = ShellKeywordLength(Line);
    CONST char *Color = ShellStyleColor(Style);
    printf("\r\033[0m\033[2K┗━> %s", Color);
    write(1, Line, Length);
    while (Line[Length] != 0)
    {
        if (Line[Length] == ' ')
        {
            write(1, "\033[0m ", 5);
            Length++;
            continue;
        }
        UINTN Start = Length;
        while (Line[Length] != 0 && Line[Length] != ' ')
        {
            Length++;
        }
        Color = ShellArgumentColor(Line + Start, Style);
        write(1, Color, strlen(Color));
        write(1, Line + Start, Length - Start);
    }
    ShellApplyInputColor(Line, Style);
    ShellEcho(IMECodepoint(IME));
}

INT32 main(INT32 Argc, char **Argv, char **Envp)
{
    char      Line[256] = {0};
    UINTN     Length = 0;
    UINT8     Byte;
    BOOLEAN   Korean = FALSE;
    IME_STATE IME;

    CONST char *Username = ShellGetEnvironment(Envp, "USER");
    if (Username == NULL || *Username == 0)
    {
        Username = "?";
    }
    (VOID) Argc;
    (VOID) Argv;

    IMEReset(&IME);
    puts("LineOS Shell: help, cd, pwd, echo, or a program path");
    puts("Hangul / Right Alt: Korean input");
    SHELL_INPUT_STYLE InputStyle = SHELL_INPUT_EMPTY;
    ShellPrompt(Username);
    ShellApplyInputColor(Line, InputStyle);
    for (;;)
    {
        if (read(0, &Byte, 1) != 1)
        {
            return 1;
        }
        CONST char *PreviousArgument = strrchr(Line, ' ');
        CONST char *PreviousColor = PreviousArgument != NULL ? ShellArgumentColor(PreviousArgument + 1, InputStyle) : ShellStyleColor(InputStyle);
        UINT32  Preedit = IMECodepoint(&IME);
        BOOLEAN Redraw = FALSE;
        if (Byte == 14)
        {
            ShellAppend(Line, &Length, Preedit);
            IMEReset(&IME);
            Korean = !Korean;
            Redraw = TRUE;
        }
        else if (Byte == '\b' || Byte == 127)
        {
            if (Preedit != 0)
            {
                write(1, "\033[0m\b \b", 7);
                IMEBackspace(&IME);
                ShellApplyInputColor(Line, InputStyle);
                ShellEcho(IMECodepoint(&IME));
            }
            else if (Length != 0)
            {
                do
                {
                    Length--;
                } while (Length != 0 && ((UINT8) Line[Length] & 0xC0) == 0x80);
                Line[Length] = 0;
                write(1, "\033[0m\b \b", 7);
            }
            Redraw = TRUE;
        }
        else if (Byte == '\n' || Byte == '\r')
        {
            ShellAppend(Line, &Length, Preedit);
            IMEReset(&IME);
            write(1, "\033[0m\n", 5);
            switch (ShellGetCommand(Line))
            {
                case SHELL_COMMAND_EMPTY:
                {
                    break;
                }
                case SHELL_COMMAND_HELP:
                {
                    puts("help | cd [PATH] | pwd | echo TEXT | PATH");
                    break;
                }
                case SHELL_COMMAND_ECHO:
                {
                    puts(Line[4] == 0 ? "" : Line + 5);
                    break;
                }
                case SHELL_COMMAND_CD:
                {
                    ShellCD(Line + (Line[2] == 0 ? 2 : 3));
                    break;
                }
                case SHELL_COMMAND_PWD:
                {
                    ShellPWD();
                    break;
                }
                case SHELL_COMMAND_EXTERNAL:
                {
                    pid_t Child = fork();
                    if (Child == 0)
                    {
                        ShellExecute(Line, Envp);
                        printf("%s: %s\n", Line, errno == ENOENT ? "command not found" : "cannot execute");
                        return 127;
                    }
                    if (Child < 0)
                    {
                        puts("Shell: fork failed");
                    }
                    else
                    {
                        pid_t Result;
                        do
                        {
                            Result = waitpid(Child, NULL, 0);
                        } while (Result < 0 && errno == EINTR);
                        if (Result < 0)
                        {
                            puts("Shell: waitpid failed");
                        }
                    }
                    break;
                }
            }
            Length = 0;
            Line[0] = 0;
            InputStyle = SHELL_INPUT_EMPTY;
            ShellPrompt(Username);
            ShellApplyInputColor(Line, InputStyle);
        }
        else if (Byte >= 32 && Byte < 127 && Length + 6 < sizeof(Line))
        {
            IME_STATE Next = IME;
            UINT32    Committed;
            if (Korean && IMEFeed(&Next, Byte, &Committed))
            {
                if (Preedit != 0)
                {
                    write(1, "\033[0m\b \b", 7);
                }
                ShellAppend(Line, &Length, Committed);
                ShellApplyInputColor(Line, InputStyle);
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
                ShellApplyInputColor(Line, InputStyle);
                write(1, &Byte, 1);
            }
            Redraw = TRUE;
        }
        if (Redraw)
        {
            SHELL_INPUT_STYLE Style = ShellInputStyle(Line, &IME, Envp);
            CONST char *Argument = strrchr(Line, ' ');
            CONST char *Color = Argument != NULL ? ShellArgumentColor(Argument + 1, Style) : ShellStyleColor(Style);
            if (Style != InputStyle || strcmp(PreviousColor, Color) != 0)
            {
                InputStyle = Style;
                ShellRenderInput(Line, &IME, InputStyle);
            }
            ShellApplyInputColor(Line, InputStyle);
        }
    }
}
