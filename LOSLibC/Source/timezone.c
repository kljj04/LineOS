// LOSLibC/Source/timezone.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define TIMEZONE_PATH "/etc/timezone"
#define TIMEZONE_NAME_CAPACITY 64U
#define TIMEZONE_SEOUL_OFFSET (9L * 60 * 60)

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
    size_t Length = 0;
    BOOLEAN Valid = TRUE;
    int FD = open(TIMEZONE_PATH, O_RDONLY);

    if (FD < 0)
    {
        return FALSE;
    }

    while (Length + 1 < TIMEZONE_NAME_CAPACITY)
    {
        size_t Available = TIMEZONE_NAME_CAPACITY - Length - 1;
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

STATIC long TimezoneOffset(VOID)
{
    char Name[TIMEZONE_NAME_CAPACITY];
    long Offset = 0;
    int SavedError = errno;

    if (TimezoneReadName(Name))
    {
        if (strcmp(Name, "Asia/Seoul") == 0)
        {
            Offset = TIMEZONE_SEOUL_OFFSET;
        }
    }

    errno = SavedError;

    return Offset;
}

struct tm *localtime_r(const time_t *Timer, struct tm *Result)
{
    if (Timer == NULL || Result == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    long Offset = TimezoneOffset();

    if (*Timer > 0x7FFFFFFFFFFFFFFFL - Offset)
    {
        errno = EOVERFLOW;
        return NULL;
    }

    time_t Local = *Timer + Offset;

    if (gmtime_r(&Local, Result) == NULL)
    {
        return NULL;
    }

    Result->tm_gmtoff = Offset;
    Result->tm_zone = "UTC";

    if (Offset != 0)
    {
        Result->tm_zone = "KST";
    }

    return Result;
}
