// kernel/src/runtime/compiler.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <runtime/compiler.h>
#include <lineos/typeinfo.h>

UINT128 __udivti3(UINT128 dividend, UINT128 divisor)
{
    UINT128 quotient;
    UINT128 bit;

    quotient = 0;
    bit = 1;

    if (divisor == 0)
    {
        return 0;
    }

    while ((divisor << 1) > divisor && (divisor << 1) <= dividend)
    {
        divisor <<= 1;
        bit <<= 1;
    }

    while (bit != 0)
    {
        if (dividend >= divisor)
        {
            dividend -= divisor;
            quotient |= bit;
        }

        divisor >>= 1;
        bit >>= 1;
    }

    return quotient;
}