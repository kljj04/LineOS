// Scripts/Validation/DynamicLoader/Math.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

static int IncrementImpl(int Value)
{
    return Value + 1;
}

static void *IncrementResolver(void)
{
    return IncrementImpl;
}

int Increment(int Value) __attribute__((ifunc("IncrementResolver")));
