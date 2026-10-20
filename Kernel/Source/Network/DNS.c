// Kernel/Source/Network/DNS.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>
#include <Arch/AMD64/CPU.h>

STATIC BOOLEAN ReadName(CONST UINT8 *Packet, UINT32 Length, UINT32 *Offset, CHAR8 *Name)
{
    UINT32  Position = *Offset;
    UINT32  Output = 0;
    BOOLEAN Jumped = FALSE;

    for (UINT32 Step = 0; Step < 128; Step++)
    {
        UINT8 Count;

        if (Position >= Length)
        {
            return FALSE;
        }

        Count = Packet[Position++];

        if ((Count & 0xC0) == 0xC0)
        {
            UINT32 Target;

            if (Position >= Length)
            {
                return FALSE;
            }

            Target = ((UINT32) (Count & 0x3F) << 8) | Packet[Position++];

            if (Target >= Position - 2)
            {
                return FALSE;
            }

            if (!Jumped)
            {
                *Offset = Position;
                Jumped = TRUE;
            }

            Position = Target;
            continue;
        }

        if ((Count & 0xC0) != 0 || Count > 63 || Count > Length - Position || Output + Count + 1 > 254)
        {
            return FALSE;
        }

        if (Count == 0)
        {
            Name[Output] = 0;

            if (!Jumped)
            {
                *Offset = Position;
            }

            return TRUE;
        }

        if (Output != 0)
        {
            Name[Output++] = '.';
        }

        for (UINT32 Index = 0; Index < Count; Index++)
        {
            CHAR8 Value = Packet[Position++];

            if (Value == 0 || Value == '.')
            {
                return FALSE;
            }

            Name[Output++] = Value >= 'A' && Value <= 'Z' ? Value + ('a' - 'A') : Value;
        }
    }

    return FALSE;
}

