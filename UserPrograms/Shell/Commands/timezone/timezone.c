// UserPrograms/Shell/Commands/timezone/timezone.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <locale.h>

#include <LOSLibC/TypeInfo.h>
#include <Syscall/Time/TimeZone.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define TIMEZONE_TEMP_PATH "/etc/.timezone.tmp"

STATIC CONST TIMEZONE_ENTRY *TimezoneFind(CONST char *Name)
{
    for (UINT32 Index = 0; Index < TIMEZONE_COUNT; Index++)
    {
        if (strcmp(Name, TimezoneEntries[Index].Name) == 0 || strcmp(Name, TimezoneEntries[Index].Abbreviation) == 0)
        {
            return &TimezoneEntries[Index];
        }
    }

    return NULL;
}

STATIC BOOLEAN TimezoneRead(CONST TIMEZONE_ENTRY **Entry)
{
    char   Name[TIMEZONE_NAME_CAPACITY];
    size_t Length = 0;
    int    FD = open(TIMEZONE_PATH, O_RDONLY);

    if (FD < 0)
    {
        return FALSE;
    }

    while (Length + 1 < sizeof(Name))
    {
        ssize_t Count = read(FD, Name + Length, sizeof(Name) - Length - 1);

        if (Count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            close(FD);

            return FALSE;
        }

        if (Count == 0)
        {
            break;
        }

        Length += (size_t) Count;
    }

    if (close(FD) != 0 || Length + 1 == sizeof(Name))
    {
        return FALSE;
    }

    Name[Length] = 0;

    while (Length != 0 && (Name[Length - 1] == '\n' || Name[Length - 1] == '\r' || Name[Length - 1] == ' ' || Name[Length - 1] == '\t'))
    {
        Name[--Length] = 0;
    }

    *Entry = NULL;

    for (UINT32 Index = 0; Index < TIMEZONE_COUNT; Index++)
    {
        if (strcmp(Name, TimezoneEntries[Index].Name) == 0)
        {
            *Entry = &TimezoneEntries[Index];

            return TRUE;
        }
    }

    return FALSE;
}

STATIC VOID TimezonePrint(CONST TIMEZONE_ENTRY *Entry)
{
    INT32  Offset = Entry->Offset;
    UINT32 Absolute = Offset < 0 ? (UINT32) -Offset : (UINT32) Offset;

    printf(gettext("Timezone: %s\n"), Entry->Name);
    printf(gettext("Name:     %s\n"), Entry->Abbreviation);
    printf(gettext("Offset:   UTC%c%02u:%02u\n"), Offset < 0 ? '-' : '+', Absolute / TIMEZONE_SECONDS_PER_HOUR, (Absolute % TIMEZONE_SECONDS_PER_HOUR) / TIMEZONE_SECONDS_PER_MINUTE);
    puts(gettext("DST:      Disabled"));
}

STATIC BOOLEAN TimezoneSet(CONST TIMEZONE_ENTRY *Entry)
{
    int FD = open(TIMEZONE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (FD < 0)
    {
        return FALSE;
    }

    size_t Length = strlen(Entry->Name);
    size_t Position = 0;

    while (Position < Length)
    {
        ssize_t Count = write(FD, Entry->Name + Position, Length - Position);

        if (Count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            close(FD);

            return FALSE;
        }

        if (Count == 0)
        {
            close(FD);

            return FALSE;
        }

        Position += (size_t) Count;
    }

    if (write(FD, "\n", 1) != 1)
    {
        close(FD);

        return FALSE;
    }

    return close(FD) == 0;
}

INT32 main(INT32 Argc, char **Argv)
{
    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    CONST TIMEZONE_ENTRY *Entry;

    if (Argc == 1)
    {
        if (!TimezoneRead(&Entry))
        {
            puts(gettext("timezone: cannot read current timezone"));

            return 1;
        }

        TimezonePrint(Entry);

        return 0;
    }

    if (Argc == 2 && strcmp(Argv[1], "-a") == 0)
    {
        for (UINT32 Index = 0; Index < TIMEZONE_COUNT; Index++)
        {
            puts(TimezoneEntries[Index].Name);
        }

        return 0;
    }

    if (Argc == 2 && strcmp(Argv[1], "--help") == 0)
    {
        puts(gettext("Usage:"));
        puts("  timezone");
        puts("  timezone -a");
        puts(gettext("  timezone set <TIMEZONE>"));

        return 0;
    }

    if (Argc == 3 && strcmp(Argv[1], "set") == 0)
    {
        Entry = TimezoneFind(Argv[2]);

        if (Entry == NULL)
        {
            printf(gettext("timezone: unknown timezone '%s'\n"), Argv[2]);

            return 1;
        }

        CONST TIMEZONE_ENTRY *Previous = NULL;

        TimezoneRead(&Previous);

        if (TimezoneSet(Entry) != 0)
        {
            return 1;
        }

        if (Previous != NULL)
        {
            printf(gettext("Timezone changed: %s -> %s\n"), Previous->Abbreviation, Entry->Abbreviation);
        }
        else
        {
            printf(gettext("Timezone set: %s\n"), Entry->Abbreviation);
        }

        return 0;
    }

    puts(gettext("Usage: timezone [-a | set <TIMEZONE> | --help]"));

    return 1;
}
