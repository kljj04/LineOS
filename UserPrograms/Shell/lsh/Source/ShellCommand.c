// UserPrograms/Shell/lsh/Source/ShellCommand.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Shell.h>
#include <ShellParse.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

SHELL_COMMAND ShellGetCommand(CONST char *Line)
{
    if (*Line == 0)
    {
        return SHELL_COMMAND_EMPTY;
    }

    if (ShellAssignmentLength(Line) != 0)
    {
        return SHELL_COMMAND_ASSIGNMENT;
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

    if (strcmp(Line, "type") == 0 || strncmp(Line, "type ", 5) == 0)
    {
        return SHELL_COMMAND_TYPE;
    }

    if (strcmp(Line, "exit") == 0 || strncmp(Line, "exit ", 5) == 0)
    {
        return SHELL_COMMAND_EXIT;
    }

    if (strcmp(Line, "logout") == 0 || strncmp(Line, "logout ", 7) == 0)
    {
        return SHELL_COMMAND_LOGOUT;
    }

    return SHELL_COMMAND_EXTERNAL;
}

STATIC char PreviousDirectory[4096];

INT32 ShellCD(CONST char *Path, char **Envp)
{
    while (*Path == ' ')
    {
        Path++;
    }

    char Expanded[4096];

    if (*Path == 0 || (Path[0] == '~' && (Path[1] == 0 || Path[1] == '/')))
    {
        CONST char *Home = ShellGetEnvironment(Envp, "HOME");

        if (Home == NULL || *Home == 0)
        {
            puts(ShellMessage("cd: HOME is not set", "cd: HOME이 설정되지 않았습니다"));

            return 1;
        }

        CONST char *Suffix = *Path == 0 ? "" : Path + 1;
        UINTN       HomeLength = strlen(Home);
        UINTN       SuffixLength = strlen(Suffix);

        if (HomeLength + SuffixLength >= sizeof(Expanded))
        {
            ShellError("cd", NULL, ENAMETOOLONG);

            return 1;
        }

        memcpy(Expanded, Home, HomeLength);
        memcpy(Expanded + HomeLength, Suffix, SuffixLength + 1);
        Path = Expanded;
    }

    BOOLEAN Previous = strcmp(Path, "-") == 0;

    if (Previous && PreviousDirectory[0] == 0)
    {
        puts(ShellMessage("cd: previous directory is not set", "cd: 이전 디렉터리가 설정되지 않았습니다"));

        return 1;
    }

    char Current[4096];

    if (getcwd(Current, sizeof(Current)) == NULL)
    {
        ShellError("cd", NULL, errno);

        return 1;
    }

    if (chdir(Previous ? PreviousDirectory : *Path == 0 ? "/" : Path) < 0)
    {
        ShellError("cd", Previous ? PreviousDirectory : Path, errno);

        return 1;
    }

    memcpy(PreviousDirectory, Current, strlen(Current) + 1);

    if (Previous)
    {
        ShellPWD();
    }

    return 0;
}

INT32 ShellPWD(VOID)
{
    char Path[4096];

    if (getcwd(Path, sizeof(Path)) == NULL)
    {
        ShellError("pwd", NULL, errno);

        return 1;
    }

    puts(Path);

    return 0;
}

CONST char *ShellGetEnvironment(char **Envp, CONST char *Name)
{
    CONST char *Value = ShellGetVariable(Name);

    if (Value != NULL)
    {
        return Value;
    }

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

BOOLEAN ShellFindCommand(CONST char *Command, char **Envp, char Path[4096])
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

VOID ShellExecute(char *Line, char **Envp, INT32 Status)
{
    SHELL_PARSE Parse;

    if (!ShellParseLine(Line, Envp, Status, &Parse))
    {
        errno = EINVAL;

        return;
    }

    if (Parse.Count > 64)
    {
        errno = E2BIG;

        return;
    }

    char *Arguments[65];

    for (UINTN Index = 0; Index < Parse.Count; Index++)
    {
        if (Parse.Tokens[Index].Type != SHELL_TOKEN_WORD)
        {
            errno = EOPNOTSUPP;

            return;
        }

        Arguments[Index] = (char *) Parse.Tokens[Index].Text;
    }

    Arguments[Parse.Count] = NULL;

    char Path[4096];

    if (Parse.Count != 0 && ShellFindCommand(Arguments[0], Envp, Path))
    {
        execve(Path, Arguments, Envp);
    }
}

INT32 ShellType(CONST char *Arguments, char **Envp)
{
    INT32 Status = 0;

    while (*Arguments != 0)
    {
        while (*Arguments == ' ')
        {
            Arguments++;
        }

        if (*Arguments == 0)
        {
            break;
        }

        char  Command[256];
        UINTN Length = 0;

        while (*Arguments != 0 && *Arguments != ' ')
        {
            if (Length + 1 < sizeof(Command))
            {
                Command[Length++] = *Arguments;
            }

            Arguments++;
        }

        Command[Length] = 0;

        switch (ShellGetCommand(Command))
        {
            case SHELL_COMMAND_HELP:
            case SHELL_COMMAND_ECHO:
            case SHELL_COMMAND_CD:
            case SHELL_COMMAND_PWD:
            case SHELL_COMMAND_TYPE:
            case SHELL_COMMAND_EXIT:
            case SHELL_COMMAND_LOGOUT:
            {
                printf(ShellMessage("%s is a shell builtin\n", "%s: 쉘 빌틴 명령어\n"), Command);
                break;
            }

            default:
            {
                char Path[4096];

                if (ShellFindCommand(Command, Envp, Path))
                {
                    printf(ShellMessage("%s is %s\n", "%s: %s\n"), Command, Path);
                }
                else
                {
                    ShellError("type", Command, errno);
                    Status = 1;
                }

                break;
            }
        }
    }

    return Status;
}

VOID ShellExecutionError(CONST char *Line, char **Envp, INT32 Status, INT32 Error)
{
    SHELL_PARSE Parse;
    CONST char *Name = Line;

    if (ShellParseLine(Line, Envp, Status, &Parse) && Parse.Count != 0 && Parse.Tokens[0].Type == SHELL_TOKEN_WORD)
    {
        Name = Parse.Tokens[0].Text;
    }

    if (Error == ENOENT && strchr(Name, '/') == NULL)
    {
        ShellCommandNotFound(Name);
    }
    else
    {
        ShellError(Name, NULL, Error);
    }
}
