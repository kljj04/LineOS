// Kernel/Source/Network/HTTP.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Network/NetworkInternal.h>
#include <Memory/PMM.h>

typedef struct
{
    UINT8   Buffer[1024];
    UINT32  Position;
    UINT32  Length;
    UINT64  Start;
    UINT32  TimeoutMls;
    BOOLEAN EOFReached;
} NETWORK_HTTP_STREAM;

STATIC INT32 ReadByte(NETWORK_HTTP_STREAM *Stream)
{
    if (Stream->Position == Stream->Length)
    {
        if (Stream->EOFReached)
        {
            return -1;
        }

        UINT64 Elapsed = NetworkNow() - Stream->Start;

        if (Elapsed >= Stream->TimeoutMls)
        {
            NetworkError(L"HTTP total response timeout");
            return -2;
        }

        if (!NetworkTCPReceiveInternal(Stream->Buffer, sizeof(Stream->Buffer), &Stream->Length, Stream->TimeoutMls - (UINT32) Elapsed))
        {
            return -2;
        }

        Stream->Position = 0;

        if (Stream->Length == 0)
        {
            Stream->EOFReached = TRUE;
            return -1;
        }
    }

    return Stream->Buffer[Stream->Position++];
}

STATIC BOOLEAN ReadLine(NETWORK_HTTP_STREAM *Stream, CHAR8 *Line, UINT32 Capacity, UINT32 *Budget)
{
    UINT32 Length = 0;

    for (;;)
    {
        INT32 Value = ReadByte(Stream);

        if (Value < 0 || *Budget == 0)
        {
            return NetworkError(L"HTTP headers truncated or too large");
        }

        (*Budget)--;

        if (Value == '\r')
        {
            if (*Budget == 0 || ReadByte(Stream) != '\n')
            {
                return NetworkError(L"invalid HTTP line ending");
            }

            (*Budget)--;
            Line[Length] = 0;
            return TRUE;
        }

        if ((Value < 32 && Value != '\t') || Value > 126 || Length + 1 >= Capacity)
        {
            return NetworkError(L"invalid or oversized HTTP header line");
        }

        Line[Length++] = (CHAR8) Value;
    }
}

STATIC BOOLEAN EqualText(CONST CHAR8 *A, CONST char *B)
{
    while (*A != 0 && *B != 0)
    {
        CHAR8 Value = *A++;

        if (Value >= 'A' && Value <= 'Z')
        {
            Value += 'a' - 'A';
        }

        if (Value != (CHAR8) *B++)
        {
            return FALSE;
        }
    }

    return *A == 0 && *B == 0;
}

STATIC BOOLEAN ParseDecimal(CONST CHAR8 *Text, UINT32 *Value)
{
    UINT32 Result = 0;

    if (*Text == 0)
    {
        return FALSE;
    }

    while (*Text != 0)
    {
        UINT8 Digit = *Text++ - '0';

        if (Digit > 9 || Result > (UINT32_MAX - Digit) / 10)
        {
            return FALSE;
        }

        Result = Result * 10 + Digit;
    }

    *Value = Result;
    return TRUE;
}

STATIC BOOLEAN CopyBody(NETWORK_HTTP_STREAM *Stream, UINT8 *Body, UINT32 Capacity, UINT32 *Length, UINT32 Count)
{
    if (Count > Capacity - *Length)
    {
        return NetworkError(L"HTTP body buffer too small");
    }

    for (UINT32 Index = 0; Index < Count; Index++)
    {
        INT32 Value = ReadByte(Stream);

        if (Value < 0)
        {
            return NetworkError(L"HTTP body truncated or read failed");
        }

        Body[(*Length)++] = (UINT8) Value;
    }

    return TRUE;
}

