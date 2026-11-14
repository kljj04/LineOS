// LOSLibC/Source/Runtime/assert.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <assert.h>
#include <LOSLibC/TypeInfo.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

STATIC VOID AssertWrite(CONST char *Text)
{
    write(2, Text, strlen(Text));
}

VOID LOSAssertFail(CONST char *Expression, CONST char *File, unsigned int Line, CONST char *Function)
{
    char  Digits[sizeof(Line) * 3 + 1];
    char *Position = Digits + sizeof(Digits);

    *--Position = 0;

    do
    {
        *--Position = '0' + Line % 10;
        Line /= 10;
    } while (Line != 0);

    AssertWrite(File);
    AssertWrite(":");
    AssertWrite(Position);
    AssertWrite(": ");
    AssertWrite(Function);
    AssertWrite(": assertion failed: ");
    AssertWrite(Expression);
    AssertWrite("\n");
    exit(134);
}
