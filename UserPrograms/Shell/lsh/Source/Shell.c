// UserPrograms/Shell/lsh/Source/Shell.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>

INT32 main(INT32 Argc, char **Argv, char **Envp)
{
    char      Line[256] = {0};
    UINTN     Length = 0;
    UINT8     Byte;
    BOOLEAN   Korean = FALSE;
    IME_STATE IME;
    UINT8 EscapeState = 0;

    CONST char *Username = ShellGetEnvironment(Envp, "USER");
    if (Username == NULL || *Username == 0)
    {
        Username = "?";
    }
    BOOLEAN LoginShell = Argc > 0 && Argv != NULL && Argv[0] != NULL && Argv[0][0] == '-';

    IMEReset(&IME);
    ShellHistoryInit(Envp);
    if (LoginShell)
    {
        CONST char *Clear = "\033[0m\033[2J\033[H";
        write(1, Clear, strlen(Clear));
    }
    SHELL_INPUT_STYLE InputStyle = SHELL_INPUT_EMPTY;
    ShellPrompt(Username);
    ShellApplyInputColor(Line, InputStyle);
    for (;;)
    {
        if (read(0, &Byte, 1) != 1)
        {
            return 1;
        }
        if (Byte == 27)
        {
            EscapeState = 1;
            continue;
        }
        if (EscapeState == 1)
        {
            EscapeState = Byte == '[' ? 2 : 0;
            continue;
        }
        if (EscapeState == 2)
        {
            EscapeState = 0;
            if (Byte == 'A' || Byte == 'B')
            {
                ShellAppend(Line, &Length, IMECodepoint(&IME));
                IMEReset(&IME);
                if (ShellHistoryMove(Byte == 'A', Line, &Length))
                {
                    InputStyle = ShellInputStyle(Line, &IME, Envp);
                    ShellRenderInput(Line, &IME, InputStyle);
                }
            }
            continue;
        }
        UINT8 PreviousColors[260];
        UINTN PreviousLength = strlen(Line);
        ShellInputColors(Line, InputStyle, PreviousColors);
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
            ShellHistoryAdd(Line);
            SHELL_COMMAND Command = ShellGetCommand(Line);
            switch (Command)
            {
                case SHELL_COMMAND_EMPTY:
                {
                    break;
                }
                case SHELL_COMMAND_HELP:
                {
                    puts("help | cd [PATH|-] | pwd | echo TEXT | type NAME... | exit | logout | PATH");
                    break;
                }
                case SHELL_COMMAND_ECHO:
                {
                    puts(Line[4] == 0 ? "" : Line + 5);
                    break;
                }
                case SHELL_COMMAND_CD:
                {
                    ShellCD(Line + (Line[2] == 0 ? 2 : 3), Envp);
                    break;
                }
                case SHELL_COMMAND_PWD:
                {
                    ShellPWD();
                    break;
                }
                case SHELL_COMMAND_TYPE:
                {
                    ShellType(Line + (Line[4] == 0 ? 4 : 5), Envp);
                    break;
                }
                case SHELL_COMMAND_EXIT:
                case SHELL_COMMAND_LOGOUT:
                {
                    if (Command == SHELL_COMMAND_LOGOUT && !LoginShell)
                    {
                        puts("logout: not a login shell");
                        break;
                    }
                    CONST char *Argument = strchr(Line, ' ');
                    while (Argument != NULL && *Argument == ' ')
                    {
                        Argument++;
                    }
                    if (Argument != NULL && *Argument != 0)
                    {
                        puts("exit/logout: arguments are not supported");
                        break;
                    }
                    return 0;
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
            ShellHistoryEdited();
            SHELL_INPUT_STYLE Style = ShellInputStyle(Line, &IME, Envp);
            UINT8 Colors[260];
            ShellInputColors(Line, Style, Colors);
            UINTN CurrentLength = strlen(Line);
            UINTN CommonLength = CurrentLength < PreviousLength ? CurrentLength : PreviousLength;
            if (Style != InputStyle || memcmp(PreviousColors, Colors, CommonLength) != 0)
            {
                InputStyle = Style;
                ShellRenderInput(Line, &IME, InputStyle);
            }
            ShellApplyInputColor(Line, InputStyle);
        }
    }
}
