// UserPrograms/Shell/Commands/touch/touch.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <locale.h>

#include <LOSLibC/TypeInfo.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

STATIC VOID TouchError(CONST char *Path)
{
    INT32 Error = errno;
    char  Prefix[4104] = "touch: ";

    if (strlen(Path) < 4096)
    {
        strcat(Prefix, Path);
    }

    errno = Error;
    perror(Prefix);
}

STATIC INT32 TouchFile(CONST char *Path, BOOLEAN NoCreate)
{
    if (utimensat(AT_FDCWD, Path, NULL, 0) == 0)
    {
        return 0;
    }

    if (errno != ENOENT)
    {
        TouchError(Path);

        return 1;
    }

    if (NoCreate)
    {
        return 0;
    }

    INT32 FD = open(Path, O_WRONLY | O_CREAT, 0666);

    if (FD < 0)
    {
        TouchError(Path);

        return 1;
    }

    if (close(FD) < 0)
    {
        TouchError(Path);

        return 1;
    }

    if (utimensat(AT_FDCWD, Path, NULL, 0) < 0)
    {
        TouchError(Path);

        return 1;
    }

    return 0;
}

INT32 main(INT32 Argc, char **Argv)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    BOOLEAN NoCreate = FALSE;
    INT32   First = 1;

    for (; First < Argc; First++)
    {
        CONST char *Argument = Argv[First];

        if (strcmp(Argument, "--") == 0)
        {
            First++;
            break;
        }

        if (strcmp(Argument, "-c") == 0 || strcmp(Argument, "--no-create") == 0)
        {
            NoCreate = TRUE;
        }
        else if (strcmp(Argument, "--help") == 0)
        {
            puts(gettext("Usage: touch [-c|--no-create] [--] FILE..."));

            return 0;
        }
        else if (Argument[0] == '-' && Argument[1] != 0)
        {
            printf(gettext("touch: unknown option: %s\n"), Argument);

            return 1;
        }
        else
        {
            break;
        }
    }

    if (First == Argc)
    {
        puts(gettext("touch: missing file operand"));

        return 1;
    }

    INT32 Status = 0;

    for (INT32 Index = First; Index < Argc; Index++)
    {
        if (TouchFile(Argv[Index], NoCreate) != 0)
        {
            Status = 1;
        }
    }

    return Status;
}
