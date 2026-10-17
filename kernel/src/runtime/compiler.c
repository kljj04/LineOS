// kernel/src/runtime/compiler.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <runtime/compiler.h>
#include <LineOS/TypeInfo.h>

UINT128 __udivti3(UINT128 Dividend, UINT128 Divisor)
{
    UINT128 Quotient;
    UINT128 Bit;

    Quotient = 0;
    Bit = 1;

    if (Divisor == 0)
    {
        return 0;
    }

    while ((Divisor << 1) > Divisor && (Divisor << 1) <= Dividend)
    {
        Divisor <<= 1;
        Bit <<= 1;
    }

    while (Bit != 0)
    {
        if (Dividend >= Divisor)
        {
            Dividend -= Divisor;
            Quotient |= Bit;
        }

        Divisor >>= 1;
        Bit >>= 1;
    }

    return Quotient;
}