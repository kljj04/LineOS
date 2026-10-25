// UserPrograms/init/Init.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include "stdlib.h"


#include <stdio.h>

int _Start(void)
{
    printf("Hello, LineOS!\n");
    printf("Number: %d\n", 12345);
    printf("Hex: 0x%X\n", 0xDEADBEEF);
    printf("Negative: %d\n", -42);
    printf("Pointer: %p\n", (void *) _Start);
    printf("Korean: 안녕하세요!\n");

    puts("LOSLibC stdio test passed!");

    exit(0);
}