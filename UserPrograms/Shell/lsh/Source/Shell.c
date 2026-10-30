// UserPrograms/Shell/lsh/Source/Shell.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <ShellParse.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>

INT32 main(INT32 Argc, char **Argv, char **Envp)
{
    char Line[256];
    INT32 LastStatus = 0;

    CONST char *Username = ShellGetEnvironment(Envp, "USER");
    if (Username == NULL || *Username == 0)
    {
        Username = "?";
    }
    BOOLEAN LoginShell = Argc > 0 && Argv != NULL && Argv[0] != NULL && Argv[0][0] == '-';

    ShellHistoryInit(Envp);
    if (LoginShell)
    {
        CONST char *Clear = "\033[0m\033[2J\033[H";
        write(1, Clear, strlen(Clear));
    }
    for (;;)
    {
        ShellPrompt(Username);
        if (!ShellReadCommand(Line, Envp))
        {
            return 1;
        }
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
                puts("help | cd [PATH|-] | pwd | echo TEXT [> FILE | >> FILE] | type NAME... | exit | logout | PATH");
                LastStatus = 0;
                break;
            }
            case SHELL_COMMAND_ECHO:
            {
                LastStatus = ShellEchoCommand(Line + 4, Envp, LastStatus);
                break;
            }
            case SHELL_COMMAND_CD:
            {
                LastStatus = ShellCD(Line + (Line[2] == 0 ? 2 : 3), Envp);
                break;
            }
            case SHELL_COMMAND_PWD:
            {
                LastStatus = ShellPWD();
                break;
            }
            case SHELL_COMMAND_TYPE:
            {
                LastStatus = ShellType(Line + (Line[4] == 0 ? 4 : 5), Envp);
                break;
            }
            case SHELL_COMMAND_EXIT:
            case SHELL_COMMAND_LOGOUT:
            {
                if (Command == SHELL_COMMAND_LOGOUT && !LoginShell)
                {
                    puts("logout: not a login shell");
                    LastStatus = 1;
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
                    LastStatus = 1;
                    break;
                }
                return 0;
            }
            case SHELL_COMMAND_EXTERNAL:
            {
                pid_t Child = fork();
                if (Child == 0)
                {
                    ShellExecute(Line, Envp, LastStatus);
                    printf("%s: %s\n", Line, errno == ENOENT ? "command not found" : "cannot execute");
                    return 127;
                }
                if (Child < 0)
                {
                    puts("Shell: fork failed");
                    LastStatus = 1;
                }
                else
                {
                    pid_t Result;
                    INT32 Status = 0;
                    do
                    {
                        Result = waitpid(Child, &Status, 0);
                    } while (Result < 0 && errno == EINTR);
                    if (Result < 0)
                    {
                        puts("Shell: waitpid failed");
                        LastStatus = 1;
                    }
                    else
                    {
                        LastStatus = (Status & 127) != 0 ? 128 + (Status & 127) : (Status >> 8) & 255;
                    }
                }
                break;
            }
        }
    }
}
