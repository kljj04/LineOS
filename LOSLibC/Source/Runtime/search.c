// LOSLibC/Source/Runtime/search.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>

void *bsearch(const void *Key, const void *Base, size_t Count, size_t Size, int (*Compare)(const void *, const void *))
{
    const unsigned char *Start = Base;

    while (Count != 0)
    {
        size_t Middle = Count / 2;
        const unsigned char *Item = Start + Middle * Size;
        int Order = Compare(Key, Item);

        if (Order == 0)
        {
            return (void *) Item;
        }

        if (Order < 0)
        {
            Count = Middle;
        }
        else
        {
            Start = Item + Size;
            Count -= Middle + 1;
        }
    }

    return NULL;
}

int abs(int Value)
{
    return Value < 0 ? -Value : Value;
}

long labs(long Value)
{
    return Value < 0 ? -Value : Value;
}

long long llabs(long long Value)
{
    return Value < 0 ? -Value : Value;
}
