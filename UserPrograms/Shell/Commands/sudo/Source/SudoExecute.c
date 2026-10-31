// UserPrograms/Shell/Commands/sudo/Source/SudoExecute.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <Sudo.h>
#include <locale.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <syscall.h>

STATIC BOOLEAN SudoExecutable(CONST char *Path)
{
    struct stat Info;

    return stat(Path, &Info) == 0 && S_ISREG(Info.st_mode) && (Info.st_mode & 0111) != 0;
}

STATIC BOOLEAN SudoFind(CONST char *Command, char Path[4096])
{
    UINTN Length = strlen(Command);

    if (Length == 0 || Length > 4080)
    {
        return FALSE;
    }

    if (strchr(Command, '/') != NULL)
    {
        strcpy(Path, Command);

        return SudoExecutable(Path);
    }

    strcpy(Path, "/bin/lsh/");
    strcpy(Path + 9, Command);

    if (SudoExecutable(Path))
    {
        return TRUE;
    }

    if (Length >= 4 && strcmp(Command + Length - 4, ".lix") == 0)
    {
        return FALSE;
    }

    strcat(Path, ".lix");

    return SudoExecutable(Path);
}

INT32 SudoExecute(INT32 Argc, char **Argv, CONST char *Username)
{
    if (strcmp(Argv[0], "cd") == 0)
    {
        if (Argc > 2)
        {
            puts(gettext("sudo: cd: too many arguments"));

            return 1;
        }

        char Home[6 + USER_DB_NAME_MAX + 1] = "/home/";

        strcat(Home, Username);

        char        Expanded[4096];
        CONST char *Path = Argc == 2 ? Argv[1] : Home;

        if (Path[0] == '~' && (Path[1] == 0 || Path[1] == '/'))
        {
            if (strlen(Home) + strlen(Path + 1) >= sizeof(Expanded))
            {
                puts(gettext("sudo: cd: path too long"));

                return 1;
            }

            strcpy(Expanded, Home);
            strcat(Expanded, Path + 1);
            Path = Expanded;
        }

        INT64 Result = LineOSSyscallResult(LineOSSyscall1(SYSCALL_PARENT_CHDIR, (UINT64) Path));

        if (Result < 0)
        {
            perror("sudo: cd");

            return 1;
        }

        return 0;
    }

    char Path[4096];

    if (!SudoFind(Argv[0], Path))
    {
        printf(gettext("sudo: %s: command not found\n"), Argv[0]);

        return 127;
    }

    if (setgid(0) != 0 || setuid(0) != 0)
    {
        perror("sudo: setuid/setgid");

        return 1;
    }

    STATIC CONST char *Categories[] = {"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES"};
    char               LocaleEnvironment[LC_ALL][64];
    char              *Environment[3 + LC_ALL + 1] = {"USER=root", "HOME=/home/root", "PATH=/bin/lsh"};

    for (INT32 Index = 0; Index < LC_ALL; Index++)
    {
        strcpy(LocaleEnvironment[Index], Categories[Index]);
        strcat(LocaleEnvironment[Index], "=");
        strcat(LocaleEnvironment[Index], setlocale(Index, NULL));
        Environment[3 + Index] = LocaleEnvironment[Index];
    }

    Environment[3 + LC_ALL] = NULL;
    execve(Path, Argv, Environment);
    perror("sudo: execve");

    return 126;
}