STATIC BOOLEAN ReadResponse(NETWORK_HTTP_STREAM *Stream, UINT8 *Body, UINT32 Capacity, UINT32 *Length, UINT16 *Status)
{
    CHAR8   Line[1024] = {0};
    UINT32  Budget = 8192;
    UINT32  ContentLength = 0;
    BOOLEAN HasLength = FALSE;
    BOOLEAN Chunked = FALSE;
    BOOLEAN Final = FALSE;

    for (UINT32 Interim = 0; Interim < 5; Interim++)
    {
        if (!ReadLine(Stream, Line, sizeof(Line), &Budget))
        {
            return FALSE;
        }

        if (Line[0] != 'H' || Line[1] != 'T' || Line[2] != 'T' || Line[3] != 'P' || Line[4] != '/' || Line[5] != '1' || Line[6] != '.' || (Line[7] != '0' && Line[7] != '1') || Line[8] != ' ' || Line[9] < '1' || Line[9] > '5' || Line[10] < '0' || Line[10] > '9' || Line[11] < '0' || Line[11] > '9' || Line[12] != ' ')
        {
            return NetworkError(L"invalid HTTP status line");
        }

        *Status = (Line[9] - '0') * 100 + (Line[10] - '0') * 10 + Line[11] - '0';
        HasLength = FALSE;
        Chunked = FALSE;

        for (;;)
        {
            CHAR8 *Value;
            UINT32 Index = 0;
            UINT32 End;

            if (!ReadLine(Stream, Line, sizeof(Line), &Budget))
            {
                return FALSE;
            }

            if (Line[0] == 0)
            {
                break;
            }

            while (Line[Index] != 0 && Line[Index] != ':')
            {
                CHAR8 Ch = Line[Index];

                if (!((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9') || Ch == '!' || Ch == '#' || Ch == '$' || Ch == '%' || Ch == '&' || Ch == '\'' || Ch == '*' || Ch == '+' || Ch == '-' || Ch == '.' || Ch == '^' || Ch == '_' || Ch == '`' || Ch == '|' || Ch == '~'))
                {
                    return NetworkError(L"invalid HTTP header name");
                }

                Index++;
            }

            if (Index == 0 || Line[Index] != ':')
            {
                return NetworkError(L"malformed HTTP header");
            }

            Line[Index++] = 0;
            Value = Line + Index;

            while (*Value == ' ' || *Value == '\t')
            {
                Value++;
            }

            End = 0;

            while (Value[End] != 0)
            {
                End++;
            }

            while (End != 0 && (Value[End - 1] == ' ' || Value[End - 1] == '\t'))
            {
                Value[--End] = 0;
            }

            if (EqualText(Line, "content-length"))
            {
                if (HasLength || !ParseDecimal(Value, &ContentLength))
                {
                    return NetworkError(L"invalid or duplicate HTTP Content-Length");
                }

                HasLength = TRUE;
            }
            else if (EqualText(Line, "transfer-encoding"))
            {
                if (Chunked || !EqualText(Value, "chunked"))
                {
                    return NetworkError(L"unsupported HTTP transfer encoding");
                }

                Chunked = TRUE;
            }
            else if (EqualText(Line, "content-encoding") && !EqualText(Value, "identity"))
            {
                return NetworkError(L"unsupported HTTP content encoding");
            }
        }

        if (HasLength && Chunked)
        {
            return NetworkError(L"ambiguous HTTP body framing");
        }

        if (*Status >= 200)
        {
            Final = TRUE;
            break;
        }

        if (*Status == 101 || HasLength || Chunked)
        {
            return NetworkError(L"unsupported HTTP interim response");
        }
    }

    if (!Final)
    {
        return NetworkError(L"too many HTTP interim responses");
    }

    if (*Status == 204 || *Status == 304)
    {
        return TRUE;
    }

    if (Chunked)
    {
        for (;;)
        {
            UINT32 Count = 0;
            UINT32 Index = 0;
            UINT32 ChunkBudget = 1024;

            if (!ReadLine(Stream, Line, sizeof(Line), &ChunkBudget))
            {
                return FALSE;
            }

            while (Line[Index] != 0 && Line[Index] != ';')
            {
                CHAR8  Ch = Line[Index++];
                UINT32 Digit;

                if (Ch >= '0' && Ch <= '9')
                {
                    Digit = Ch - '0';
                }
                else if (Ch >= 'a' && Ch <= 'f')
                {
                    Digit = Ch - 'a' + 10;
                }
                else if (Ch >= 'A' && Ch <= 'F')
                {
                    Digit = Ch - 'A' + 10;
                }
                else
                {
                    return NetworkError(L"invalid HTTP chunk size");
                }

                if (Count > (UINT32_MAX - Digit) / 16)
                {
                    return NetworkError(L"HTTP chunk size overflow");
                }

                Count = Count * 16 + Digit;
            }

            if (Index == 0)
            {
                return NetworkError(L"empty HTTP chunk size");
            }

            if (Count == 0)
            {
                UINT32 TrailerBudget = 8192;

                do
                {
                    if (!ReadLine(Stream, Line, sizeof(Line), &TrailerBudget))
                    {
                        return FALSE;
                    }
                } while (Line[0] != 0);

                return TRUE;
            }

            if (!CopyBody(Stream, Body, Capacity, Length, Count) || ReadByte(Stream) != '\r' || ReadByte(Stream) != '\n')
            {
                return NetworkError(L"HTTP chunk truncated or body buffer too small");
            }
        }
    }

    if (HasLength)
    {
        return CopyBody(Stream, Body, Capacity, Length, ContentLength);
    }

    for (;;)
    {
        INT32 Value = ReadByte(Stream);

        if (Value == -1)
        {
            return TRUE;
        }

        if (Value < 0)
        {
            return FALSE;
        }

        if (*Length == Capacity)
        {
            return NetworkError(L"HTTP body buffer too small");
        }

        Body[(*Length)++] = (UINT8) Value;
    }
}

BOOLEAN NetworkHTTPGet(CONST CHAR8 *Host, UINT16 Port, CONST CHAR8 *Path, VOID *Body, UINT32 Capacity, UINT32 *Length, UINT16 *Status, UINT32 TimeoutMls)
{
    UINT64              Start;
    UINT8               Request[1024];
    UINT32              RequestLength = 0;
    UINT32              HostLength = 0;
    UINT32              PathLength = 0;
    UINT32              Address;
    BOOLEAN             Result = FALSE;
    BOOLEAN             Opened = FALSE;
    NETWORK_HTTP_STREAM Stream = {0};
    CONST char         *Parts[5];
    CHAR8               PortText[7] = {0};

    if (Host == NULL || Path == NULL || Path[0] != '/' || Port == 0 || Body == NULL || Length == NULL || Status == NULL || TimeoutMls == 0)
    {
        return NetworkError(L"invalid HTTP arguments");
    }

    *Length = 0;
    *Status = 0;

    while (HostLength < 253 && Host[HostLength] != 0)
    {
        CHAR8 Ch = Host[HostLength++];

        if (!((Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') || (Ch >= '0' && Ch <= '9') || Ch == '.' || Ch == '-'))
        {
            return NetworkError(L"invalid HTTP host");
        }
    }

    while (PathLength < 512 && Path[PathLength] != 0)
    {
        CHAR8 Ch = Path[PathLength++];

        if (Ch <= 32 || Ch >= 127 || Ch == '#')
        {
            return NetworkError(L"invalid HTTP path");
        }
    }

    if (HostLength == 0 || HostLength >= 253 || PathLength >= 512)
    {
        return NetworkError(L"HTTP host or path too long");
    }

    Parts[0] = "GET ";
    Parts[1] = (CONST char *) Path;
    Parts[2] = " HTTP/1.1\r\nHost: ";
    Parts[3] = (CONST char *) Host;
    Parts[4] = "";

    for (UINT32 Part = 0; Part < 5; Part++)
    {
        for (UINT32 Index = 0; Parts[Part][Index] != 0; Index++)
        {
            Request[RequestLength++] = Parts[Part][Index];
        }
    }

    if (Port != 80)
    {
        UINT16 Value = Port;
        UINT32 Count = 0;

        Request[RequestLength++] = ':';

        do
        {
            PortText[Count++] = '0' + Value % 10;
            Value /= 10;
        } while (Value != 0);

        while (Count != 0)
        {
            Request[RequestLength++] = PortText[--Count];
        }
    }

    {
        CONST char *Tail = "\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n";

        for (UINT32 Index = 0; Tail[Index] != 0; Index++)
        {
            Request[RequestLength++] = Tail[Index];
        }
    }

    if (!NetworkBegin())
    {
        return FALSE;
    }

    Start = NetworkNow();

    if (NetworkDNSResolveInternal(Host, &Address, NetworkRemaining(Start, TimeoutMls)) && NetworkTCPConnectInternal(Address, Port, NetworkRemaining(Start, TimeoutMls)))
    {
        Opened = TRUE;

        if (NetworkTCPSendInternal(Request, RequestLength, NetworkRemaining(Start, TimeoutMls)))
        {
            Stream.Start = Start;
            Stream.TimeoutMls = TimeoutMls;
            Result = ReadResponse(&Stream, Body, Capacity, Length, Status);
        }
    }

    if (Opened)
    {
        if (Result)
        {
            Result = NetworkTCPCloseInternal(NetworkRemaining(Start, TimeoutMls));
        }
        else
        {
            NetworkTCPAbortInternal();
        }
    }

    NetworkEnd();

    if (Result)
    {
        NetworkError(L"ok");
    }

    return Result;
}
