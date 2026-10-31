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
    char  Line[SHELL_INPUT_CAPACITY];
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
        ShellLocaleRefresh();
        ShellPrompt(Username);

        if (!ShellReadCommand(Line, Envp))
        {
            if (errno == EINTR)
            {
                LastStatus = 130;
                continue;
            }

            if (errno != 0)
            {
                ShellError("read", NULL, errno);

                return 1;
            }

            ShellExitNotice(TRUE, LastStatus);

            return LastStatus;
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
                puts(ShellMessage("help | cd [PATH|-] | pwd | echo TEXT [> FILE | >> FILE] | type NAME... | exit [STATUS] | logout [STATUS] | PATH", "help | cd [경로|-] | pwd | echo 문자열 [> 파일 | >> 파일] | type 이름... | exit [상태] | logout [상태] | 프로그램 경로"));
                LastStatus = 0;
                break;
            }

            case SHELL_COMMAND_ASSIGNMENT:
            {
                LastStatus = ShellAssign(Line, Envp, LastStatus);
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
                    puts(ShellMessage("logout: not a login shell", "logout: 로그인 쉘이 아닙니다"));
                    LastStatus = 1;
                    break;
                }

                INT32 Status;

                if (!ShellExitStatus(Line, LastStatus, &Status))
                {
                    puts(ShellMessage("exit/logout: expected one numeric status", "exit/logout: 숫자 상태 코드 하나를 지정해야 합니다"));
                    LastStatus = 2;
                    break;
                }

                ShellExitNotice(FALSE, Status);

                return Status;
            }

            case SHELL_COMMAND_EXTERNAL:
            {
                pid_t Child = fork();

                if (Child == 0)
                {
                    ShellExecute(Line, Envp, LastStatus);

                    INT32 Error = errno;

                    ShellExecutionError(Line, Envp, LastStatus, Error);

                    return Error == ENOENT ? 127 : 126;
                }

                if (Child < 0)
                {
                    ShellError("fork", NULL, errno);
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
                        ShellError("waitpid", NULL, errno);
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

        if (Command != SHELL_COMMAND_EMPTY && LastStatus != 0)
        {
            ShellExitNotice(FALSE, LastStatus);
        }
    }
}
