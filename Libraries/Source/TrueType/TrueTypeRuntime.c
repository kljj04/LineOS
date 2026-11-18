// Libraries/Source/TrueType/TrueTypeRuntime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <TrueType/TrueTypeRuntime.h>
#include <stdlib.h>

#define PI      3.14159265358979323846
#define TWO_PI  6.28318530717958647692
#define HALF_PI 1.57079632679489661923

INT32 TTFFloor(FLOAT64 Value)
{
    INT32 Integer = (INT32) Value;

    if ((FLOAT64) Integer > Value)
    {
        Integer--;
    }

    return Integer;
}

INT32 TTFCeil(FLOAT64 Value)
{
    INT32 Integer = (INT32) Value;

    if ((FLOAT64) Integer < Value)
    {
        Integer++;
    }

    return Integer;
}

FLOAT64 TTFFAbs(FLOAT64 Value)
{
    return Value < 0.0 ? -Value : Value;
}

FLOAT64 TTFSqrt(FLOAT64 Value)
{
    FLOAT64 Result;

    if (Value <= 0.0)
    {
        return 0.0;
    }

    Result = Value >= 1.0 ? Value : 1.0;

    for (UINTN Index = 0; Index < 16; Index++)
    {
        Result = 0.5 * (Result + Value / Result);
    }

    return Result;
}

FLOAT64 TTFFMod(FLOAT64 Value, FLOAT64 Divisor)
{
    INT64 Quotient;

    if (Divisor == 0.0)
    {
        return 0.0;
    }

    Quotient = (INT64) (Value / Divisor);

    return Value - ((FLOAT64) Quotient * Divisor);
}

STATIC FLOAT64 TTFCbrt(FLOAT64 Value)
{
    FLOAT64 Result;

    if (Value == 0.0)
    {
        return 0.0;
    }

    Result = Value > 1.0 ? Value : 1.0;

    for (UINTN Index = 0; Index < 24; Index++)
    {
        Result = ((2.0 * Result) + (Value / (Result * Result))) / 3.0;
    }

    return Result;
}

FLOAT64 TTFPow(FLOAT64 Base, FLOAT64 Exponent)
{
    INT64   WholeExponent = (INT64) Exponent;
    FLOAT64 Result = 1.0;
    FLOAT64 Factor = Base;

    if (Exponent > 0.333333333 && Exponent < 0.333333334)
    {
        return TTFCbrt(Base);
    }

    if ((FLOAT64) WholeExponent != Exponent)
    {
        return 0.0;
    }

    if (WholeExponent < 0)
    {
        Factor = 1.0 / Factor;
        WholeExponent = -WholeExponent;
    }

    while (WholeExponent > 0)
    {
        if ((WholeExponent & 1) != 0)
        {
            Result *= Factor;
        }

        Factor *= Factor;
        WholeExponent >>= 1;
    }

    return Result;
}

STATIC FLOAT64 TTFNormalizeRadians(FLOAT64 Value)
{
    Value = TTFFMod(Value, TWO_PI);

    if (Value > PI)
    {
        Value -= TWO_PI;
    }
    else if (Value < -PI)
    {
        Value += TWO_PI;
    }

    return Value;
}

FLOAT64 TTFCos(FLOAT64 Value)
{
    FLOAT64 X = TTFNormalizeRadians(Value);
    FLOAT64 X2 = X * X;

    return 1.0 - (X2 / 2.0) + ((X2 * X2) / 24.0) - ((X2 * X2 * X2) / 720.0) + ((X2 * X2 * X2 * X2) / 40320.0);
}

FLOAT64 TTFACos(FLOAT64 Value)
{
    FLOAT64 Negate;
    FLOAT64 X;
    FLOAT64 Result;

    if (Value < -1.0)
    {
        Value = -1.0;
    }
    else if (Value > 1.0)
    {
        Value = 1.0;
    }

    Negate = Value < 0.0 ? 1.0 : 0.0;
    X = TTFFAbs(Value);
    Result = -0.0187293;
    Result = Result * X + 0.0742610;
    Result = Result * X - 0.2121144;
    Result = Result * X + HALF_PI;
    Result = Result * TTFSqrt(1.0 - X);
    Result = Result - (2.0 * Negate * Result);

    return (Negate * PI) + Result;
}

VOID *TTFAlloc(UINTN Size, VOID *UserData)
{
    TTF_ALLOCATOR *Allocator = (TTF_ALLOCATOR *) UserData;

    if (Allocator == NULL || Allocator->Allocate == NULL || Allocator->Free == NULL || Size == 0)
    {
        return NULL;
    }

    return Allocator->Allocate(Size, Allocator->Context);
}

VOID TTFFree(VOID *Pointer, VOID *UserData)
{
    TTF_ALLOCATOR *Allocator = (TTF_ALLOCATOR *) UserData;

    if (Pointer != NULL && Allocator != NULL && Allocator->Free != NULL)
    {
        Allocator->Free(Pointer, Allocator->Context);
    }
}

VOID TTFAssert(BOOLEAN Condition)
{
    if (!Condition)
    {
        exit(1);
    }
}

UINTN TTFStrLen(CONST CHAR8 *String)
{
    UINTN Length = 0;

    if (String != NULL)
    {
        while (String[Length] != 0)
        {
            Length++;
        }
    }

    return Length;
}
