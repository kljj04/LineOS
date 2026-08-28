// truetype_runtime.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <memory/memory.h>
#include <render/truetype/truetype_runtime.h>

#define PAGE_SIZE 4096ULL
#define PI        3.14159265358979323846
#define TWO_PI    6.28318530717958647692
#define HALF_PI   1.57079632679489661923

typedef struct
{
    UINTN PageCount;
    UINTN Size;
} TTF_ALLOC_HEADER;

INT32 KFloor(FLOAT64 Value)
{
    INT32 Integer = (INT32) Value;

    if ((FLOAT64) Integer > Value)
    {
        Integer--;
    }

    return Integer;
}

INT32 KCeil(FLOAT64 Value)
{
    INT32 Integer = (INT32) Value;

    if ((FLOAT64) Integer < Value)
    {
        Integer++;
    }

    return Integer;
}

FLOAT64 KFAbs(FLOAT64 Value)
{
    return Value < 0.0 ? -Value : Value;
}

FLOAT64 KSqrt(FLOAT64 Value)
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

FLOAT64 KFMod(FLOAT64 Value, FLOAT64 Divisor)
{
    INT64 Quotient;

    if (Divisor == 0.0)
    {
        return 0.0;
    }

    Quotient = (INT64) (Value / Divisor);
    return Value - ((FLOAT64) Quotient * Divisor);
}

STATIC FLOAT64 KCbrt(FLOAT64 Value)
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

FLOAT64 KPow(FLOAT64 Base, FLOAT64 Exponent)
{
    INT64   WholeExponent = (INT64) Exponent;
    FLOAT64 Result = 1.0;
    FLOAT64 Factor = Base;

    if (Exponent > 0.333333333 && Exponent < 0.333333334)
    {
        return KCbrt(Base);
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

STATIC FLOAT64 KNormalizeRadians(FLOAT64 Value)
{
    Value = KFMod(Value, TWO_PI);

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

FLOAT64 KCos(FLOAT64 Value)
{
    FLOAT64 X = KNormalizeRadians(Value);
    FLOAT64 X2 = X * X;

    return 1.0 - (X2 / 2.0) + ((X2 * X2) / 24.0) - ((X2 * X2 * X2) / 720.0) + ((X2 * X2 * X2 * X2) / 40320.0);
}

FLOAT64 KACos(FLOAT64 Value)
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
    X = KFAbs(Value);
    Result = -0.0187293;
    Result = Result * X + 0.0742610;
    Result = Result * X - 0.2121144;
    Result = Result * X + HALF_PI;
    Result = Result * KSqrt(1.0 - X);
    Result = Result - (2.0 * Negate * Result);

    return (Negate * PI) + Result;
}

VOID *KTTFAlloc(UINTN Size, VOID *UserData)
{
    TTF_ALLOC_HEADER *Header;
    UINTN             TotalSize;
    UINTN             PageCount;

    (VOID) UserData;

    if (Size == 0)
    {
        return NULL;
    }

    TotalSize = Size + sizeof(TTF_ALLOC_HEADER);
    PageCount = (TotalSize + PAGE_SIZE - 1) / PAGE_SIZE;
    Header = (TTF_ALLOC_HEADER *) KAllocPages(PageCount);

    if (Header == NULL)
    {
        return NULL;
    }

    Header->PageCount = PageCount;
    Header->Size = Size;

    return (VOID *) (Header + 1);
}

VOID KTTFFree(VOID *Pointer, VOID *UserData)
{
    TTF_ALLOC_HEADER *Header;

    (VOID) UserData;

    if (Pointer == NULL)
    {
        return;
    }

    Header = ((TTF_ALLOC_HEADER *) Pointer) - 1;
    KMemFreePages(Header, Header->PageCount);
}

VOID KAssert(BOOLEAN Condition)
{
    if (Condition)
    {
        return;
    }

    while (1)
    {
        ASM("hlt");
    }
}

UINTN KStrLen(CONST CHAR8 *String)
{
    UINTN Length = 0;

    if (String == NULL)
    {
        return 0;
    }

    while (String[Length] != 0)
    {
        Length++;
    }

    return Length;
}

VOID *memset(VOID *Destination, INT32 Value, UINTN Size)
{
    return KMemSet(Destination, (UINT8) Value, Size);
}

VOID *memcpy(VOID *Destination, CONST VOID *Source, UINTN Size)
{
    return KMemCpy(Destination, Source, Size);
}
