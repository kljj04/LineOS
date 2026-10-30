// UserPrograms/Shell/Commands/sudo/Source/SudoExecute.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Sudo.h>
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
            puts("sudo: cd: too many arguments");
            return 1;
        }
        char Home[6 + USER_DB_NAME_MAX + 1] = "/home/";
        strcat(Home, Username);
        char Expanded[4096];
        CONST char *Path = Argc == 2 ? Argv[1] : Home;
        if (Path[0] == '~' && (Path[1] == 0 || Path[1] == '/'))
        {
            if (strlen(Home) + strlen(Path + 1) >= sizeof(Expanded))
            {
                puts("sudo: cd: path too long");
                return 1;
            }
            strcpy(Expanded, Home);
            strcat(Expanded, Path + 1);
            Path = Expanded;
        }
        INT64 Result = LineOSSyscallResult(LineOSSyscall1(SYSCALL_PARENT_CHDIR, (UINT64) Path));
        if (Result < 0)
        {
            puts("sudo: cd: cannot change directory");
            return 1;
        }
        return 0;
    }
    char Path[4096];
    if (!SudoFind(Argv[0], Path))
    {
        printf("sudo: %s: command not found\n", Argv[0]);
        return 127;
    }
    if (setgid(0) != 0 || setuid(0) != 0)
    {
        puts("sudo: cannot establish root credentials");
        return 1;
    }
    char *Environment[] = {"USER=root", "HOME=/home/root", "PATH=/bin/lsh", NULL};
    execve(Path, Argv, Environment);
    puts("sudo: execve failed");
    return 126;
}
