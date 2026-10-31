// LOSLibC/Source/IO/perror.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <LOSLibC/TypeInfo.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

STATIC VOID ErrorWrite(CONST char *Text)
{
    size_t Length = strlen(Text);

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
        Length -= (size_t) Count;
    }
}

void perror(const char *Prefix)
{
    int Error = errno;

    if (Prefix != NULL && *Prefix != 0)
    {
        ErrorWrite(Prefix);
        ErrorWrite(": ");
    }

    ErrorWrite(strerror(Error));
    ErrorWrite("\n");
    errno = Error;
}