STATIC BOOLEAN EqualName(CONST CHAR8 *A, CONST CHAR8 *B)
{
    for (UINT32 Index = 0; Index < 255; Index++)
    {
        if (A[Index] != B[Index])
        {
            return FALSE;
        }

        if (A[Index] == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN NetworkDNSResolveInternal(CONST CHAR8 *Name, UINT32 *Address, UINT32 TimeoutMls)
{
    UINT8   Query[512] = {0};
    UINT8   Response[512];
    CHAR8   Canonical[255];
    CHAR8   Owner[255];
    UINT32  QueryLength = 12;
    UINT32  Input = 0;
    UINT32  ResponseLength = 0;
    UINT32  Offset;
    UINT32  AnswersOffset;
    UINT16  ID = (UINT16) RDTSC();
    UINT16  Answers;
    BOOLEAN Received = FALSE;
    UINT64  Start = NetworkNow();

    if (Name == NULL || Address == NULL || TimeoutMls == 0)
    {
        return NetworkError(L"invalid DNS arguments");
    }

    *Address = 0;
    {
        UINT32  Position = 0;
        UINT32  Literal = 0;
        BOOLEAN Numeric = TRUE;

        for (UINT32 Part = 0; Part < 4; Part++)
        {
            UINT32 Value = 0;
            UINT32 Digits = 0;

            while (Position < 16 && Name[Position] >= '0' && Name[Position] <= '9')
            {
                Value = Value * 10 + Name[Position++] - '0';
                Digits++;
            }

            if (Digits == 0 || Digits > 3 || Value > 255 || (Part < 3 ? Name[Position] != '.' : Name[Position] != 0))
            {
                Numeric = FALSE;
                break;
            }

            Literal = (Literal << 8) | Value;
            Position++;
        }

        if (Numeric)
        {
            if ((Literal >> 24) == 0 || (Literal >> 24) == 127 || (Literal >> 24) >= 224)
            {
                return NetworkError(L"invalid IPv4 literal");
            }

            *Address = Literal;
            return TRUE;
        }
    }
    NetworkWrite16(Query, ID);
    NetworkWrite16(Query + 2, 0x0100);
    NetworkWrite16(Query + 4, 1);

    while (Input < 253 && Name[Input] != 0)
    {
        UINT32 Label = QueryLength++;
        UINT32 Count = 0;

        while (Input < 253 && Name[Input] != 0 && Name[Input] != '.')
        {
            CHAR8 Value = Name[Input++];

            if (!((Value >= 'a' && Value <= 'z') || (Value >= 'A' && Value <= 'Z') || (Value >= '0' && Value <= '9') || Value == '-') || Count == 63)
            {
                return NetworkError(L"invalid DNS label");
            }

            Query[QueryLength++] = Value;
            Count++;
        }

        if (Count == 0 || Query[Label + 1] == '-' || Query[QueryLength - 1] == '-')
        {
            return NetworkError(L"invalid DNS label");
        }

        Query[Label] = Count;

        if (Name[Input] == '.')
        {
            Input++;
        }
    }

    if (Input == 0 || Input >= 253 || Name[Input] != 0)
    {
        return NetworkError(L"DNS name too long or empty");
    }

    Query[QueryLength++] = 0;
    NetworkWrite16(Query + QueryLength, 1);
    NetworkWrite16(Query + QueryLength + 2, 1);
    QueryLength += 4;
    Offset = 12;

    if (!ReadName(Query, QueryLength, &Offset, Canonical))
    {
        return NetworkError(L"invalid DNS name");
    }

    for (UINT32 Attempt = 0; Attempt < 3; Attempt++)
    {
        UINT32 Remaining = NetworkRemaining(Start, TimeoutMls);
        UINT32 AttemptTimeout = Remaining / (3 - Attempt);

        if (AttemptTimeout == 0)
        {
            break;
        }

        if (NetworkUDPExchangeInternal(NetworkState.Config.DNS, 53, Query, QueryLength, Response, sizeof(Response), &ResponseLength, AttemptTimeout))
        {
            Received = TRUE;
            break;
        }
    }

    if (!Received)
    {
        return FALSE;
    }

    if (ResponseLength < 12 || NetworkRead16(Response) != ID || (NetworkRead16(Response + 2) & 0xFA0F) != 0x8000 || NetworkRead16(Response + 4) != 1)
    {
        return NetworkError(L"DNS failed, truncated or mismatched response");
    }

    Offset = 12;

    if (!ReadName(Response, ResponseLength, &Offset, Owner) || !EqualName(Owner, Canonical) || Offset + 4 > ResponseLength || NetworkRead16(Response + Offset) != 1 || NetworkRead16(Response + Offset + 2) != 1)
    {
        return NetworkError(L"DNS question mismatch");
    }

    AnswersOffset = Offset + 4;
    Answers = NetworkRead16(Response + 6);

    for (UINT32 Depth = 0; Depth < 8; Depth++)
    {
        BOOLEAN Alias = FALSE;
        Offset = AnswersOffset;

        for (UINT32 Index = 0; Index < Answers; Index++)
        {
            UINT16 Type;
            UINT16 Class;
            UINT16 DataLength;
            UINT32 End;

            if (!ReadName(Response, ResponseLength, &Offset, Owner) || Offset + 10 > ResponseLength)
            {
                return NetworkError(L"DNS answer malformed");
            }

            Type = NetworkRead16(Response + Offset);
            Class = NetworkRead16(Response + Offset + 2);
            DataLength = NetworkRead16(Response + Offset + 8);
            Offset += 10;

            if (DataLength > ResponseLength - Offset)
            {
                return NetworkError(L"DNS answer truncated");
            }

            End = Offset + DataLength;

            if (Class == 1 && EqualName(Owner, Canonical))
            {
                if (Type == 1 && DataLength == 4)
                {
                    UINT32 Result = NetworkRead32(Response + Offset);
                    UINT8  First = Result >> 24;

                    if (First == 0 || First == 127 || First >= 224)
                    {
                        return NetworkError(L"DNS address unsupported");
                    }

                    *Address = Result;
                    NetworkError(L"ok");
                    return TRUE;
                }

                if (Type == 5)
                {
                    if (!ReadName(Response, ResponseLength, &Offset, Canonical) || Offset != End)
                    {
                        return NetworkError(L"DNS CNAME malformed");
                    }

                    Alias = TRUE;
                    break;
                }
            }

            Offset = End;
        }

        if (!Alias)
        {
            break;
        }
    }

    return NetworkError(L"DNS A record unavailable");
}

BOOLEAN NetworkDNSResolve(CONST CHAR8 *Name, UINT32 *Address, UINT32 TimeoutMls)
{
    BOOLEAN Result;

    if (!NetworkBegin())
    {
        return FALSE;
    }

    Result = NetworkDNSResolveInternal(Name, Address, TimeoutMls);
    NetworkEnd();
    return Result;
}
