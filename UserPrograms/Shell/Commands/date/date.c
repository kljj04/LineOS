// UserPrograms/Shell/Commands/date/date.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdio.h>
#include <time.h>

STATIC VOID DatePair(INT32 Value)
{
    putchar('0' + Value / 10);
    putchar('0' + Value % 10);
}

INT32 main(INT32 Argc, char **Argv)
{
    STATIC CONST char *Weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct timespec Now;
    struct tm Date;

    (VOID) Argv;

    if (Argc != 1)
    {
        puts("Usage: date");
        return 1;
    }

    if (clock_gettime(CLOCK_REALTIME, &Now) != 0)
    {
        puts("date: cannot read current time");
        return 1;
    }

    if (localtime_r(&Now.tv_sec, &Date) == NULL)
    {
        puts("date: cannot convert date");
        return 1;
    }

    printf("%d. ", Date.tm_year + 1900);
    DatePair(Date.tm_mon + 1);
    printf(". ");
    DatePair(Date.tm_mday);
    printf(". (%s) ", Weekdays[Date.tm_wday]);

    DatePair(Date.tm_hour);
    putchar(':');
    DatePair(Date.tm_min);
    putchar(':');
    DatePair(Date.tm_sec);
    printf(" %s\n", Date.tm_zone);

    return 0;
}
