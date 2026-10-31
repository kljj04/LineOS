// UserPrograms/Shell/Commands/date/date.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <LOSLibC/TypeInfo.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <errno.h>

INT32 main(INT32 Argc, char **Argv)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    struct timespec Now;
    struct tm       Date;
    char            Buffer[128];

    (VOID) Argv;

    if (Argc != 1)
    {
        puts(gettext("Usage: date"));

        return 1;
    }

    if (clock_gettime(CLOCK_REALTIME, &Now) != 0 || localtime_r(&Now.tv_sec, &Date) == NULL)
    {
        int Error = errno;

        printf("date: %s\n", strerror(Error));

        return 1;
    }

    if (strftime(Buffer, sizeof(Buffer), "%c %Z", &Date) == 0)
    {
        puts(gettext("date: cannot format current time"));

        return 1;
    }

    puts(Buffer);

    return 0;
}
