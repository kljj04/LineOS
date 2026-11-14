// LOSLibC/Source/Runtime/random.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <stdlib.h>
#include <stdint.h>

static uint32_t RandomState = 1;

void srand(unsigned int Seed)
{
    RandomState = Seed;
}

int rand(void)
{
    RandomState = RandomState * UINT32_C(1103515245) + UINT32_C(12345);

    return (int) (RandomState >> 1);
}
