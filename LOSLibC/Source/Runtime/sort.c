// LOSLibC/Source/Runtime/sort.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>

static void SwapItems(unsigned char *Left, unsigned char *Right, size_t Size)
{
    for (size_t Index = 0; Index < Size; Index++)
    {
        unsigned char Saved = Left[Index];
        Left[Index] = Right[Index];
        Right[Index] = Saved;
    }
}

static void SiftDown(unsigned char *Base, size_t Count, size_t Size, size_t Root, int (*Compare)(const void *, const void *))
{
    while (Root < Count / 2)
    {
        size_t Child = Root * 2 + 1;

        if (Child + 1 < Count && Compare(Base + Child * Size, Base + (Child + 1) * Size) < 0)
        {
            Child++;
        }

        if (Compare(Base + Root * Size, Base + Child * Size) >= 0)
        {
            return;
        }

        SwapItems(Base + Root * Size, Base + Child * Size, Size);
        Root = Child;
    }
}

void qsort(void *Base, size_t Count, size_t Size, int (*Compare)(const void *, const void *))
{
    if (Count < 2 || Size == 0 || Count > (size_t) -1 / Size)
    {
        return;
    }

    unsigned char *Bytes = Base;

    for (size_t Root = Count / 2; Root != 0; Root--)
    {
        SiftDown(Bytes, Count, Size, Root - 1, Compare);
    }

    for (size_t Remaining = Count - 1; Remaining != 0; Remaining--)
    {
        SwapItems(Bytes, Bytes + Remaining * Size, Size);
        SiftDown(Bytes, Remaining, Size, 0, Compare);
    }
}
