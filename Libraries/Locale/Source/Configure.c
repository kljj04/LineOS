// Libraries/Locale/Source/Configure.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLocale.h>
#include <locale.h>
#include <Syscall/Time/TimeZone.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

STATIC BOOLEAN LocaleReadSetting(CONST char *Path, char *Buffer, UINTN Capacity)
{
    int FD = open(Path, O_RDONLY);
    if (FD < 0)
    {
        return FALSE;
    }
    UINTN Length = 0;
    while (Length + 1 < Capacity)
    {
        ssize_t Count = read(FD, Buffer + Length, Capacity - Length - 1);
        if (Count < 0 && errno == EINTR)
        {
            continue;
        }
        if (Count <= 0)
        {
            break;
        }
        Length += (UINTN) Count;
    }
    close(FD);
    while (Length != 0 && (Buffer[Length - 1] == '\n' || Buffer[Length - 1] == '\r' || Buffer[Length - 1] == ' ' || Buffer[Length - 1] == '\t'))
    {
        Length--;
    }
    Buffer[Length] = 0;
    return Length != 0;
}

CONST char *LOSLocaleNormalize(CONST char *Name)
{
    STATIC CONST char *Names[] = {"C", "C.UTF-8", "en_US.UTF-8", "ko_KR.UTF-8"};

    if (Name == NULL)
    {
        return NULL;
    }
    if (strcmp(Name, "POSIX") == 0)
    {
        return Names[0];
    }
    for (UINTN Index = 0; Index < sizeof(Names) / sizeof(Names[0]); Index++)
    {
        if (strcmp(Name, Names[Index]) == 0)
        {
            return Names[Index];
        }
    }
    return NULL;
}

char *LOSLocaleRefresh(VOID)
{
    STATIC CONST char *Categories[] = {"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES"};
    int                SavedError   = errno;
    char               Name[64];
    CONST char        *Default = "C";

    if (LocaleReadSetting("/etc/locale", Name, sizeof(Name)))
    {
        CONST char *Normalized = LOSLocaleNormalize(Name);
        if (Normalized != NULL)
        {
            Default = Normalized;
        }
    }
    char  Selection[256];
    UINTN Position = 0;
    for (INT32 Index = 0; Index < LC_ALL; Index++)
    {
        CONST char *Value = getenv("LC_ALL");
        if (Value == NULL || *Value == 0)
        {
            Value = getenv(Categories[Index]);
        }
        if (Value == NULL || *Value == 0)
        {
            Value = getenv("LANG");
        }
        if (Value == NULL || *Value == 0)
        {
            Value = Default;
        }
        Value = LOSLocaleNormalize(Value);
        if (Value == NULL)
        {
            errno = EINVAL;
            return NULL;
        }
        UINTN Length = strlen(Value);
        memcpy(Selection + Position, Value, Length);
        Position += Length;
        Selection[Position++] = Index + 1 == LC_ALL ? 0 : ';';
    }
    char *Result = setlocale(LC_ALL, Selection);
    if (Result != NULL)
    {
        errno = SavedError;
    }
    return Result;
}

STATIC VOID __attribute__((constructor)) LocaleConfigure(VOID)
{
    int  SavedError = errno;
    char Name[TIMEZONE_NAME_CAPACITY];
    LOSLocaleRefresh();
    if (getenv("TZ") == NULL && LocaleReadSetting(TIMEZONE_PATH, Name, sizeof(Name)))
    {
        for (UINT32 Index = 0; Index < TIMEZONE_COUNT; Index++)
        {
            CONST TIMEZONE_ENTRY *Entry = &TimezoneEntries[Index];
            if (strcmp(Name, Entry->Name) != 0)
            {
                continue;
            }
            INT32  Offset    = Entry->Offset;
            UINT32 Magnitude = Offset < 0 ? (UINT32) -Offset : (UINT32) Offset;
            char   Zone[96];
            snprintf(Zone, sizeof(Zone), "<%s>%s%u:%02u:%02u", Entry->Abbreviation, Offset >= 0 ? "-" : "+", Magnitude / 3600, Magnitude / 60 % 60, Magnitude % 60);
            setenv("TZ", Zone, 0);
            tzset();
            break;
        }
    }
    errno = SavedError;
}
