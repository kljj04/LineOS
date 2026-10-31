// LOSLibC/Source/Locale/locale.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

STATIC CONST char *Names[] = {"C", "C.UTF-8", "en_US.UTF-8", "ko_KR.UTF-8"};
STATIC CONST char *Categories[] = {"LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY", "LC_MESSAGES"};
STATIC UINT32      Current[LC_ALL];
STATIC char        Query[256];

STATIC int LocaleFind(CONST char *Name)
{
    if (strcmp(Name, "POSIX") == 0)
    {
        return 0;
    }

    for (UINT32 Index = 0; Index < sizeof(Names) / sizeof(Names[0]); Index++)
    {
        if (strcmp(Name, Names[Index]) == 0)
        {
            return (int) Index;
        }
    }

    return -1;
}

STATIC VOID LocaleDefault(char *Name)
{
    strcpy(Name, "C");

    int FD = open("/etc/locale", O_RDONLY);

    if (FD < 0)
    {
        return;
    }

    char    Buffer[64];
    size_t  Length = 0;
    BOOLEAN Valid = TRUE;

    while (Length < sizeof(Buffer) - 1)
    {
        ssize_t Count = read(FD, Buffer + Length, sizeof(Buffer) - Length - 1);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count < 0)
        {
            Valid = FALSE;
        }

        if (Count <= 0)
        {
            break;
        }

        Length += (size_t) Count;
    }

    if (close(FD) != 0 || Length == sizeof(Buffer) - 1)
    {
        Valid = FALSE;
    }

    for (size_t Index = 0; Index < Length; Index++)
    {
        if (Buffer[Index] == 0)
        {
            Valid = FALSE;
        }
    }

    while (Length != 0 && (Buffer[Length - 1] == '\n' || Buffer[Length - 1] == '\r' || Buffer[Length - 1] == ' ' || Buffer[Length - 1] == '\t'))
    {
        Length--;
    }

    Buffer[Length] = 0;

    if (Valid && LocaleFind(Buffer) >= 0)
    {
        strcpy(Name, Buffer);
    }
}

STATIC BOOLEAN LocaleComposite(CONST char *Text, UINT32 Values[LC_ALL])
{
    UINT32 Seen = 0;

    while (*Text != 0)
    {
        int Category = -1;

        for (int Index = 0; Index < LC_ALL; Index++)
        {
            size_t Length = strlen(Categories[Index]);

            if (strncmp(Text, Categories[Index], Length) == 0 && Text[Length] == '=')
            {
                Category = Index;
                Text += Length + 1;
                break;
            }
        }

        if (Category < 0 || (Seen & (1U << Category)) != 0)
        {
            return FALSE;
        }

        char  Name[64];
        UINTN Length = 0;

        while (*Text != 0 && *Text != ';')
        {
            if (Length + 1 >= sizeof(Name))
            {
                return FALSE;
            }

            Name[Length++] = *Text++;
        }

        Name[Length] = 0;

        int ID = LocaleFind(Name);

        if (ID < 0)
        {
            return FALSE;
        }

        Values[Category] = (UINT32) ID;
        Seen |= 1U << Category;

        if (*Text == ';')
        {
            Text++;

            if (*Text == 0)
            {
                return FALSE;
            }
        }
    }

    return Seen == (1U << LC_ALL) - 1;
}

char *setlocale(int Category, const char *Locale)
{
    if (Category < 0 || Category > LC_ALL)
    {
        errno = EINVAL;

        return NULL;
    }

    int SavedError = errno;

    if (Locale != NULL)
    {
        UINT32 Replacement[LC_ALL];

        memcpy(Replacement, Current, sizeof(Current));

        char Default[64];

        if (*Locale == 0)
        {
            LocaleDefault(Default);
        }

        BOOLEAN Composite = *Locale != 0 && strchr(Locale, '=') != NULL;

        if (Composite && (Category != LC_ALL || !LocaleComposite(Locale, Replacement)))
        {
            errno = EINVAL;

            return NULL;
        }

        for (int Index = 0; !Composite && Index < LC_ALL; Index++)
        {
            if (Category != LC_ALL && Index != Category)
            {
                continue;
            }

            CONST char *Name = Locale;

            if (*Locale == 0)
            {
                Name = getenv("LC_ALL");

                if (Name == NULL || *Name == 0)
                {
                    Name = getenv(Categories[Index]);
                }

                if (Name == NULL || *Name == 0)
                {
                    Name = getenv("LANG");
                }

                if (Name == NULL || *Name == 0)
                {
                    Name = Default;
                }
            }

            int ID = LocaleFind(Name);

            if (ID < 0)
            {
                errno = EINVAL;

                return NULL;
            }

            Replacement[Index] = (UINT32) ID;
        }

        memcpy(Current, Replacement, sizeof(Current));
    }

    errno = SavedError;

    if (Category != LC_ALL)
    {
        return (char *) Names[Current[Category]];
    }

    BOOLEAN Same = TRUE;

    for (int Index = 1; Index < LC_ALL; Index++)
    {
        Same = Same && Current[Index] == Current[0];
    }

    if (Same)
    {
        return (char *) Names[Current[0]];
    }

    Query[0] = 0;

    for (int Index = 0; Index < LC_ALL; Index++)
    {
        if (Index != 0)
        {
            strcat(Query, ";");
        }

        strcat(Query, Categories[Index]);
        strcat(Query, "=");
        strcat(Query, Names[Current[Index]]);
    }

    return Query;
}

struct lconv *localeconv(void)
{
    STATIC struct lconv Values = {".", "", "", "", "", "", "", "", "", "", CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX};

    return &Values;
}
