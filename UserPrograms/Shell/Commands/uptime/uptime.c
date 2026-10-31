// UserPrograms/Shell/Commands/uptime/uptime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <locale.h>

#include <LOSLibC/TypeInfo.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <time.h>

STATIC VOID UptimePair(UINT32 Value)
{
    printf("%c%c", '0' + Value / 10, '0' + Value % 10);
}

STATIC VOID UptimeClock(CONST struct tm *Date)
{
    UptimePair(Date->tm_hour);
    putchar(':');
    UptimePair(Date->tm_min);
    putchar(':');
    UptimePair(Date->tm_sec);
}

STATIC VOID UptimeDuration(UINT64 Seconds)
{
    CONST UINT64 Units[] = {86400, 3600, 60, 1};
    CONST char  *Names[] = {"day", "hour", "minute", "second"};
    BOOLEAN      Printed = FALSE;

    printf("%s", gettext("up "));

    for (UINTN Index = 0; Index < 4; Index++)
    {
        UINT64 Value = Seconds / Units[Index];

        Seconds %= Units[Index];

        if (Value == 0)
        {
            continue;
        }

        if (Printed)
        {
            printf(", ");
        }

        printf("%llu %s", Value, gettext(Names[Index]));

        if (Value != 1 && strcmp(setlocale(LC_MESSAGES, NULL), "ko_KR.UTF-8") != 0)
        {
            putchar('s');
        }

        Printed = TRUE;
    }

    if (!Printed)
    {
        printf("%s", gettext("0 seconds"));
    }
}

INT32 main(INT32 Argc, char **Argv)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    BOOLEAN Pretty = FALSE;
    BOOLEAN Since = FALSE;

    for (INT32 Index = 1; Index < Argc; Index++)
    {
        if (strcmp(Argv[Index], "-p") == 0 || strcmp(Argv[Index], "--pretty") == 0)
        {
            Pretty = TRUE;
        }
        else if (strcmp(Argv[Index], "-s") == 0 || strcmp(Argv[Index], "--since") == 0)
        {
            Since = TRUE;
        }
        else if (strcmp(Argv[Index], "-h") == 0 || strcmp(Argv[Index], "--help") == 0)
        {
            puts(gettext("Usage: uptime [-p|--pretty] [-s|--since]"));
            puts(gettext("  -p  Show elapsed time only"));
            puts(gettext("  -s  Show boot time in the configured timezone"));

            return 0;
        }
        else
        {
            printf(gettext("uptime: unknown argument: %s\n"), Argv[Index]);

            return 1;
        }
    }

    if (Pretty && Since)
    {
        puts(gettext("uptime: --pretty and --since cannot be combined"));

        return 1;
    }

    SYS_INFO Info;

    if (sysinfo(&Info) != 0 || Info.Uptime < 0)
    {
        puts(gettext("uptime: cannot read system information"));

        return 1;
    }

    if (Pretty)
    {
        UptimeDuration((UINT64) Info.Uptime);
        putchar('\n');

        return 0;
    }

    struct timespec Real;
    struct timespec Monotonic;
    struct tm       Date;

    if (clock_gettime(CLOCK_REALTIME, &Real) != 0)
    {
        puts(gettext("uptime: cannot read current time"));

        return 1;
    }

    time_t Display = Real.tv_sec;

    if (Since)
    {
        if (clock_gettime(CLOCK_MONOTONIC, &Monotonic) != 0)
        {
            puts(gettext("uptime: cannot read monotonic time"));

            return 1;
        }

        Display -= Monotonic.tv_sec;

        if (Real.tv_nsec < Monotonic.tv_nsec)
        {
            Display--;
        }
    }

    if (localtime_r(&Display, &Date) == NULL)
    {
        puts(gettext("uptime: cannot convert time"));

        return 1;
    }

    if (Since)
    {
        printf("%d-", Date.tm_year + 1900);
        UptimePair(Date.tm_mon + 1);
        putchar('-');
        UptimePair(Date.tm_mday);
        putchar(' ');
        UptimeClock(&Date);
        printf(" %s\n", Date.tm_zone);

        return 0;
    }

    UptimeClock(&Date);
    printf(" %s  ", Date.tm_zone);
    UptimeDuration((UINT64) Info.Uptime);
    putchar('\n');

    return 0;
}
