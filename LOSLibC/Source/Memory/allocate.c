// LOSLibC/Source/Memory/allocate.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

typedef union
{
    max_align_t Align;
    size_t      Size;
} ALLOCATION_HEADER;

void *malloc(size_t Size)
{
    if (Size > (size_t) -1 - sizeof(ALLOCATION_HEADER))
    {
        errno = ENOMEM;

        return NULL;
    }

    if (Size == 0)
    {
        Size = 1;
    }

    ALLOCATION_HEADER *Header = mmap(NULL, sizeof(*Header) + Size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (Header == MAP_FAILED)
    {
        return NULL;
    }

    Header->Size = Size;

    return Header + 1;
}

void *calloc(size_t Count, size_t Size)
{
    if (Size != 0 && Count > (size_t) -1 / Size)
    {
        errno = ENOMEM;

        return NULL;
    }

    void *Pointer = malloc(Count * Size);

    if (Pointer != NULL)
    {
        memset(Pointer, 0, Count * Size);
    }

    return Pointer;
}

void free(void *Pointer)
{
    if (Pointer == NULL)
    {
        return;
    }

    int                Error = errno;
    ALLOCATION_HEADER *Header = (ALLOCATION_HEADER *) Pointer - 1;
    munmap(Header, sizeof(*Header) + Header->Size);
    errno = Error;
}

void *realloc(void *Pointer, size_t Size)
{
    if (Pointer == NULL)
    {
        return malloc(Size);
    }

    if (Size == 0)
    {
        free(Pointer);

        return NULL;
    }

    ALLOCATION_HEADER *Header = (ALLOCATION_HEADER *) Pointer - 1;

    if (Size <= Header->Size)
    {
        return Pointer;
    }

    void *Replacement = malloc(Size);

    if (Replacement != NULL)
    {
        memcpy(Replacement, Pointer, Header->Size);
        free(Pointer);
    }

    return Replacement;
}
