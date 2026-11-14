// LOSLibC/Source/String/string.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>

size_t strlen(const char *String)
{
    size_t Length;

    Length = 0;

    while (String[Length] != '\0')
    {
        Length++;
    }

    return Length;
}

int strcmp(const char *Left, const char *Right)
{
    while (*Left != '\0' && *Left == *Right)
    {
        Left++;
        Right++;
    }

    return (int) (unsigned char) *Left - (int) (unsigned char) *Right;
}

int strncmp(const char *Left, const char *Right, size_t Size)
{
    size_t Index;

    for (Index = 0; Index < Size; Index++)
    {
        if (Left[Index] != Right[Index])
        {
            return (int) (unsigned char) Left[Index] - (int) (unsigned char) Right[Index];
        }

        if (Left[Index] == '\0')
        {
            return 0;
        }
    }

    return 0;
}

char *strcpy(char *Destination, const char *Source)
{
    char *Result;

    Result = Destination;

    while ((*Destination++ = *Source++) != '\0')
    {
    }

    return Result;
}

char *strncpy(char *Destination, const char *Source, size_t Size)
{
    size_t Index;

    Index = 0;

    while (Index < Size && Source[Index] != '\0')
    {
        Destination[Index] = Source[Index];
        Index++;
    }

    while (Index < Size)
    {
        Destination[Index] = '\0';
        Index++;
    }

    return Destination;
}

char *strcat(char *Destination, const char *Source)
{
    char *Result;

    Result = Destination;

    while (*Destination != '\0')
    {
        Destination++;
    }

    while ((*Destination++ = *Source++) != '\0')
    {
    }

    return Result;
}

char *strchr(const char *String, int Character)
{
    unsigned char Target;

    Target = (unsigned char) Character;

    while (1)
    {
        if ((unsigned char) *String == Target)
        {
            return (char *) String;
        }

        if (*String == '\0')
        {
            return NULL;
        }

        String++;
    }
}

char *strrchr(const char *String, int Character)
{
    const char   *Result;
    unsigned char Target;

    Result = NULL;
    Target = (unsigned char) Character;

    do
    {
        if ((unsigned char) *String == Target)
        {
            Result = String;
        }
    } while (*String++ != '\0');

    return (char *) Result;
}

char *strstr(const char *String, const char *Needle)
{
    size_t Length = strlen(Needle);

    if (Length == 0)
    {
        return (char *) String;
    }

    for (; *String != 0; String++)
    {
        if (strncmp(String, Needle, Length) == 0)
        {
            return (char *) String;
        }
    }

    return NULL;
}

size_t strnlen(const char *String, size_t Limit)
{
    size_t Length = 0;

    while (Length < Limit && String[Length] != 0)
    {
        Length++;
    }

    return Length;
}

char *strdup(const char *String)
{
    size_t Size = strlen(String) + 1;
    char  *Copy = malloc(Size);

    if (Copy != NULL)
    {
        memcpy(Copy, String, Size);
    }

    return Copy;
}

char *strtok_r(char *Text, const char *Delimiters, char **Next)
{
    if (Text == NULL)
    {
        Text = *Next;
    }

    if (Text == NULL)
    {
        return NULL;
    }

    while (*Text != 0 && strchr(Delimiters, *Text) != NULL)
    {
        Text++;
    }

    if (*Text == 0)
    {
        *Next = Text;

        return NULL;
    }

    char *Token = Text;

    while (*Text != 0 && strchr(Delimiters, *Text) == NULL)
    {
        Text++;
    }

    if (*Text != 0)
    {
        *Text++ = 0;
    }

    *Next = Text;

    return Token;
}

char *strtok(char *Text, const char *Delimiters)
{
    static char *Next;

    return strtok_r(Text, Delimiters, &Next);
}

int strncasecmp(const char *Left, const char *Right, size_t Size)
{
    for (size_t Index = 0; Index < Size; Index++)
    {
        int A = tolower((unsigned char) Left[Index]);
        int B = tolower((unsigned char) Right[Index]);

        if (A != B || A == 0)
        {
            return A - B;
        }
    }

    return 0;
}

int strcasecmp(const char *Left, const char *Right)
{
    return strncasecmp(Left, Right, (size_t) -1);
}

char *strndup(const char *Text, size_t Limit)
{
    size_t Length = strnlen(Text, Limit);
    char *Copy = malloc(Length + 1);

    if (Copy != NULL)
    {
        memcpy(Copy, Text, Length);
        Copy[Length] = 0;
    }

    return Copy;
}

size_t strcspn(const char *Text, const char *Reject)
{
    size_t Length = 0;

    while (Text[Length] != 0 && strchr(Reject, Text[Length]) == NULL)
    {
        Length++;
    }

    return Length;
}

char *strpbrk(const char *Text, const char *Accept)
{
    Text += strcspn(Text, Accept);

    return *Text == 0 ? NULL : (char *) Text;
}

char *strncat(char *Destination, const char *Source, size_t Limit)
{
    size_t Start = strlen(Destination);
    size_t Length = strnlen(Source, Limit);
    memcpy(Destination + Start, Source, Length);
    Destination[Start + Length] = 0;

    return Destination;
}
