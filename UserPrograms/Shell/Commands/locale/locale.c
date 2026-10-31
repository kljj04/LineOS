// UserPrograms/Shell/Commands/locale/locale.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <LOSLibC/TypeInfo.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

STATIC BOOLEAN LocaleWrite(CONST char *Name)
{
    char   Path[64] = "/etc/.locale.";
    char   Digits[24];
    UINT32 Count = 0;
    UINT64 PID = (UINT64) getpid();

    do
    {
        Digits[Count++] = '0' + PID % 10;
        PID /= 10;
    } while (PID != 0);

    UINTN Position = strlen(Path);

    while (Count != 0)
    {
        Path[Position++] = Digits[--Count];
    }

    Path[Position] = 0;

    int FD = open(Path, O_WRONLY | O_CREAT | O_EXCL, 0644);

    if (FD < 0)
    {
        return FALSE;
    }

    char Text[64];

    strcpy(Text, Name);
    strcat(Text, "\n");

    UINTN Length = strlen(Text);
    UINTN Written = 0;
    int   Error = 0;

    while (Written < Length)
    {
        ssize_t Result = write(FD, Text + Written, Length - Written);

        if (Result < 0 && errno == EINTR)
        {
            continue;
        }

        if (Result <= 0)
        {
            Error = Result < 0 ? errno : EIO;
            break;
        }

        Written += (UINTN) Result;
    }

    if (close(FD) < 0 && Error == 0)
    {
        Error = errno;
    }

    if (Error == 0 && rename(Path, "/etc/locale") < 0)
    {
        Error = errno;
    }

    if (Error != 0)
    {
        remove(Path);
        errno = Error;

        return FALSE;
    }

    return TRUE;
}

INT32 main(INT32 Argc, char **Argv)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    if (Argc == 1)
    {
        if (setlocale(LC_ALL, "") == NULL)
        {
            puts(gettext("locale: invalid locale environment"));

            return 1;
        }

        puts(setlocale(LC_ALL, NULL));

        return 0;
    }

    if (Argc == 2 && strcmp(Argv[1], "-a") == 0)
    {
        puts("C\nC.UTF-8\nen_US.UTF-8\nko_KR.UTF-8");

        return 0;
    }

    if (Argc == 3 && strcmp(Argv[1], "set") == 0)
    {
        char *Name = setlocale(LC_ALL, Argv[2]);

        if (Name == NULL || Argv[2][0] == 0 || strchr(Argv[2], '=') != NULL)
        {
            printf(gettext("locale: unsupported locale '%s'\n"), Argv[2]);

            return 1;
        }

        if (geteuid() != 0)
        {
            printf("locale: %s\n", strerror(EACCES));

            return 1;
        }

        if (!LocaleWrite(Name))
        {
            int Error = errno;

            printf("locale: /etc/locale: %s\n", strerror(Error));

            return 1;
        }

        printf(gettext("Locale set: %s\n"), Name);

        return 0;
    }

    puts(gettext("Usage: locale [-a | set <LOCALE> | --help]"));

    return Argc == 2 && strcmp(Argv[1], "--help") == 0 ? 0 : 1;
}
