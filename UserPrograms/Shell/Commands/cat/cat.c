// UserPrograms/Shell/Commands/cat/cat.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAT_BUFFER_SIZE 16384

STATIC INT32 CatWrite(CONST UINT8 *Buffer, UINTN Length)
{
    UINTN Position = 0;

    while (Position < Length)
    {
        ssize_t Written = write(1, Buffer + Position, Length - Position);

        if (Written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            puts("cat: write failed");
            return 1;
        }

        if (Written == 0)
        {
            puts("cat: write returned zero");
            return 1;
        }

        Position += (UINTN) Written;
    }

    return 0;
}

STATIC INT32 CatRead(INT32 FD, CONST CHAR8 *Name)
{
    UINT8 Buffer[CAT_BUFFER_SIZE];

    for (;;)
    {
        ssize_t Count = read(FD, Buffer, sizeof(Buffer));

        if (Count == 0)
        {
            return 0;
        }

        if (Count < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            printf("cat: cannot read '%s'\n", Name);
            return 1;
        }

        if (CatWrite(Buffer, (UINTN) Count) != 0)
        {
            return 1;
        }
    }
}

STATIC INT32 CatFile(CONST CHAR8 *Path)
{
    if (strcmp(Path, "-") == 0)
    {
        return CatRead(0, "stdin");
    }

    INT32 FD = open(Path, O_RDONLY);

    if (FD < 0)
    {
        printf("cat: cannot open '%s'\n", Path);
        return 1;
    }

    INT32 Result = CatRead(FD, Path);

    if (close(FD) < 0)
    {
        printf("cat: cannot close '%s'\n", Path);
        Result = 1;
    }

    return Result;
}

INT32 main(INT32 Argc, CHAR8 **Argv)
{
    INT32 Status = 0;

    if (Argc <= 1)
    {
        puts("cat: missing file operand");
        return 1;
    }

    for (INT32 Index = 1; Index < Argc; Index++)
    {
        CONST CHAR8 *Argument = Argv[Index];

        if (strcmp(Argument, "--help") == 0)
        {
            puts("Usage: cat [FILE ...]");
            puts("  FILE  Read and print a file");
            puts("  -     Read standard input");
            return 0;
        }

        if (CatFile(Argument) != 0)
        {
            Status = 1;
        }
    }

    return Status;
}