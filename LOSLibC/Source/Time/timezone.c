// LOSLibC/Source/Time/timezone.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <Syscall/Time/TimeZone.h>

STATIC BOOLEAN TimezoneIsSpace(char Character)
{
    switch (Character)
    {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        {
            return TRUE;
        }

        default:
        {
            return FALSE;
        }
    }
}

STATIC BOOLEAN TimezoneReadName(char Name[TIMEZONE_NAME_CAPACITY])
{
    size_t  Length = 0;
    BOOLEAN Valid = TRUE;
    int     FD = open(TIMEZONE_PATH, O_RDONLY);

    if (FD < 0)
    {
        return FALSE;
    }

    while (Length + 1 < TIMEZONE_NAME_CAPACITY)
    {
        size_t  Available = TIMEZONE_NAME_CAPACITY - Length - 1;
        ssize_t Count = read(FD, Name + Length, Available);

        if (Count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            Valid = FALSE;
            break;
        }

        if (Count == 0)
        {
            break;
        }

        Length += (size_t) Count;
    }

    if (close(FD) != 0)
    {
        Valid = FALSE;
    }

    if (Length + 1 == TIMEZONE_NAME_CAPACITY)
    {
        Valid = FALSE;
    }

    if (!Valid)
    {
        return FALSE;
    }

    for (size_t Index = 0; Index < Length; Index++)
    {
        if (Name[Index] == 0)
        {
            return FALSE;
        }
    }

    while (Length != 0)
    {
        if (!TimezoneIsSpace(Name[Length - 1]))
        {
            break;
        }

        Length--;
    }

    size_t Start = 0;

    while (Start < Length && TimezoneIsSpace(Name[Start]))
    {
        Start++;
    }

    Length -= Start;
    memmove(Name, Name + Start, Length);
    Name[Length] = 0;

    return TRUE;
}

STATIC CONST TIMEZONE_ENTRY *TimezoneFind(CONST char *Name)
{
    for (UINT32 Index = 0; Index < TIMEZONE_COUNT; Index++)
    {
        if (strcmp(Name, TimezoneEntries[Index].Name) == 0)
        {
            return &TimezoneEntries[Index];
        }
    }

    return &TimezoneEntries[TIMEZONE_UTC];
}

STATIC CONST TIMEZONE_ENTRY *TimezoneGet(VOID)
{
    char                  Name[TIMEZONE_NAME_CAPACITY];
    int                   SavedError = errno;
    CONST TIMEZONE_ENTRY *Entry = &TimezoneEntries[TIMEZONE_UTC];

    if (TimezoneReadName(Name))
    {
        Entry = TimezoneFind(Name);
    }

    errno = SavedError;

    return Entry;
}

struct tm *localtime_r(const time_t *Timer, struct tm *Result)
{
    if (Timer == NULL || Result == NULL)
    {
        errno = EINVAL;

        return NULL;
    }

    CONST TIMEZONE_ENTRY *Timezone = TimezoneGet();
    INT64                 Offset = Timezone->Offset;
    INT64                 Timestamp = (INT64) *Timer;

    if ((Offset > 0 && Timestamp > INT64_MAX - Offset) || (Offset < 0 && Timestamp < INT64_MIN - Offset))
    {
        errno = EOVERFLOW;

        return NULL;
    }

    time_t Local = (time_t) (Timestamp + Offset);

    if (gmtime_r(&Local, Result) == NULL)
    {
        return NULL;
    }

    Result->tm_gmtoff = (long) Offset;
    Result->tm_zone = (char *) Timezone->Abbreviation;

    return Result;
}
