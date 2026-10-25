// LOSLibC/Source/string.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <string.h>
#include <stdint.h>

void *memset(void *Destination, int Value, size_t Size)
{
    unsigned char *Bytes;
    unsigned char  Byte;
    size_t         Index;

    Bytes = (unsigned char *) Destination;
    Byte = (unsigned char) Value;

    for (Index = 0; Index < Size; Index++)
    {
        Bytes[Index] = Byte;
    }

    return Destination;
}

void *memcpy(void *Destination, const void *Source, size_t Size)
{
    unsigned char       *DestinationBytes;
    const unsigned char *SourceBytes;
    size_t               Index;

    DestinationBytes = (unsigned char *) Destination;
    SourceBytes = (const unsigned char *) Source;

    for (Index = 0; Index < Size; Index++)
    {
        DestinationBytes[Index] = SourceBytes[Index];
    }

    return Destination;
}

void *memmove(void *Destination, const void *Source, size_t Size)
{
    unsigned char       *DestinationBytes;
    const unsigned char *SourceBytes;
    size_t               Index;

    DestinationBytes = (unsigned char *) Destination;
    SourceBytes = (const unsigned char *) Source;

    if (Destination == Source || Size == 0)
    {
        return Destination;
    }

    if ((uintptr_t) Destination < (uintptr_t) Source)
    {
        for (Index = 0; Index < Size; Index++)
        {
            DestinationBytes[Index] = SourceBytes[Index];
        }
    }
    else
    {
        for (Index = Size; Index > 0; Index--)
        {
            DestinationBytes[Index - 1] = SourceBytes[Index - 1];
        }
    }

    return Destination;
}

int memcmp(const void *Left, const void *Right, size_t Size)
{
    const unsigned char *LeftBytes;
    const unsigned char *RightBytes;
    size_t               Index;

    LeftBytes = (const unsigned char *) Left;
    RightBytes = (const unsigned char *) Right;

    for (Index = 0; Index < Size; Index++)
    {
        if (LeftBytes[Index] != RightBytes[Index])
        {
            return (int) LeftBytes[Index] - (int) RightBytes[Index];
        }
    }

    return 0;
}

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