// UserPrograms/Shell/Commands/cat/cat.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <libintl.h>

#include <LOSLibC/TypeInfo.h>
#include <errno.h>
#include <locale.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CAT_BUFFER_SIZE 16384

STATIC VOID CatErrorText(CONST char *Text)
{
    UINTN Length = strlen(Text);

    while (Length != 0)
    {
        ssize_t Count = write(2, Text, Length);

        if (Count < 0 && errno == EINTR)
        {
            continue;
        }

        if (Count <= 0)
        {
            break;
        }

        Text += Count;
        Length -= (UINTN) Count;
    }
}

STATIC VOID CatError(CONST char *Name)
{
    INT32 Error = errno;

    CatErrorText("cat: ");
    CatErrorText(Name);
    CatErrorText(": ");
    CatErrorText(strerror(Error));
    CatErrorText("\n");
    errno = Error;
}

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

            CatError("stdout");

            return 1;
        }

        if (Written == 0)
        {
            errno = EIO;
            CatError("stdout");

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

            CatError((CONST char *) Name);

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
        CatError((CONST char *) Path);

        return 1;
    }

    INT32 Result = CatRead(FD, Path);

    if (close(FD) < 0)
    {
        CatError((CONST char *) Path);
        Result = 1;
    }

    return Result;
}

INT32 main(INT32 Argc, CHAR8 **Argv)
{
    INT32 Status = 0;

    if (setlocale(LC_ALL, "") == NULL)
    {
        setlocale(LC_ALL, "C");
    }

    if (Argc <= 1)
    {
        puts(gettext("cat: missing file operand"));

        return 1;
    }

    for (INT32 Index = 1; Index < Argc; Index++)
    {
        CONST CHAR8 *Argument = Argv[Index];

        if (strcmp(Argument, "--help") == 0)
        {
            puts(gettext("Usage: cat [FILE ...]"));
            puts(gettext("  FILE  Read and print a file"));
            puts(gettext("  -     Read standard input"));

            return 0;
        }

        if (CatFile(Argument) != 0)
        {
            Status = 1;
        }
    }

    return Status;
}