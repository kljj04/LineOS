// LOSLibC/Include/stdio.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once


#define STDIO_BUFFER_SIZE 512
#include <stddef.h>

typedef struct
{
    char Buffer[STDIO_BUFFER_SIZE];
    size_t Length;
    size_t Total;
    int Failed;
} STDIO_OUTPUT;

#define EOF (-1)

int printf(const char *Format, ...);
int puts(const char *String);
int putchar(int Character);