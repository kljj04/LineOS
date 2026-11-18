// LOSLibC/Source/String/memory.c
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
