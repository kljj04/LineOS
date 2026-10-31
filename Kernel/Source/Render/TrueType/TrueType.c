// Kernel/Source/Render/TrueType/TrueType.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/TrueType/TrueType.h>
#include <Render/TrueType/TrueTypeRuntime.h>
#include <LineOS/TypeInfo.h>

#define LINEOS_MAX_OVERSAMPLE 8
typedef INT32 TestOversamplePow2[(LINEOS_MAX_OVERSAMPLE & (LINEOS_MAX_OVERSAMPLE - 1)) == 0 ? 1 : -1];
#define LINEOS_NOTUSED(v) (VOID) sizeof(v)
STATIC UINT8 BufGet8(BUFFER *B)
{
    if (B->Cursor >= B->Size)
        return 0;

    return B->Data[B->Cursor++];
}
STATIC UINT8 BufPeek8(BUFFER *B)
{
    if (B->Cursor >= B->Size)
        return 0;

    return B->Data[B->Cursor];
}
STATIC VOID BufSeek(BUFFER *B, INT32 O)
{
    KAssert(!(O > B->Size || O < 0));
    B->Cursor = (O > B->Size || O < 0) ? B->Size : O;
}
STATIC VOID BufSkip(BUFFER *B, INT32 O)
{
    BufSeek(B, B->Cursor + O);
}
STATIC UINT32 BufGet(BUFFER *B, INT32 N)
{
    UINT32 V = 0;
    INT32  I;

    KAssert(N >= 1 && N <= 4);

    for (I = 0; I < N; I++)
        V = (V << 8) | BufGet8(B);

    return V;
}
STATIC BUFFER NewBuf(CONST VOID *P, UINTN Size)
{
    BUFFER R;

    KAssert(Size < 0x40000000);
    R.Data = (UINT8 *) P;
    R.Size = (INT32) Size;
    R.Cursor = 0;

    return R;
}
#define BUF_GET16(b) BufGet((b), 2)
#define BUF_GET32(b) BufGet((b), 4)
STATIC BUFFER BufRange(CONST BUFFER *B, INT32 O, INT32 S)
{
    BUFFER R = NewBuf(NULL, 0);

    if (O < 0 || S < 0 || O > B->Size || S > B->Size - O)
        return R;
    R.Data = B->Data + O;
    R.Size = S;

    return R;
}
STATIC BUFFER CffGetIndex(BUFFER *B)
{
    INT32 Count, Start, Offsize;

    Start = B->Cursor;
    Count = BUF_GET16(B);

    if (Count)
    {
        Offsize = BufGet8(B);
        KAssert(Offsize >= 1 && Offsize <= 4);
        BufSkip(B, Offsize * Count);
        BufSkip(B, BufGet(B, Offsize) - 1);
    }

    return BufRange(B, Start, B->Cursor - Start);
}
STATIC UINT32 CffInt(BUFFER *B)
{
    INT32 B0 = BufGet8(B);

    if (B0 >= 32 && B0 <= 246)
        return B0 - 139;
    else if (B0 >= 247 && B0 <= 250)

        return (B0 - 247) * 256 + BufGet8(B) + 108;
    else if (B0 >= 251 && B0 <= 254)

        return -(B0 - 251) * 256 - BufGet8(B) - 108;
    else if (B0 == 28)

        return BUF_GET16(B);
    else if (B0 == 29)

        return BUF_GET32(B);
    KAssert(0);

    return 0;
}
STATIC VOID CffSkipOperand(BUFFER *B)
{
    INT32 V, B0 = BufPeek8(B);

    KAssert(B0 >= 28);

    if (B0 == 30)
    {
        BufSkip(B, 1);

        while (B->Cursor < B->Size)
        {
            V = BufGet8(B);

            if ((V & 0xF) == 0xF || (V >> 4) == 0xF)
                break;
        }
    }
    else
    {
        CffInt(B);
    }
}
STATIC BUFFER DictGet(BUFFER *B, INT32 Key)
{
    BufSeek(B, 0);

    while (B->Cursor < B->Size)
    {
        INT32 Start = B->Cursor, End, Op;

        while (BufPeek8(B) >= 28)
            CffSkipOperand(B);
        End = B->Cursor;
        Op = BufGet8(B);

        if (Op == 12)
            Op = BufGet8(B) | 0x100;

        if (Op == Key)
            return BufRange(B, Start, End - Start);
    }

    return BufRange(B, 0, 0);
}
STATIC VOID DictGetInts(BUFFER *B, INT32 Key, INT32 Outcount, UINT32 *Out)
{
    INT32  I;
    BUFFER Operands = DictGet(B, Key);

    for (I = 0; I < Outcount && Operands.Cursor < Operands.Size; I++)
        Out[I] = CffInt(&Operands);
}
STATIC INT32 CffIndexCount(BUFFER *B)
{
    BufSeek(B, 0);

    return BUF_GET16(B);
}
STATIC BUFFER CffIndexGet(BUFFER B, INT32 I)
{
    INT32 Count, Offsize, Start, End;

    BufSeek(&B, 0);
    Count = BUF_GET16(&B);
    Offsize = BufGet8(&B);
    KAssert(I >= 0 && I < Count);
    KAssert(Offsize >= 1 && Offsize <= 4);
    BufSkip(&B, I * Offsize);
    Start = BufGet(&B, Offsize);
    End = BufGet(&B, Offsize);

    return BufRange(&B, 2 + (Count + 1) * Offsize + Start, End - Start);
}
#define TT_BYTE(p)  (*(UINT8 *) (p))
#define TT_CHAR(p)  (*(INT8 *) (p))
#define TT_FIXED(p) TTLONG(p)
STATIC UINT16 TTUSHORT(UINT8 *P)
{
    return P[0] * 256 + P[1];
}
STATIC INT16 TTSHORT(UINT8 *P)
{
    return P[0] * 256 + P[1];
}
STATIC UINT32 TTULONG(UINT8 *P)
{
    return (P[0] << 24) + (P[1] << 16) + (P[2] << 8) + P[3];
}
STATIC INT32 TTLONG(UINT8 *P)
{
    return (P[0] << 24) + (P[1] << 16) + (P[2] << 8) + P[3];
}
#define TAG4(p, c0, c1, c2, c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define TAG(p, str)             TAG4(p, str[0], str[1], str[2], str[3])
STATIC INT32 Isfont(UINT8 *Font)
{
    if (TAG4(Font, '1', 0, 0, 0))
        return 1;

    if (TAG(Font, "typ1"))
        return 1;

    if (TAG(Font, "OTTO"))
        return 1;

    if (TAG4(Font, 0, 1, 0, 0))
        return 1;

    if (TAG(Font, "true"))
        return 1;

    return 0;
}
STATIC UINT32 FindTable(UINT8 *Data, UINT32 Fontstart, CONST VOID *TagName)
{
    CONST UINT8 *Tag = (CONST UINT8 *) TagName;
    INT32        NumTables = TTUSHORT(Data + Fontstart + 4);
    UINT32       Tabledir = Fontstart + 12;
    INT32        I;

    for (I = 0; I < NumTables; ++I)
    {
        UINT32 Loc = Tabledir + 16 * I;

        if (TAG(Data + Loc + 0, Tag))
            return TTULONG(Data + Loc + 8);
    }

    return 0;
}
STATIC INT32 GetFontOffsetForIndexInternal(UINT8 *FontCollection, INT32 Index)
{
    if (Isfont(FontCollection))
        return Index == 0 ? 0 : -1;

    if (TAG(FontCollection, "ttcf"))
    {
        if (TTULONG(FontCollection + 4) == 0x00010000 || TTULONG(FontCollection + 4) == 0x00020000)
        {
            INT32 N = TTLONG(FontCollection + 8);

            if (Index >= N)
                return -1;

            return TTULONG(FontCollection + 12 + Index * 4);
        }
    }

    return -1;
}
STATIC INT32 GetNumberOfFontsInternal(UINT8 *FontCollection)
{
    if (Isfont(FontCollection))
        return 1;

    if (TAG(FontCollection, "ttcf"))
    {
        if (TTULONG(FontCollection + 4) == 0x00010000 || TTULONG(FontCollection + 4) == 0x00020000)
        {
            return TTLONG(FontCollection + 8);
        }
    }

    return 0;
}
STATIC BUFFER GetSubrs(BUFFER Cff, BUFFER Fontdict)
{
    UINT32 Subrsoff = 0, PrivateLoc[2] = {0, 0};
    BUFFER Pdict;

    DictGetInts(&Fontdict, 18, 2, PrivateLoc);

    if (!PrivateLoc[1] || !PrivateLoc[0])
        return NewBuf(NULL, 0);
    Pdict = BufRange(&Cff, PrivateLoc[1], PrivateLoc[0]);
    DictGetInts(&Pdict, 19, 1, &Subrsoff);

    if (!Subrsoff)
        return NewBuf(NULL, 0);
    BufSeek(&Cff, PrivateLoc[1] + Subrsoff);

    return CffGetIndex(&Cff);
}
STATIC INT32 GetSVG(FONT_INFO *Info)
{
    UINT32 T;

    if (Info->Svg < 0)
    {
        T = FindTable(Info->Data, Info->FontStart, "SVG ");

        if (T)
        {
            UINT32 Offset = TTULONG(Info->Data + T + 2);

            Info->Svg = T + Offset;
        }
        else
        {
            Info->Svg = 0;
        }
    }

    return Info->Svg;
}
STATIC INT32 InitFontInternal(FONT_INFO *Info, UINT8 *Data, INT32 Fontstart)
{
    UINT32 Cmap, T;
    INT32  I, NumTables;

    Info->Data = Data;
    Info->FontStart = Fontstart;
    Info->CFF = NewBuf(NULL, 0);
    Cmap = FindTable(Data, Fontstart, "cmap");
    Info->Loca = FindTable(Data, Fontstart, "loca");
    Info->Head = FindTable(Data, Fontstart, "head");
    Info->Glyf = FindTable(Data, Fontstart, "glyf");
    Info->Hhea = FindTable(Data, Fontstart, "hhea");
    Info->Hmtx = FindTable(Data, Fontstart, "hmtx");
    Info->Kern = FindTable(Data, Fontstart, "kern");
    Info->GPOS = FindTable(Data, Fontstart, "GPOS");

    if (!Cmap || !Info->Head || !Info->Hhea || !Info->Hmtx)
        return 0;

    if (Info->Glyf)
    {
        if (!Info->Loca)
            return 0;
    }
    else
    {
        BUFFER B, Topdict, Topdictidx;
        UINT32 Cstype = 2, Charstrings = 0, Fdarrayoff = 0, Fdselectoff = 0;
        UINT32 Cff;

        Cff = FindTable(Data, Fontstart, "CFF ");

        if (!Cff)
            return 0;
        Info->FontDicts = NewBuf(NULL, 0);
        Info->FdSelect = NewBuf(NULL, 0);
        Info->CFF = NewBuf(Data + Cff, 512 * 1024 * 1024);
        B = Info->CFF;
        BufSkip(&B, 2);
        BufSeek(&B, BufGet8(&B));
        CffGetIndex(&B);
        Topdictidx = CffGetIndex(&B);
        Topdict = CffIndexGet(Topdictidx, 0);
        CffGetIndex(&B);
        Info->GSubrs = CffGetIndex(&B);
        DictGetInts(&Topdict, 17, 1, &Charstrings);
        DictGetInts(&Topdict, 0x100 | 6, 1, &Cstype);
        DictGetInts(&Topdict, 0x100 | 36, 1, &Fdarrayoff);
        DictGetInts(&Topdict, 0x100 | 37, 1, &Fdselectoff);
        Info->Subrs = GetSubrs(B, Topdict);

        if (Cstype != 2)
            return 0;

        if (Charstrings == 0)
            return 0;

        if (Fdarrayoff)
        {
            if (!Fdselectoff)
                return 0;
            BufSeek(&B, Fdarrayoff);
            Info->FontDicts = CffGetIndex(&B);
            Info->FdSelect = BufRange(&B, Fdselectoff, B.Size - Fdselectoff);
        }

        BufSeek(&B, Charstrings);
        Info->CharStrings = CffGetIndex(&B);
    }

    T = FindTable(Data, Fontstart, "maxp");

    if (T)
        Info->NumGlyphs = TTUSHORT(Data + T + 4);
    else
        Info->NumGlyphs = 0xffff;
    Info->Svg = -1;
    NumTables = TTUSHORT(Data + Cmap + 2);
    Info->IndexMap = 0;

    for (I = 0; I < NumTables; ++I)
    {
        UINT32 EncodingRecord = Cmap + 4 + 8 * I;

        switch (TTUSHORT(Data + EncodingRecord))
        {
            case 3:
                switch (TTUSHORT(Data + EncodingRecord + 2))
                {
                    case 1:
                    case 10:
                        Info->IndexMap = Cmap + TTULONG(Data + EncodingRecord + 4);
                        break;
                }

                break;

            case 0:
                Info->IndexMap = Cmap + TTULONG(Data + EncodingRecord + 4);
                break;
        }
    }

    if (Info->IndexMap == 0)
        return 0;
    Info->IndexToLocFormat = TTUSHORT(Data + Info->Head + 50);

    return 1;
}
EXTERN INT32 FindGlyphIndex(CONST FONT_INFO *Info, INT32 UnicodeCodepoint)
{
    UINT8 *Data = Info->Data;
    UINT32 IndexMap = Info->IndexMap;
    UINT16 Format = TTUSHORT(Data + IndexMap + 0);

    if (Format == 0)
    {
        INT32 Bytes = TTUSHORT(Data + IndexMap + 2);

        if (UnicodeCodepoint < Bytes - 6)
            return TT_BYTE(Data + IndexMap + 6 + UnicodeCodepoint);

        return 0;
    }
    else if (Format == 6)
    {
        UINT32 First = TTUSHORT(Data + IndexMap + 6);
        UINT32 Count = TTUSHORT(Data + IndexMap + 8);

        if ((UINT32) UnicodeCodepoint >= First && (UINT32) UnicodeCodepoint < First + Count)
            return TTUSHORT(Data + IndexMap + 10 + (UnicodeCodepoint - First) * 2);

        return 0;
    }
    else if (Format == 2)
    {
        KAssert(0);

        return 0;
    }
    else if (Format == 4)
    {
        UINT16 Segcount = TTUSHORT(Data + IndexMap + 6) >> 1;
        UINT16 SearchRange = TTUSHORT(Data + IndexMap + 8) >> 1;
        UINT16 EntrySelector = TTUSHORT(Data + IndexMap + 10);
        UINT16 RangeShift = TTUSHORT(Data + IndexMap + 12) >> 1;
        UINT32 EndCount = IndexMap + 14;
        UINT32 Search = EndCount;

        if (UnicodeCodepoint > 0xffff)
            return 0;

        if (UnicodeCodepoint >= TTUSHORT(Data + Search + RangeShift * 2))
            Search += RangeShift * 2;
        Search -= 2;

        while (EntrySelector)
        {
            UINT16 End;

            SearchRange >>= 1;
            End = TTUSHORT(Data + Search + SearchRange * 2);

            if (UnicodeCodepoint > End)
                Search += SearchRange * 2;
            --EntrySelector;
        }

        Search += 2;
        {
            UINT16 Offset, Start, Last;
            UINT16 Item = (UINT16) ((Search - EndCount) >> 1);

            Start = TTUSHORT(Data + IndexMap + 14 + Segcount * 2 + 2 + 2 * Item);
            Last = TTUSHORT(Data + EndCount + 2 * Item);

            if (UnicodeCodepoint < Start || UnicodeCodepoint > Last)
                return 0;
            Offset = TTUSHORT(Data + IndexMap + 14 + Segcount * 6 + 2 + 2 * Item);

            if (Offset == 0)
                return (UINT16) (UnicodeCodepoint + TTSHORT(Data + IndexMap + 14 + Segcount * 4 + 2 + 2 * Item));

            return TTUSHORT(Data + Offset + (UnicodeCodepoint - Start) * 2 + IndexMap + 14 + Segcount * 6 + 2 + 2 * Item);
        }
    }
    else if (Format == 12 || Format == 13)
    {
        UINT32 Ngroups = TTULONG(Data + IndexMap + 12);
        INT32  Low, High;

        Low = 0;
        High = (INT32) Ngroups;

        while (Low < High)
        {
            INT32  Mid = Low + ((High - Low) >> 1);
            UINT32 StartChar = TTULONG(Data + IndexMap + 16 + Mid * 12);
            UINT32 EndChar = TTULONG(Data + IndexMap + 16 + Mid * 12 + 4);

            if ((UINT32) UnicodeCodepoint < StartChar)
                High = Mid;
            else if ((UINT32) UnicodeCodepoint > EndChar)
                Low = Mid + 1;
            else
            {
                UINT32 StartGlyph = TTULONG(Data + IndexMap + 16 + Mid * 12 + 8);

                if (Format == 12)
                    return StartGlyph + UnicodeCodepoint - StartChar;
                else

                    return StartGlyph;
            }
        }

        return 0;
    }

    KAssert(0);

    return 0;
}
EXTERN INT32 GetCodepointShape(CONST FONT_INFO *Info, INT32 UnicodeCodepoint, VERTEX **Vertices)
{
    return GetGlyphShape(Info, FindGlyphIndex(Info, UnicodeCodepoint), Vertices);
}
STATIC VOID Setvertex(VERTEX *V, UINT8 Type, INT32 X, INT32 Y, INT32 Cx, INT32 Cy)
{
    V->Type = Type;
    V->X = (INT16) X;
    V->Y = (INT16) Y;
    V->Cx = (INT16) Cx;
    V->Cy = (INT16) Cy;
}
STATIC INT32 GetGlyfOffset(CONST FONT_INFO *Info, INT32 GlyphIndex)
{
    INT32 G1, G2;

    KAssert(!Info->CFF.Size);

    if (GlyphIndex >= Info->NumGlyphs)
        return -1;

    if (Info->IndexToLocFormat >= 2)
        return -1;

    if (Info->IndexToLocFormat == 0)
    {
        G1 = Info->Glyf + TTUSHORT(Info->Data + Info->Loca + GlyphIndex * 2) * 2;
        G2 = Info->Glyf + TTUSHORT(Info->Data + Info->Loca + GlyphIndex * 2 + 2) * 2;
    }
    else
    {
        G1 = Info->Glyf + TTULONG(Info->Data + Info->Loca + GlyphIndex * 4);
        G2 = Info->Glyf + TTULONG(Info->Data + Info->Loca + GlyphIndex * 4 + 4);
    }

    return G1 == G2 ? -1 : G1;
}
STATIC INT32 GetGlyphInfoT2(CONST FONT_INFO *Info, INT32 GlyphIndex, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1);
EXTERN INT32 GetGlyphBox(CONST FONT_INFO *Info, INT32 GlyphIndex, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1)
{
    if (Info->CFF.Size)
    {
        GetGlyphInfoT2(Info, GlyphIndex, X0, Y0, X1, Y1);
    }
    else
    {
        INT32 G = GetGlyfOffset(Info, GlyphIndex);

        if (G < 0)
            return 0;

        if (X0)
            *X0 = TTSHORT(Info->Data + G + 2);

        if (Y0)
            *Y0 = TTSHORT(Info->Data + G + 4);

        if (X1)
            *X1 = TTSHORT(Info->Data + G + 6);

        if (Y1)
            *Y1 = TTSHORT(Info->Data + G + 8);
    }

    return 1;
}
EXTERN INT32 GetCodepointBox(CONST FONT_INFO *Info, INT32 Codepoint, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1)
{
    return GetGlyphBox(Info, FindGlyphIndex(Info, Codepoint), X0, Y0, X1, Y1);
}
EXTERN INT32 IsGlyphEmpty(CONST FONT_INFO *Info, INT32 GlyphIndex)
{
    INT16 NumberOfContours;
    INT32 G;

    if (Info->CFF.Size)
        return GetGlyphInfoT2(Info, GlyphIndex, NULL, NULL, NULL, NULL) == 0;
    G = GetGlyfOffset(Info, GlyphIndex);

    if (G < 0)
        return 1;
    NumberOfContours = TTSHORT(Info->Data + G);

    return NumberOfContours == 0;
}
STATIC INT32 CloseShape(VERTEX *Vertices, INT32 NumVertices, INT32 WasOff, INT32 StartOff, INT32 Sx, INT32 Sy, INT32 Scx, INT32 Scy, INT32 Cx, INT32 Cy)
{
    if (StartOff)
    {
        if (WasOff)
            Setvertex(&Vertices[NumVertices++], LINEOS_VCURVE, (Cx + Scx) >> 1, (Cy + Scy) >> 1, Cx, Cy);
        Setvertex(&Vertices[NumVertices++], LINEOS_VCURVE, Sx, Sy, Scx, Scy);
    }
    else
    {
        if (WasOff)
            Setvertex(&Vertices[NumVertices++], LINEOS_VCURVE, Sx, Sy, Cx, Cy);
        else
            Setvertex(&Vertices[NumVertices++], LINEOS_VLINE, Sx, Sy, 0, 0);
    }

    return NumVertices;
}
STATIC INT32 GetGlyphShapeTT(CONST FONT_INFO *Info, INT32 GlyphIndex, VERTEX **Pvertices)
{
    INT16   NumberOfContours;
    UINT8  *EndPtsOfContours;
    UINT8  *Data = Info->Data;
    VERTEX *Vertices = 0;
    INT32   NumVertices = 0;
    INT32   G = GetGlyfOffset(Info, GlyphIndex);

    *Pvertices = NULL;

    if (G < 0)
        return 0;
    NumberOfContours = TTSHORT(Data + G);

    if (NumberOfContours > 0)
    {
        UINT8  Flags = 0, Flagcount;
        INT32  Ins, I, J = 0, M, N, NextMove, WasOff = 0, Off, StartOff = 0;
        INT32  X, Y, Cx, Cy, Sx, Sy, Scx, Scy;
        UINT8 *Points;

        EndPtsOfContours = (Data + G + 10);
        Ins = TTUSHORT(Data + G + 10 + NumberOfContours * 2);
        Points = Data + G + 10 + NumberOfContours * 2 + 2 + Ins;
        N = 1 + TTUSHORT(EndPtsOfContours + NumberOfContours * 2 - 2);
        M = N + 2 * NumberOfContours;
        Vertices = (VERTEX *) KTTFAlloc(M * sizeof(Vertices[0]), Info->UserData);

        if (Vertices == 0)
            return 0;
        NextMove = 0;
        Flagcount = 0;
        Off = M - N;

        for (I = 0; I < N; ++I)
        {
            if (Flagcount == 0)
            {
                Flags = *Points++;

                if (Flags & 8)
                    Flagcount = *Points++;
            }
            else
                --Flagcount;
            Vertices[Off + I].Type = Flags;
        }

        X = 0;

        for (I = 0; I < N; ++I)
        {
            Flags = Vertices[Off + I].Type;

            if (Flags & 2)
            {
                INT16 Dx = *Points++;

                X += (Flags & 16) ? Dx : -Dx;
            }
            else
            {
                if (!(Flags & 16))
                {
                    X = X + (INT16) (Points[0] * 256 + Points[1]);
                    Points += 2;
                }
            }

            Vertices[Off + I].X = (INT16) X;
        }

        Y = 0;

        for (I = 0; I < N; ++I)
        {
            Flags = Vertices[Off + I].Type;

            if (Flags & 4)
            {
                INT16 Dy = *Points++;

                Y += (Flags & 32) ? Dy : -Dy;
            }
            else
            {
                if (!(Flags & 32))
                {
                    Y = Y + (INT16) (Points[0] * 256 + Points[1]);
                    Points += 2;
                }
            }

            Vertices[Off + I].Y = (INT16) Y;
        }

        NumVertices = 0;
        Sx = Sy = Cx = Cy = Scx = Scy = 0;

        for (I = 0; I < N; ++I)
        {
            Flags = Vertices[Off + I].Type;
            X = (INT16) Vertices[Off + I].X;
            Y = (INT16) Vertices[Off + I].Y;

            if (NextMove == I)
            {
                if (I != 0)
                    NumVertices = CloseShape(Vertices, NumVertices, WasOff, StartOff, Sx, Sy, Scx, Scy, Cx, Cy);
                StartOff = !(Flags & 1);

                if (StartOff)
                {
                    Scx = X;
                    Scy = Y;

                    if (!(Vertices[Off + I + 1].Type & 1))
                    {
                        Sx = (X + (INT32) Vertices[Off + I + 1].X) >> 1;
                        Sy = (Y + (INT32) Vertices[Off + I + 1].Y) >> 1;
                    }
                    else
                    {
                        Sx = (INT32) Vertices[Off + I + 1].X;
                        Sy = (INT32) Vertices[Off + I + 1].Y;
                        ++I;
                    }
                }
                else
                {
                    Sx = X;
                    Sy = Y;
                }

                Setvertex(&Vertices[NumVertices++], LINEOS_VMOVE, Sx, Sy, 0, 0);
                WasOff = 0;
                NextMove = 1 + TTUSHORT(EndPtsOfContours + J * 2);
                ++J;
            }
            else
            {
                if (!(Flags & 1))
                {
                    if (WasOff)
                        Setvertex(&Vertices[NumVertices++], LINEOS_VCURVE, (Cx + X) >> 1, (Cy + Y) >> 1, Cx, Cy);
                    Cx = X;
                    Cy = Y;
                    WasOff = 1;
                }
                else
                {
                    if (WasOff)
                        Setvertex(&Vertices[NumVertices++], LINEOS_VCURVE, X, Y, Cx, Cy);
                    else
                        Setvertex(&Vertices[NumVertices++], LINEOS_VLINE, X, Y, 0, 0);
                    WasOff = 0;
                }
            }
        }

        NumVertices = CloseShape(Vertices, NumVertices, WasOff, StartOff, Sx, Sy, Scx, Scy, Cx, Cy);
    }
    else if (NumberOfContours < 0)
    {
        INT32  More = 1;
        UINT8 *Comp = Data + G + 10;

        NumVertices = 0;
        Vertices = 0;

        while (More)
        {
            UINT16  Flags, Gidx;
            INT32   CompNumVerts = 0, I;
            VERTEX *CompVerts = 0, *Tmp = 0;
            FLOAT32 Mtx[6] = {1, 0, 0, 1, 0, 0}, M, N;

            Flags = TTSHORT(Comp);
            Comp += 2;
            Gidx = TTSHORT(Comp);
            Comp += 2;

            if (Flags & 2)
            {
                if (Flags & 1)
                {
                    Mtx[4] = TTSHORT(Comp);
                    Comp += 2;
                    Mtx[5] = TTSHORT(Comp);
                    Comp += 2;
                }
                else
                {
                    Mtx[4] = TT_CHAR(Comp);
                    Comp += 1;
                    Mtx[5] = TT_CHAR(Comp);
                    Comp += 1;
                }
            }
            else
            {
                KAssert(0);
            }

            if (Flags & (1 << 3))
            {
                Mtx[0] = Mtx[3] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
                Mtx[1] = Mtx[2] = 0;
            }
            else if (Flags & (1 << 6))
            {
                Mtx[0] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
                Mtx[1] = Mtx[2] = 0;
                Mtx[3] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
            }
            else if (Flags & (1 << 7))
            {
                Mtx[0] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
                Mtx[1] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
                Mtx[2] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
                Mtx[3] = TTSHORT(Comp) / 16384.0f;
                Comp += 2;
            }

            M = (FLOAT32) KSqrt(Mtx[0] * Mtx[0] + Mtx[1] * Mtx[1]);
            N = (FLOAT32) KSqrt(Mtx[2] * Mtx[2] + Mtx[3] * Mtx[3]);
            CompNumVerts = GetGlyphShape(Info, Gidx, &CompVerts);

            if (CompNumVerts > 0)
            {
                for (I = 0; I < CompNumVerts; ++I)
                {
                    VERTEX     *V = &CompVerts[I];
                    VERTEX_TYPE X, Y;

                    X = V->X;
                    Y = V->Y;
                    V->X = (VERTEX_TYPE) (M * (Mtx[0] * X + Mtx[2] * Y + Mtx[4]));
                    V->Y = (VERTEX_TYPE) (N * (Mtx[1] * X + Mtx[3] * Y + Mtx[5]));
                    X = V->Cx;
                    Y = V->Cy;
                    V->Cx = (VERTEX_TYPE) (M * (Mtx[0] * X + Mtx[2] * Y + Mtx[4]));
                    V->Cy = (VERTEX_TYPE) (N * (Mtx[1] * X + Mtx[3] * Y + Mtx[5]));
                }

                Tmp = (VERTEX *) KTTFAlloc((NumVertices + CompNumVerts) * sizeof(VERTEX), Info->UserData);

                if (!Tmp)
                {
                    if (Vertices)
                        KTTFFree(Vertices, Info->UserData);

                    if (CompVerts)
                        KTTFFree(CompVerts, Info->UserData);

                    return 0;
                }

                if (NumVertices > 0 && Vertices)
                    KMemCpy(Tmp, Vertices, NumVertices * sizeof(VERTEX));
                KMemCpy(Tmp + NumVertices, CompVerts, CompNumVerts * sizeof(VERTEX));

                if (Vertices)
                    KTTFFree(Vertices, Info->UserData);
                Vertices = Tmp;
                KTTFFree(CompVerts, Info->UserData);
                NumVertices += CompNumVerts;
            }

            More = Flags & (1 << 5);
        }
    }
    else
    {
    }

    *Pvertices = Vertices;

    return NumVertices;
}
typedef struct
{
    INT32   bounds;
    INT32   started;
    FLOAT32 FirstX, FirstY;
    FLOAT32 x, y;
    INT32   MinX, MaxX, MinY, MaxY;
    VERTEX *pvertices;
    INT32   NumVertices;
} CS_CONTEXT;
#define LINEOS_CSCTX_INIT(bounds) {bounds, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0}
STATIC VOID TrackVertex(CS_CONTEXT *C, INT32 X, INT32 Y)
{
    if (X > C->MaxX || !C->started)
        C->MaxX = X;

    if (Y > C->MaxY || !C->started)
        C->MaxY = Y;

    if (X < C->MinX || !C->started)
        C->MinX = X;

    if (Y < C->MinY || !C->started)
        C->MinY = Y;
    C->started = 1;
}
STATIC VOID CsctxV(CS_CONTEXT *C, UINT8 Type, INT32 X, INT32 Y, INT32 Cx, INT32 Cy, INT32 Cx1, INT32 Cy1)
{
    if (C->bounds)
    {
        TrackVertex(C, X, Y);

        if (Type == LINEOS_VCUBIC)
        {
            TrackVertex(C, Cx, Cy);
            TrackVertex(C, Cx1, Cy1);
        }
    }
    else
    {
        Setvertex(&C->pvertices[C->NumVertices], Type, X, Y, Cx, Cy);
        C->pvertices[C->NumVertices].Cx1 = (INT16) Cx1;
        C->pvertices[C->NumVertices].Cy1 = (INT16) Cy1;
    }

    C->NumVertices++;
}
STATIC VOID CsctxCloseShape(CS_CONTEXT *Ctx)
{
    if (Ctx->FirstX != Ctx->x || Ctx->FirstY != Ctx->y)
        CsctxV(Ctx, LINEOS_VLINE, (INT32) Ctx->FirstX, (INT32) Ctx->FirstY, 0, 0, 0, 0);
}
STATIC VOID CsctxRmoveTo(CS_CONTEXT *Ctx, FLOAT32 Dx, FLOAT32 Dy)
{
    CsctxCloseShape(Ctx);
    Ctx->FirstX = Ctx->x = Ctx->x + Dx;
    Ctx->FirstY = Ctx->y = Ctx->y + Dy;
    CsctxV(Ctx, LINEOS_VMOVE, (INT32) Ctx->x, (INT32) Ctx->y, 0, 0, 0, 0);
}
STATIC VOID CsctxRlineTo(CS_CONTEXT *Ctx, FLOAT32 Dx, FLOAT32 Dy)
{
    Ctx->x += Dx;
    Ctx->y += Dy;
    CsctxV(Ctx, LINEOS_VLINE, (INT32) Ctx->x, (INT32) Ctx->y, 0, 0, 0, 0);
}
STATIC VOID CsctxRccurveTo(CS_CONTEXT *Ctx, FLOAT32 Dx1, FLOAT32 Dy1, FLOAT32 Dx2, FLOAT32 Dy2, FLOAT32 Dx3, FLOAT32 Dy3)
{
    FLOAT32 Cx1 = Ctx->x + Dx1;
    FLOAT32 Cy1 = Ctx->y + Dy1;
    FLOAT32 Cx2 = Cx1 + Dx2;
    FLOAT32 Cy2 = Cy1 + Dy2;

    Ctx->x = Cx2 + Dx3;
    Ctx->y = Cy2 + Dy3;
    CsctxV(Ctx, LINEOS_VCUBIC, (INT32) Ctx->x, (INT32) Ctx->y, (INT32) Cx1, (INT32) Cy1, (INT32) Cx2, (INT32) Cy2);
}
STATIC BUFFER GetSubr(BUFFER Idx, INT32 N)
{
    INT32 Count = CffIndexCount(&Idx);
    INT32 Bias = 107;

    if (Count >= 33900)
        Bias = 32768;
    else if (Count >= 1240)
        Bias = 1131;
    N += Bias;

    if (N < 0 || N >= Count)
        return NewBuf(NULL, 0);

    return CffIndexGet(Idx, N);
}
STATIC BUFFER CidGetGlyphSubrs(CONST FONT_INFO *Info, INT32 GlyphIndex)
{
    BUFFER Fdselect = Info->FdSelect;
    INT32  Nranges, Start, End, V, Fmt, Fdselector = -1, I;

    BufSeek(&Fdselect, 0);
    Fmt = BufGet8(&Fdselect);

    if (Fmt == 0)
    {
        BufSkip(&Fdselect, GlyphIndex);
        Fdselector = BufGet8(&Fdselect);
    }
    else if (Fmt == 3)
    {
        Nranges = BUF_GET16(&Fdselect);
        Start = BUF_GET16(&Fdselect);

        for (I = 0; I < Nranges; I++)
        {
            V = BufGet8(&Fdselect);
            End = BUF_GET16(&Fdselect);

            if (GlyphIndex >= Start && GlyphIndex < End)
            {
                Fdselector = V;
                break;
            }

            Start = End;
        }
    }

    if (Fdselector == -1)
        NewBuf(NULL, 0);

    return GetSubrs(Info->CFF, CffIndexGet(Info->FontDicts, Fdselector));
}
STATIC INT32 RunCharstring(CONST FONT_INFO *Info, INT32 GlyphIndex, CS_CONTEXT *C)
{
    INT32   InHeader = 1, Maskbits = 0, SubrStackHeight = 0, Sp = 0, V, I, B0;
    INT32   HasSubrs = 0, ClearStack;
    FLOAT32 S[48];
    BUFFER  SubrStack[10], Subrs = Info->Subrs, B;
    FLOAT32 F;
#define LINEOS_CSERR(s) (0)

    B = CffIndexGet(Info->CharStrings, GlyphIndex);

    while (B.Cursor < B.Size)
    {
        I = 0;
        ClearStack = 1;
        B0 = BufGet8(&B);

        switch (B0)
        {
            case 0x13:
            case 0x14:
                if (InHeader)
                    Maskbits += (Sp / 2);
                InHeader = 0;
                BufSkip(&B, (Maskbits + 7) / 8);
                break;

            case 0X01:
            case 0x03:
            case 0X12:
            case 0x17:
                Maskbits += (Sp / 2);
                break;

            case 0x15:
                InHeader = 0;

                if (Sp < 2)
                    return LINEOS_CSERR("rmoveto stack");
                CsctxRmoveTo(C, S[Sp - 2], S[Sp - 1]);
                break;

            case 0x04:
                InHeader = 0;

                if (Sp < 1)
                    return LINEOS_CSERR("vmoveto stack");
                CsctxRmoveTo(C, 0, S[Sp - 1]);
                break;

            case 0x16:
                InHeader = 0;

                if (Sp < 1)
                    return LINEOS_CSERR("hmoveto stack");
                CsctxRmoveTo(C, S[Sp - 1], 0);
                break;

            case 0x05:
                if (Sp < 2)
                    return LINEOS_CSERR("rlineto stack");

                for (; I + 1 < Sp; I += 2)
                    CsctxRlineTo(C, S[I], S[I + 1]);
                break;

            case 0x07:
                if (Sp < 1)
                    return LINEOS_CSERR("vlineto stack");

                goto vlineto;

            case 0x06:
                if (Sp < 1)
                    return LINEOS_CSERR("hlineto stack");

                for (;;)
                {
                    if (I >= Sp)
                        break;
                    CsctxRlineTo(C, S[I], 0);
                    I++;
                vlineto:
                    if (I >= Sp)
                        break;
                    CsctxRlineTo(C, 0, S[I]);
                    I++;
                }

                break;

            case 0x1F:
                if (Sp < 4)
                    return LINEOS_CSERR("hvcurveto stack");

                goto hvcurveto;

            case 0x1E:
                if (Sp < 4)
                    return LINEOS_CSERR("vhcurveto stack");

                for (;;)
                {
                    if (I + 3 >= Sp)
                        break;
                    CsctxRccurveTo(C, 0, S[I], S[I + 1], S[I + 2], S[I + 3], (Sp - I == 5) ? S[I + 4] : 0.0f);
                    I += 4;
                hvcurveto:
                    if (I + 3 >= Sp)
                        break;
                    CsctxRccurveTo(C, S[I], 0, S[I + 1], S[I + 2], (Sp - I == 5) ? S[I + 4] : 0.0f, S[I + 3]);
                    I += 4;
                }

                break;

            case 0x08:
                if (Sp < 6)
                    return LINEOS_CSERR("rcurveline stack");

                for (; I + 5 < Sp; I += 6)
                    CsctxRccurveTo(C, S[I], S[I + 1], S[I + 2], S[I + 3], S[I + 4], S[I + 5]);
                break;

            case 0x18:
                if (Sp < 8)
                    return LINEOS_CSERR("rcurveline stack");

                for (; I + 5 < Sp - 2; I += 6)
                    CsctxRccurveTo(C, S[I], S[I + 1], S[I + 2], S[I + 3], S[I + 4], S[I + 5]);

                if (I + 1 >= Sp)
                    return LINEOS_CSERR("rcurveline stack");
                CsctxRlineTo(C, S[I], S[I + 1]);
                break;

            case 0x19:
                if (Sp < 8)
                    return LINEOS_CSERR("rlinecurve stack");

                for (; I + 1 < Sp - 6; I += 2)
                    CsctxRlineTo(C, S[I], S[I + 1]);

                if (I + 5 >= Sp)
                    return LINEOS_CSERR("rlinecurve stack");
                CsctxRccurveTo(C, S[I], S[I + 1], S[I + 2], S[I + 3], S[I + 4], S[I + 5]);
                break;

            case 0x1A:
            case 0x1B:
                if (Sp < 4)
                    return LINEOS_CSERR("(vv|hh)curveto stack");
                F = 0.0;

                if (Sp & 1)
                {
                    F = S[I];
                    I++;
                }

                for (; I + 3 < Sp; I += 4)
                {
                    if (B0 == 0x1B)
                        CsctxRccurveTo(C, S[I], F, S[I + 1], S[I + 2], S[I + 3], 0.0);
                    else
                        CsctxRccurveTo(C, F, S[I], S[I + 1], S[I + 2], 0.0, S[I + 3]);
                    F = 0.0;
                }

                break;

            case 0x0A:
                if (!HasSubrs)
                {
                    if (Info->FdSelect.Size)
                        Subrs = CidGetGlyphSubrs(Info, GlyphIndex);
                    HasSubrs = 1;
                }

            case 0x1D:
                if (Sp < 1)
                    return LINEOS_CSERR("call(g|)subr stack");
                V = (INT32) S[--Sp];

                if (SubrStackHeight >= 10)
                    return LINEOS_CSERR("recursion limit");
                SubrStack[SubrStackHeight++] = B;
                B = GetSubr(B0 == 0x0A ? Subrs : Info->GSubrs, V);

                if (B.Size == 0)
                    return LINEOS_CSERR("subr not found");
                B.Cursor = 0;
                ClearStack = 0;
                break;

            case 0x0B:
                if (SubrStackHeight <= 0)
                    return LINEOS_CSERR("return outside subr");
                B = SubrStack[--SubrStackHeight];
                ClearStack = 0;
                break;

            case 0x0E:
                CsctxCloseShape(C);

                return 1;

            case 0x0C:
            {
                FLOAT32 Dx1, Dx2, Dx3, Dx4, Dx5, Dx6, Dy1, Dy2, Dy3, Dy4, Dy5, Dy6;
                FLOAT32 Dx, Dy;
                INT32   B1 = BufGet8(&B);

                switch (B1)
                {
                    case 0x22:
                        if (Sp < 7)
                            return LINEOS_CSERR("hflex stack");
                        Dx1 = S[0];
                        Dx2 = S[1];
                        Dy2 = S[2];
                        Dx3 = S[3];
                        Dx4 = S[4];
                        Dx5 = S[5];
                        Dx6 = S[6];
                        CsctxRccurveTo(C, Dx1, 0, Dx2, Dy2, Dx3, 0);
                        CsctxRccurveTo(C, Dx4, 0, Dx5, -Dy2, Dx6, 0);
                        break;

                    case 0X23:
                        if (Sp < 13)
                            return LINEOS_CSERR("flex stack");
                        Dx1 = S[0];
                        Dy1 = S[1];
                        Dx2 = S[2];
                        Dy2 = S[3];
                        Dx3 = S[4];
                        Dy3 = S[5];
                        Dx4 = S[6];
                        Dy4 = S[7];
                        Dx5 = S[8];
                        Dy5 = S[9];
                        Dx6 = S[10];
                        Dy6 = S[11];
                        CsctxRccurveTo(C, Dx1, Dy1, Dx2, Dy2, Dx3, Dy3);
                        CsctxRccurveTo(C, Dx4, Dy4, Dx5, Dy5, Dx6, Dy6);
                        break;

                    case 0x24:
                        if (Sp < 9)
                            return LINEOS_CSERR("hflex1 stack");
                        Dx1 = S[0];
                        Dy1 = S[1];
                        Dx2 = S[2];
                        Dy2 = S[3];
                        Dx3 = S[4];
                        Dx4 = S[5];
                        Dx5 = S[6];
                        Dy5 = S[7];
                        Dx6 = S[8];
                        CsctxRccurveTo(C, Dx1, Dy1, Dx2, Dy2, Dx3, 0);
                        CsctxRccurveTo(C, Dx4, 0, Dx5, Dy5, Dx6, -(Dy1 + Dy2 + Dy5));
                        break;

                    case 0x25:
                        if (Sp < 11)
                            return LINEOS_CSERR("flex1 stack");
                        Dx1 = S[0];
                        Dy1 = S[1];
                        Dx2 = S[2];
                        Dy2 = S[3];
                        Dx3 = S[4];
                        Dy3 = S[5];
                        Dx4 = S[6];
                        Dy4 = S[7];
                        Dx5 = S[8];
                        Dy5 = S[9];
                        Dx6 = Dy6 = S[10];
                        Dx = Dx1 + Dx2 + Dx3 + Dx4 + Dx5;
                        Dy = Dy1 + Dy2 + Dy3 + Dy4 + Dy5;

                        if (KFAbs(Dx) > KFAbs(Dy))
                            Dy6 = -Dy;
                        else
                            Dx6 = -Dx;
                        CsctxRccurveTo(C, Dx1, Dy1, Dx2, Dy2, Dx3, Dy3);
                        CsctxRccurveTo(C, Dx4, Dy4, Dx5, Dy5, Dx6, Dy6);
                        break;

                    default:
                        return LINEOS_CSERR("unimplemented");
                }
            }

            break;

            default:
                if (B0 != 255 && B0 != 28 && B0 < 32)
                    return LINEOS_CSERR("reserved operator");

                if (B0 == 255)
                {
                    F = (FLOAT32) (INT32) BUF_GET32(&B) / 0x10000;
                }
                else
                {
                    BufSkip(&B, -1);
                    F = (FLOAT32) (INT16) CffInt(&B);
                }

                if (Sp >= 48)
                    return LINEOS_CSERR("push stack overflow");
                S[Sp++] = F;
                ClearStack = 0;
                break;
        }

        if (ClearStack)
            Sp = 0;
    }

    return LINEOS_CSERR("no endchar");
#undef LINEOS_CSERR
}
STATIC INT32 GetGlyphShapeT2(CONST FONT_INFO *Info, INT32 GlyphIndex, VERTEX **Pvertices)
{
    CS_CONTEXT CountCtx = LINEOS_CSCTX_INIT(1);
    CS_CONTEXT OutputCtx = LINEOS_CSCTX_INIT(0);

    if (RunCharstring(Info, GlyphIndex, &CountCtx))
    {
        *Pvertices = (VERTEX *) KTTFAlloc(CountCtx.NumVertices * sizeof(VERTEX), Info->UserData);
        OutputCtx.pvertices = *Pvertices;

        if (RunCharstring(Info, GlyphIndex, &OutputCtx))
        {
            KAssert(OutputCtx.NumVertices == CountCtx.NumVertices);

            return OutputCtx.NumVertices;
        }
    }

    *Pvertices = NULL;

    return 0;
}
STATIC INT32 GetGlyphInfoT2(CONST FONT_INFO *Info, INT32 GlyphIndex, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1)
{
    CS_CONTEXT C = LINEOS_CSCTX_INIT(1);
    INT32      R = RunCharstring(Info, GlyphIndex, &C);

    if (X0)
        *X0 = R ? C.MinX : 0;

    if (Y0)
        *Y0 = R ? C.MinY : 0;

    if (X1)
        *X1 = R ? C.MaxX : 0;

    if (Y1)
        *Y1 = R ? C.MaxY : 0;

    return R ? C.NumVertices : 0;
}
EXTERN INT32 GetGlyphShape(CONST FONT_INFO *Info, INT32 GlyphIndex, VERTEX **Pvertices)
{
    if (!Info->CFF.Size)
        return GetGlyphShapeTT(Info, GlyphIndex, Pvertices);
    else

        return GetGlyphShapeT2(Info, GlyphIndex, Pvertices);
}
EXTERN VOID GetGlyphHMetrics(CONST FONT_INFO *Info, INT32 GlyphIndex, INT32 *AdvanceWidth, INT32 *LeftSideBearing)
{
    UINT16 NumOfLongHorMetrics = TTUSHORT(Info->Data + Info->Hhea + 34);

    if (GlyphIndex < NumOfLongHorMetrics)
    {
        if (AdvanceWidth)
            *AdvanceWidth = TTSHORT(Info->Data + Info->Hmtx + 4 * GlyphIndex);

        if (LeftSideBearing)
            *LeftSideBearing = TTSHORT(Info->Data + Info->Hmtx + 4 * GlyphIndex + 2);
    }
    else
    {
        if (AdvanceWidth)
            *AdvanceWidth = TTSHORT(Info->Data + Info->Hmtx + 4 * (NumOfLongHorMetrics - 1));

        if (LeftSideBearing)
            *LeftSideBearing = TTSHORT(Info->Data + Info->Hmtx + 4 * NumOfLongHorMetrics + 2 * (GlyphIndex - NumOfLongHorMetrics));
    }
}
EXTERN INT32 GetKerningTableLength(CONST FONT_INFO *Info)
{
    UINT8 *Data = Info->Data + Info->Kern;

    if (!Info->Kern)
        return 0;

    if (TTUSHORT(Data + 2) < 1)
        return 0;

    if (TTUSHORT(Data + 8) != 1)
        return 0;

    return TTUSHORT(Data + 10);
}
EXTERN INT32 GetKerningTable(CONST FONT_INFO *Info, KERNING_ENTRY *Table, INT32 TableLength)
{
    UINT8 *Data = Info->Data + Info->Kern;
    INT32  K, Length;

    if (!Info->Kern)
        return 0;

    if (TTUSHORT(Data + 2) < 1)
        return 0;

    if (TTUSHORT(Data + 8) != 1)
        return 0;
    Length = TTUSHORT(Data + 10);

    if (TableLength < Length)
        Length = TableLength;

    for (K = 0; K < Length; K++)
    {
        Table[K].Glyph1 = TTUSHORT(Data + 18 + (K * 6));
        Table[K].Glyph2 = TTUSHORT(Data + 20 + (K * 6));
        Table[K].Advance = TTSHORT(Data + 22 + (K * 6));
    }

    return Length;
}
STATIC INT32 GetGlyphKernInfoAdvance(CONST FONT_INFO *Info, INT32 Glyph1, INT32 Glyph2)
{
    UINT8 *Data = Info->Data + Info->Kern;
    UINT32 Needle, Straw;
    INT32  L, R, M;

    if (!Info->Kern)
        return 0;

    if (TTUSHORT(Data + 2) < 1)
        return 0;

    if (TTUSHORT(Data + 8) != 1)
        return 0;
    L = 0;
    R = TTUSHORT(Data + 10) - 1;
    Needle = Glyph1 << 16 | Glyph2;

    while (L <= R)
    {
        M = (L + R) >> 1;
        Straw = TTULONG(Data + 18 + (M * 6));

        if (Needle < Straw)
            R = M - 1;
        else if (Needle > Straw)
            L = M + 1;
        else

            return TTSHORT(Data + 22 + (M * 6));
    }

    return 0;
}
STATIC INT32 GetCoverageIndex(UINT8 *CoverageTable, INT32 Glyph)
{
    UINT16 CoverageFormat = TTUSHORT(CoverageTable);

    switch (CoverageFormat)
    {
        case 1:
        {
            UINT16 GlyphCount = TTUSHORT(CoverageTable + 2);
            INT32  L = 0, R = GlyphCount - 1, M;
            INT32  Straw, Needle = Glyph;

            while (L <= R)
            {
                UINT8 *GlyphArray = CoverageTable + 4;
                UINT16 GlyphID;

                M = (L + R) >> 1;
                GlyphID = TTUSHORT(GlyphArray + 2 * M);
                Straw = GlyphID;

                if (Needle < Straw)
                    R = M - 1;
                else if (Needle > Straw)
                    L = M + 1;
                else
                {
                    return M;
                }
            }

            break;
        }

        case 2:
        {
            UINT16 RangeCount = TTUSHORT(CoverageTable + 2);
            UINT8 *RangeArray = CoverageTable + 4;
            INT32  L = 0, R = RangeCount - 1, M;
            INT32  StrawStart, StrawEnd, Needle = Glyph;

            while (L <= R)
            {
                UINT8 *RangeRecord;

                M = (L + R) >> 1;
                RangeRecord = RangeArray + 6 * M;
                StrawStart = TTUSHORT(RangeRecord);
                StrawEnd = TTUSHORT(RangeRecord + 2);

                if (Needle < StrawStart)
                    R = M - 1;
                else if (Needle > StrawEnd)
                    L = M + 1;
                else
                {
                    UINT16 StartCoverageIndex = TTUSHORT(RangeRecord + 4);

                    return StartCoverageIndex + Glyph - StrawStart;
                }
            }

            break;
        }

        default:
            return -1;
    }

    return -1;
}
STATIC INT32 GetGlyphClass(UINT8 *ClassDefTable, INT32 Glyph)
{
    UINT16 ClassDefFormat = TTUSHORT(ClassDefTable);

    switch (ClassDefFormat)
    {
        case 1:
        {
            UINT16 StartGlyphID = TTUSHORT(ClassDefTable + 2);
            UINT16 GlyphCount = TTUSHORT(ClassDefTable + 4);
            UINT8 *ClassDef1ValueArray = ClassDefTable + 6;

            if (Glyph >= StartGlyphID && Glyph < StartGlyphID + GlyphCount)
                return (INT32) TTUSHORT(ClassDef1ValueArray + 2 * (Glyph - StartGlyphID));
            break;
        }

        case 2:
        {
            UINT16 ClassRangeCount = TTUSHORT(ClassDefTable + 2);
            UINT8 *ClassRangeRecords = ClassDefTable + 4;
            INT32  L = 0, R = ClassRangeCount - 1, M;
            INT32  StrawStart, StrawEnd, Needle = Glyph;

            while (L <= R)
            {
                UINT8 *ClassRangeRecord;

                M = (L + R) >> 1;
                ClassRangeRecord = ClassRangeRecords + 6 * M;
                StrawStart = TTUSHORT(ClassRangeRecord);
                StrawEnd = TTUSHORT(ClassRangeRecord + 2);

                if (Needle < StrawStart)
                    R = M - 1;
                else if (Needle > StrawEnd)
                    L = M + 1;
                else

                    return (INT32) TTUSHORT(ClassRangeRecord + 4);
            }

            break;
        }

        default:
            return -1;
    }

    return 0;
}
#define LINEOS_GPOS_TODO_ASSERT(x)
STATIC INT32 GetGlyphGPOSInfoAdvance(CONST FONT_INFO *Info, INT32 Glyph1, INT32 Glyph2)
{
    UINT16 LookupListOffset;
    UINT8 *LookupList;
    UINT16 LookupCount;
    UINT8 *Data;
    INT32  I, Sti;

    if (!Info->GPOS)
        return 0;
    Data = Info->Data + Info->GPOS;

    if (TTUSHORT(Data + 0) != 1)
        return 0;

    if (TTUSHORT(Data + 2) != 0)
        return 0;
    LookupListOffset = TTUSHORT(Data + 8);
    LookupList = Data + LookupListOffset;
    LookupCount = TTUSHORT(LookupList);

    for (I = 0; I < LookupCount; ++I)
    {
        UINT16 LookupOffset = TTUSHORT(LookupList + 2 + 2 * I);
        UINT8 *LookupTable = LookupList + LookupOffset;
        UINT16 LookupType = TTUSHORT(LookupTable);
        UINT16 SubTableCount = TTUSHORT(LookupTable + 4);
        UINT8 *SubTableOffsets = LookupTable + 6;

        if (LookupType != 2)
            continue;

        for (Sti = 0; Sti < SubTableCount; Sti++)
        {
            UINT16 SubtableOffset = TTUSHORT(SubTableOffsets + 2 * Sti);
            UINT8 *Table = LookupTable + SubtableOffset;
            UINT16 PosFormat = TTUSHORT(Table);
            UINT16 CoverageOffset = TTUSHORT(Table + 2);
            INT32  CoverageIndex = GetCoverageIndex(Table + CoverageOffset, Glyph1);

            if (CoverageIndex == -1)
                continue;

            switch (PosFormat)
            {
                case 1:
                {
                    INT32  L, R, M;
                    INT32  Straw, Needle;
                    UINT16 ValueFormat1 = TTUSHORT(Table + 4);
                    UINT16 ValueFormat2 = TTUSHORT(Table + 6);

                    if (ValueFormat1 == 4 && ValueFormat2 == 0)
                    {
                        INT32  ValueRecordPairSizeInBytes = 2;
                        UINT16 PairSetCount = TTUSHORT(Table + 8);
                        UINT16 PairPosOffset = TTUSHORT(Table + 10 + 2 * CoverageIndex);
                        UINT8 *PairValueTable = Table + PairPosOffset;
                        UINT16 PairValueCount = TTUSHORT(PairValueTable);
                        UINT8 *PairValueArray = PairValueTable + 2;

                        if (CoverageIndex >= PairSetCount)
                            return 0;
                        Needle = Glyph2;
                        R = PairValueCount - 1;
                        L = 0;

                        while (L <= R)
                        {
                            UINT16 SecondGlyph;
                            UINT8 *PairValue;

                            M = (L + R) >> 1;
                            PairValue = PairValueArray + (2 + ValueRecordPairSizeInBytes) * M;
                            SecondGlyph = TTUSHORT(PairValue);
                            Straw = SecondGlyph;

                            if (Needle < Straw)
                                R = M - 1;
                            else if (Needle > Straw)
                                L = M + 1;
                            else
                            {
                                INT16 XAdvance = TTSHORT(PairValue + 2);

                                return XAdvance;
                            }
                        }
                    }
                    else

                        return 0;
                    break;
                }

                case 2:
                {
                    UINT16 ValueFormat1 = TTUSHORT(Table + 4);
                    UINT16 ValueFormat2 = TTUSHORT(Table + 6);

                    if (ValueFormat1 == 4 && ValueFormat2 == 0)
                    {
                        UINT16 ClassDef1Offset = TTUSHORT(Table + 8);
                        UINT16 ClassDef2Offset = TTUSHORT(Table + 10);
                        INT32  Glyph1Class = GetGlyphClass(Table + ClassDef1Offset, Glyph1);
                        INT32  Glyph2Class = GetGlyphClass(Table + ClassDef2Offset, Glyph2);
                        UINT16 Class1Count = TTUSHORT(Table + 12);
                        UINT16 Class2Count = TTUSHORT(Table + 14);
                        UINT8 *Class1Records, *Class2Records;
                        INT16  XAdvance;

                        if (Glyph1Class < 0 || Glyph1Class >= Class1Count)
                            return 0;

                        if (Glyph2Class < 0 || Glyph2Class >= Class2Count)
                            return 0;
                        Class1Records = Table + 16;
                        Class2Records = Class1Records + 2 * (Glyph1Class * Class2Count);
                        XAdvance = TTSHORT(Class2Records + 2 * Glyph2Class);

                        return XAdvance;
                    }
                    else

                        return 0;
                    break;
                }

                default:
                    return 0;
            }
        }
    }

    return 0;
}
EXTERN INT32 GetGlyphKernAdvance(CONST FONT_INFO *Info, INT32 G1, INT32 G2)
{
    INT32 XAdvance = 0;

    if (Info->GPOS)
        XAdvance += GetGlyphGPOSInfoAdvance(Info, G1, G2);
    else if (Info->Kern)
        XAdvance += GetGlyphKernInfoAdvance(Info, G1, G2);

    return XAdvance;
}
EXTERN INT32 GetCodepointKernAdvance(CONST FONT_INFO *Info, INT32 Ch1, INT32 Ch2)
{
    if (!Info->Kern && !Info->GPOS)
        return 0;

    return GetGlyphKernAdvance(Info, FindGlyphIndex(Info, Ch1), FindGlyphIndex(Info, Ch2));
}
EXTERN VOID GetCodepointHMetrics(CONST FONT_INFO *Info, INT32 Codepoint, INT32 *AdvanceWidth, INT32 *LeftSideBearing)
{
    GetGlyphHMetrics(Info, FindGlyphIndex(Info, Codepoint), AdvanceWidth, LeftSideBearing);
}
EXTERN VOID GetFontVMetrics(CONST FONT_INFO *Info, INT32 *Ascent, INT32 *Descent, INT32 *LineGap)
{
    if (Ascent)
        *Ascent = TTSHORT(Info->Data + Info->Hhea + 4);

    if (Descent)
        *Descent = TTSHORT(Info->Data + Info->Hhea + 6);

    if (LineGap)
        *LineGap = TTSHORT(Info->Data + Info->Hhea + 8);
}
EXTERN INT32 GetFontVMetricsOS2(CONST FONT_INFO *Info, INT32 *TypoAscent, INT32 *TypoDescent, INT32 *TypoLineGap)
{
    INT32 Tab = FindTable(Info->Data, Info->FontStart, "OS/2");

    if (!Tab)
        return 0;

    if (TypoAscent)
        *TypoAscent = TTSHORT(Info->Data + Tab + 68);

    if (TypoDescent)
        *TypoDescent = TTSHORT(Info->Data + Tab + 70);

    if (TypoLineGap)
        *TypoLineGap = TTSHORT(Info->Data + Tab + 72);

    return 1;
}
EXTERN VOID GetFontBoundingBox(CONST FONT_INFO *Info, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1)
{
    *X0 = TTSHORT(Info->Data + Info->Head + 36);
    *Y0 = TTSHORT(Info->Data + Info->Head + 38);
    *X1 = TTSHORT(Info->Data + Info->Head + 40);
    *Y1 = TTSHORT(Info->Data + Info->Head + 42);
}
EXTERN FLOAT32 ScaleForPixelHeight(CONST FONT_INFO *Info, FLOAT32 Height)
{
    INT32 Fheight = TTSHORT(Info->Data + Info->Hhea + 4) - TTSHORT(Info->Data + Info->Hhea + 6);

    return (FLOAT32) Height / Fheight;
}
EXTERN FLOAT32 ScaleForMappingEmToPixels(CONST FONT_INFO *Info, FLOAT32 Pixels)
{
    INT32 UnitsPerEm = TTUSHORT(Info->Data + Info->Head + 18);

    return Pixels / UnitsPerEm;
}
EXTERN VOID FreeShape(CONST FONT_INFO *Info, VERTEX *V)
{
    KTTFFree(V, Info->UserData);
}
EXTERN UINT8 *FindSVGDoc(CONST FONT_INFO *Info, INT32 Gl)
{
    INT32  I;
    UINT8 *Data = Info->Data;
    UINT8 *SVGDocList = Data + GetSVG((FONT_INFO *) Info);
    INT32  NumEntries = TTUSHORT(SVGDocList);
    UINT8 *SVGDocs = SVGDocList + 2;

    for (I = 0; I < NumEntries; I++)
    {
        UINT8 *SVGDoc = SVGDocs + (12 * I);

        if ((Gl >= TTUSHORT(SVGDoc)) && (Gl <= TTUSHORT(SVGDoc + 2)))
            return SVGDoc;
    }

    return 0;
}
EXTERN INT32 GetGlyphSVG(CONST FONT_INFO *Info, INT32 Gl, CONST CHAR8 **Svg)
{
    UINT8 *Data = Info->Data;
    UINT8 *SVGDoc;

    if (Info->Svg == 0)
        return 0;
    SVGDoc = FindSVGDoc(Info, Gl);

    if (SVGDoc != NULL)
    {
        *Svg = (CHAR8 *) Data + Info->Svg + TTULONG(SVGDoc + 4);

        return TTULONG(SVGDoc + 8);
    }
    else
    {
        return 0;
    }
}
EXTERN INT32 GetCodepointSVG(CONST FONT_INFO *Info, INT32 UnicodeCodepoint, CONST CHAR8 **Svg)
{
    return GetGlyphSVG(Info, FindGlyphIndex(Info, UnicodeCodepoint), Svg);
}
EXTERN VOID GetGlyphBitmapBoxSubpixel(CONST FONT_INFO *Font, INT32 Glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1)
{
    INT32 X0 = 0, Y0 = 0, X1, Y1;

    if (!GetGlyphBox(Font, Glyph, &X0, &Y0, &X1, &Y1))
    {
        if (Ix0)
            *Ix0 = 0;

        if (Iy0)
            *Iy0 = 0;

        if (Ix1)
            *Ix1 = 0;

        if (Iy1)
            *Iy1 = 0;
    }
    else
    {
        if (Ix0)
            *Ix0 = KFloor(X0 * ScaleX + ShiftX);

        if (Iy0)
            *Iy0 = KFloor(-Y1 * ScaleY + ShiftY);

        if (Ix1)
            *Ix1 = KCeil(X1 * ScaleX + ShiftX);

        if (Iy1)
            *Iy1 = KCeil(-Y0 * ScaleY + ShiftY);
    }
}
EXTERN VOID GetGlyphBitmapBox(CONST FONT_INFO *Font, INT32 Glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1)
{
    GetGlyphBitmapBoxSubpixel(Font, Glyph, ScaleX, ScaleY, 0.0f, 0.0f, Ix0, Iy0, Ix1, Iy1);
}
EXTERN VOID GetCodepointBitmapBoxSubpixel(CONST FONT_INFO *Font, INT32 Codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1)
{
    GetGlyphBitmapBoxSubpixel(Font, FindGlyphIndex(Font, Codepoint), ScaleX, ScaleY, ShiftX, ShiftY, Ix0, Iy0, Ix1, Iy1);
}
EXTERN VOID GetCodepointBitmapBox(CONST FONT_INFO *Font, INT32 Codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1)
{
    GetCodepointBitmapBoxSubpixel(Font, Codepoint, ScaleX, ScaleY, 0.0f, 0.0f, Ix0, Iy0, Ix1, Iy1);
}
typedef struct HHEAP_CHUNK
{
    struct HHEAP_CHUNK *next;
} HHEAP_CHUNK;
typedef struct HHEAP
{
    struct HHEAP_CHUNK *head;
    VOID               *FirstFree;
    INT32               NumRemainingInHeadChunk;
} HHEAP;
STATIC VOID *HheapAlloc(HHEAP *Hh, UINTN Size, VOID *Userdata)
{
    if (Hh->FirstFree)
    {
        VOID *P = Hh->FirstFree;

        Hh->FirstFree = *(VOID **) P;

        return P;
    }
    else
    {
        if (Hh->NumRemainingInHeadChunk == 0)
        {
            INT32        Count = (Size < 32 ? 2000 : Size < 128 ? 800 : 100);
            HHEAP_CHUNK *C = (HHEAP_CHUNK *) KTTFAlloc(sizeof(HHEAP_CHUNK) + Size * Count, Userdata);

            if (C == NULL)
                return NULL;
            C->next = Hh->head;
            Hh->head = C;
            Hh->NumRemainingInHeadChunk = Count;
        }

        --Hh->NumRemainingInHeadChunk;

        return (CHAR8 *) (Hh->head) + sizeof(HHEAP_CHUNK) + Size * Hh->NumRemainingInHeadChunk;
    }
}
STATIC VOID HheapFree(HHEAP *Hh, VOID *P)
{
    *(VOID **) P = Hh->FirstFree;
    Hh->FirstFree = P;
}
STATIC VOID HheapCleanup(HHEAP *Hh, VOID *Userdata)
{
    HHEAP_CHUNK *C = Hh->head;

    while (C)
    {
        HHEAP_CHUNK *N = C->next;

        KTTFFree(C, Userdata);
        C = N;
    }
}
typedef struct EDGE
{
    FLOAT32 x0, y0, x1, y1;
    INT32   invert;
} EDGE;
typedef struct ACTIVE_EDGE
{
    struct ACTIVE_EDGE *next;
    FLOAT32             fx, fdx, fdy;
    FLOAT32             direction;
    FLOAT32             sy;
    FLOAT32             ey;
} ACTIVE_EDGE;
STATIC ACTIVE_EDGE *NewActive(HHEAP *Hh, EDGE *E, INT32 OffX, FLOAT32 StartPoint, VOID *Userdata)
{
    ACTIVE_EDGE *Z = (ACTIVE_EDGE *) HheapAlloc(Hh, sizeof(*Z), Userdata);
    FLOAT32      Dxdy = (E->x1 - E->x0) / (E->y1 - E->y0);

    KAssert(Z != NULL);

    if (!Z)
        return Z;
    Z->fdx = Dxdy;
    Z->fdy = Dxdy != 0.0f ? (1.0f / Dxdy) : 0.0f;
    Z->fx = E->x0 + Dxdy * (StartPoint - E->y0);
    Z->fx -= OffX;
    Z->direction = E->invert ? 1.0f : -1.0f;
    Z->sy = E->y0;
    Z->ey = E->y1;
    Z->next = 0;

    return Z;
}
STATIC VOID HandleClippedEdge(FLOAT32 *Scanline, INT32 X, ACTIVE_EDGE *E, FLOAT32 X0, FLOAT32 Y0, FLOAT32 X1, FLOAT32 Y1)
{
    if (Y0 == Y1)
        return;
    KAssert(Y0 < Y1);
    KAssert(E->sy <= E->ey);

    if (Y0 > E->ey)
        return;

    if (Y1 < E->sy)
        return;

    if (Y0 < E->sy)
    {
        X0 += (X1 - X0) * (E->sy - Y0) / (Y1 - Y0);
        Y0 = E->sy;
    }

    if (Y1 > E->ey)
    {
        X1 += (X1 - X0) * (E->ey - Y1) / (Y1 - Y0);
        Y1 = E->ey;
    }

    if (X0 == X)
        KAssert(X1 <= X + 1);
    else if (X0 == X + 1)
        KAssert(X1 >= X);
    else if (X0 <= X)
        KAssert(X1 <= X);
    else if (X0 >= X + 1)
        KAssert(X1 >= X + 1);
    else
        KAssert(X1 >= X && X1 <= X + 1);

    if (X0 <= X && X1 <= X)
        Scanline[X] += E->direction * (Y1 - Y0);
    else if (X0 >= X + 1 && X1 >= X + 1)
        ;
    else
    {
        KAssert(X0 >= X && X0 <= X + 1 && X1 >= X && X1 <= X + 1);
        Scanline[X] += E->direction * (Y1 - Y0) * (1 - ((X0 - X) + (X1 - X)) / 2);
    }
}
STATIC FLOAT32 SizedTrapezoidArea(FLOAT32 Height, FLOAT32 TopWidth, FLOAT32 BottomWidth)
{
    KAssert(TopWidth >= 0);
    KAssert(BottomWidth >= 0);

    return (TopWidth + BottomWidth) / 2.0f * Height;
}
STATIC FLOAT32 PositionTrapezoidArea(FLOAT32 Height, FLOAT32 Tx0, FLOAT32 Tx1, FLOAT32 Bx0, FLOAT32 Bx1)
{
    return SizedTrapezoidArea(Height, Tx1 - Tx0, Bx1 - Bx0);
}
STATIC FLOAT32 SizedTriangleArea(FLOAT32 Height, FLOAT32 Width)
{
    return Height * Width / 2;
}
STATIC VOID FillActiveEdgesNew(FLOAT32 *Scanline, FLOAT32 *ScanlineFill, INT32 Len, ACTIVE_EDGE *E, FLOAT32 YTop)
{
    FLOAT32 YBottom = YTop + 1;

    while (E)
    {
        KAssert(E->ey >= YTop);

        if (E->fdx == 0)
        {
            FLOAT32 X0 = E->fx;

            if (X0 < Len)
            {
                if (X0 >= 0)
                {
                    HandleClippedEdge(Scanline, (INT32) X0, E, X0, YTop, X0, YBottom);
                    HandleClippedEdge(ScanlineFill - 1, (INT32) X0 + 1, E, X0, YTop, X0, YBottom);
                }
                else
                {
                    HandleClippedEdge(ScanlineFill - 1, 0, E, X0, YTop, X0, YBottom);
                }
            }
        }
        else
        {
            FLOAT32 X0 = E->fx;
            FLOAT32 Dx = E->fdx;
            FLOAT32 Xb = X0 + Dx;
            FLOAT32 XTop, XBottom;
            FLOAT32 Sy0, Sy1;
            FLOAT32 Dy = E->fdy;

            KAssert(E->sy <= YBottom && E->ey >= YTop);

            if (E->sy > YTop)
            {
                XTop = X0 + Dx * (E->sy - YTop);
                Sy0 = E->sy;
            }
            else
            {
                XTop = X0;
                Sy0 = YTop;
            }

            if (E->ey < YBottom)
            {
                XBottom = X0 + Dx * (E->ey - YTop);
                Sy1 = E->ey;
            }
            else
            {
                XBottom = Xb;
                Sy1 = YBottom;
            }

            if (XTop >= 0 && XBottom >= 0 && XTop < Len && XBottom < Len)
            {
                if ((INT32) XTop == (INT32) XBottom)
                {
                    FLOAT32 Height;
                    INT32   X = (INT32) XTop;

                    Height = (Sy1 - Sy0) * E->direction;
                    KAssert(X >= 0 && X < Len);
                    Scanline[X] += PositionTrapezoidArea(Height, XTop, X + 1.0f, XBottom, X + 1.0f);
                    ScanlineFill[X] += Height;
                }
                else
                {
                    INT32   X, X1, X2;
                    FLOAT32 YCrossing, YFinal, Step, Sign, Area;

                    if (XTop > XBottom)
                    {
                        FLOAT32 T;

                        Sy0 = YBottom - (Sy0 - YTop);
                        Sy1 = YBottom - (Sy1 - YTop);
                        T = Sy0, Sy0 = Sy1, Sy1 = T;
                        T = XBottom, XBottom = XTop, XTop = T;
                        Dx = -Dx;
                        Dy = -Dy;
                        T = X0, X0 = Xb, Xb = T;
                    }

                    KAssert(Dy >= 0);
                    KAssert(Dx >= 0);
                    X1 = (INT32) XTop;
                    X2 = (INT32) XBottom;
                    YCrossing = YTop + Dy * (X1 + 1 - X0);
                    YFinal = YTop + Dy * (X2 - X0);

                    if (YCrossing > YBottom)
                        YCrossing = YBottom;
                    Sign = E->direction;
                    Area = Sign * (YCrossing - Sy0);
                    Scanline[X1] += SizedTriangleArea(Area, X1 + 1 - XTop);

                    if (YFinal > YBottom)
                    {
                        YFinal = YBottom;
                        Dy = (YFinal - YCrossing) / (X2 - (X1 + 1));
                    }

                    Step = Sign * Dy * 1;

                    for (X = X1 + 1; X < X2; ++X)
                    {
                        Scanline[X] += Area + Step / 2;
                        Area += Step;
                    }

                    KAssert(KFAbs(Area) <= 1.01f);
                    KAssert(Sy1 > YFinal - 0.01f);
                    Scanline[X2] += Area + Sign * PositionTrapezoidArea(Sy1 - YFinal, (FLOAT32) X2, X2 + 1.0f, XBottom, X2 + 1.0f);
                    ScanlineFill[X2] += Sign * (Sy1 - Sy0);
                }
            }
            else
            {
                INT32 X;

                for (X = 0; X < Len; ++X)
                {
                    FLOAT32 Y0 = YTop;
                    FLOAT32 X1 = (FLOAT32) (X);
                    FLOAT32 X2 = (FLOAT32) (X + 1);
                    FLOAT32 X3 = Xb;
                    FLOAT32 Y3 = YBottom;
                    FLOAT32 Y1 = (X - X0) / Dx + YTop;
                    FLOAT32 Y2 = (X + 1 - X0) / Dx + YTop;

                    if (X0 < X1 && X3 > X2)
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X1, Y1);
                        HandleClippedEdge(Scanline, X, E, X1, Y1, X2, Y2);
                        HandleClippedEdge(Scanline, X, E, X2, Y2, X3, Y3);
                    }
                    else if (X3 < X1 && X0 > X2)
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X2, Y2);
                        HandleClippedEdge(Scanline, X, E, X2, Y2, X1, Y1);
                        HandleClippedEdge(Scanline, X, E, X1, Y1, X3, Y3);
                    }
                    else if (X0 < X1 && X3 > X1)
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X1, Y1);
                        HandleClippedEdge(Scanline, X, E, X1, Y1, X3, Y3);
                    }
                    else if (X3 < X1 && X0 > X1)
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X1, Y1);
                        HandleClippedEdge(Scanline, X, E, X1, Y1, X3, Y3);
                    }
                    else if (X0 < X2 && X3 > X2)
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X2, Y2);
                        HandleClippedEdge(Scanline, X, E, X2, Y2, X3, Y3);
                    }
                    else if (X3 < X2 && X0 > X2)
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X2, Y2);
                        HandleClippedEdge(Scanline, X, E, X2, Y2, X3, Y3);
                    }
                    else
                    {
                        HandleClippedEdge(Scanline, X, E, X0, Y0, X3, Y3);
                    }
                }
            }
        }

        E = E->next;
    }
}
STATIC VOID RasterizeSortedEdges(BITMAP *Result, EDGE *E, INT32 N, INT32 Vsubsample, INT32 OffX, INT32 OffY, VOID *Userdata)
{
    HHEAP        Hh = {0, 0, 0};
    ACTIVE_EDGE *Active = NULL;
    INT32        Y, J = 0, I;
    FLOAT32      ScanlineData[129], *Scanline, *Scanline2;

    LINEOS_NOTUSED(Vsubsample);

    if (Result->W > 64)
        Scanline = (FLOAT32 *) KTTFAlloc((Result->W * 2 + 1) * sizeof(FLOAT32), Userdata);
    else
        Scanline = ScanlineData;
    Scanline2 = Scanline + Result->W;
    Y = OffY;
    E[N].y0 = (FLOAT32) (OffY + Result->H) + 1;

    while (J < Result->H)
    {
        FLOAT32       ScanYTop = Y + 0.0f;
        FLOAT32       ScanYBottom = Y + 1.0f;
        ACTIVE_EDGE **Step = &Active;

        KMemSet(Scanline, 0, Result->W * sizeof(Scanline[0]));
        KMemSet(Scanline2, 0, (Result->W + 1) * sizeof(Scanline[0]));

        while (*Step)
        {
            ACTIVE_EDGE *Z = *Step;

            if (Z->ey <= ScanYTop)
            {
                *Step = Z->next;
                KAssert(Z->direction);
                Z->direction = 0;
                HheapFree(&Hh, Z);
            }
            else
            {
                Step = &((*Step)->next);
            }
        }

        while (E->y0 <= ScanYBottom)
        {
            if (E->y0 != E->y1)
            {
                ACTIVE_EDGE *Z = NewActive(&Hh, E, OffX, ScanYTop, Userdata);

                if (Z != NULL)
                {
                    if (J == 0 && OffY != 0)
                    {
                        if (Z->ey < ScanYTop)
                        {
                            Z->ey = ScanYTop;
                        }
                    }

                    KAssert(Z->ey >= ScanYTop);
                    Z->next = Active;
                    Active = Z;
                }
            }

            ++E;
        }

        if (Active)
            FillActiveEdgesNew(Scanline, Scanline2 + 1, Result->W, Active, ScanYTop);
        {
            FLOAT32 Sum = 0;

            for (I = 0; I < Result->W; ++I)
            {
                FLOAT32 K;
                INT32   M;

                Sum += Scanline2[I];
                K = Scanline[I] + Sum;
                K = (FLOAT32) KFAbs(K) * 255 + 0.5f;
                M = (INT32) K;

                if (M > 255)
                    M = 255;
                Result->Pixels[J * Result->Stride + I] = (UINT8) M;
            }
        }

        Step = &Active;

        while (*Step)
        {
            ACTIVE_EDGE *Z = *Step;

            Z->fx += Z->fdx;
            Step = &((*Step)->next);
        }

        ++Y;
        ++J;
    }

    HheapCleanup(&Hh, Userdata);

    if (Scanline != ScanlineData)
        KTTFFree(Scanline, Userdata);
}
#define LINEOS_COMPARE(a, b) ((a)->y0 < (b)->y0)
STATIC VOID SortEdgesInsSort(EDGE *P, INT32 N)
{
    INT32 I, J;

    for (I = 1; I < N; ++I)
    {
        EDGE T = P[I], *A = &T;

        J = I;

        while (J > 0)
        {
            EDGE *B = &P[J - 1];
            INT32 C = LINEOS_COMPARE(A, B);

            if (!C)
                break;
            P[J] = P[J - 1];
            --J;
        }

        if (I != J)
            P[J] = T;
    }
}
STATIC VOID SortEdgesQuicksort(EDGE *P, INT32 N)
{
    while (N > 12)
    {
        EDGE  T;
        INT32 C01, C12, C, M, I, J;

        M = N >> 1;
        C01 = LINEOS_COMPARE(&P[0], &P[M]);
        C12 = LINEOS_COMPARE(&P[M], &P[N - 1]);

        if (C01 != C12)
        {
            INT32 Z;

            C = LINEOS_COMPARE(&P[0], &P[N - 1]);
            Z = (C == C12) ? 0 : N - 1;
            T = P[Z];
            P[Z] = P[M];
            P[M] = T;
        }

        T = P[0];
        P[0] = P[M];
        P[M] = T;
        I = 1;
        J = N - 1;

        for (;;)
        {
            for (;; ++I)
            {
                if (!LINEOS_COMPARE(&P[I], &P[0]))
                    break;
            }

            for (;; --J)
            {
                if (!LINEOS_COMPARE(&P[0], &P[J]))
                    break;
            }

            if (I >= J)
                break;
            T = P[I];
            P[I] = P[J];
            P[J] = T;
            ++I;
            --J;
        }

        if (J < (N - I))
        {
            SortEdgesQuicksort(P, J);
            P = P + I;
            N = N - I;
        }
        else
        {
            SortEdgesQuicksort(P + I, N - I);
            N = J;
        }
    }
}
STATIC VOID SortEdges(EDGE *P, INT32 N)
{
    SortEdgesQuicksort(P, N);
    SortEdgesInsSort(P, N);
}
typedef struct
{
    FLOAT32 x, y;
} POINT;
STATIC VOID RasterizeInternal(BITMAP *Result, POINT *Pts, INT32 *Wcount, INT32 Windings, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OffX, INT32 OffY, INT32 Invert, VOID *Userdata)
{
    FLOAT32 YScaleInv = Invert ? -ScaleY : ScaleY;
    EDGE   *E;
    INT32   N, I, J, K, M;
    INT32   Vsubsample = 1;

    N = 0;

    for (I = 0; I < Windings; ++I)
        N += Wcount[I];
    E = (EDGE *) KTTFAlloc(sizeof(*E) * (N + 1), Userdata);

    if (E == 0)
        return;
    N = 0;
    M = 0;

    for (I = 0; I < Windings; ++I)
    {
        POINT *P = Pts + M;

        M += Wcount[I];
        J = Wcount[I] - 1;

        for (K = 0; K < Wcount[I]; J = K++)
        {
            INT32 A = K, B = J;

            if (P[J].y == P[K].y)
                continue;
            E[N].invert = 0;

            if (Invert ? P[J].y > P[K].y : P[J].y < P[K].y)
            {
                E[N].invert = 1;
                A = J, B = K;
            }

            E[N].x0 = P[A].x * ScaleX + ShiftX;
            E[N].y0 = (P[A].y * YScaleInv + ShiftY) * Vsubsample;
            E[N].x1 = P[B].x * ScaleX + ShiftX;
            E[N].y1 = (P[B].y * YScaleInv + ShiftY) * Vsubsample;
            ++N;
        }
    }

    SortEdges(E, N);
    RasterizeSortedEdges(Result, E, N, Vsubsample, OffX, OffY, Userdata);
    KTTFFree(E, Userdata);
}
STATIC VOID AddPoint(POINT *Points, INT32 N, FLOAT32 X, FLOAT32 Y)
{
    if (!Points)
        return;
    Points[N].x = X;
    Points[N].y = Y;
}
STATIC INT32 TesselateCurve(POINT *Points, INT32 *NumPoints, FLOAT32 X0, FLOAT32 Y0, FLOAT32 X1, FLOAT32 Y1, FLOAT32 X2, FLOAT32 Y2, FLOAT32 ObjspaceFlatnessSquared, INT32 N)
{
    FLOAT32 Mx = (X0 + 2 * X1 + X2) / 4;
    FLOAT32 My = (Y0 + 2 * Y1 + Y2) / 4;
    FLOAT32 Dx = (X0 + X2) / 2 - Mx;
    FLOAT32 Dy = (Y0 + Y2) / 2 - My;

    if (N > 16)
        return 1;

    if (Dx * Dx + Dy * Dy > ObjspaceFlatnessSquared)
    {
        TesselateCurve(Points, NumPoints, X0, Y0, (X0 + X1) / 2.0f, (Y0 + Y1) / 2.0f, Mx, My, ObjspaceFlatnessSquared, N + 1);
        TesselateCurve(Points, NumPoints, Mx, My, (X1 + X2) / 2.0f, (Y1 + Y2) / 2.0f, X2, Y2, ObjspaceFlatnessSquared, N + 1);
    }
    else
    {
        AddPoint(Points, *NumPoints, X2, Y2);
        *NumPoints = *NumPoints + 1;
    }

    return 1;
}
STATIC VOID TesselateCubic(POINT *Points, INT32 *NumPoints, FLOAT32 X0, FLOAT32 Y0, FLOAT32 X1, FLOAT32 Y1, FLOAT32 X2, FLOAT32 Y2, FLOAT32 X3, FLOAT32 Y3, FLOAT32 ObjspaceFlatnessSquared, INT32 N)
{
    FLOAT32 Dx0 = X1 - X0;
    FLOAT32 Dy0 = Y1 - Y0;
    FLOAT32 Dx1 = X2 - X1;
    FLOAT32 Dy1 = Y2 - Y1;
    FLOAT32 Dx2 = X3 - X2;
    FLOAT32 Dy2 = Y3 - Y2;
    FLOAT32 Dx = X3 - X0;
    FLOAT32 Dy = Y3 - Y0;
    FLOAT32 Longlen = (FLOAT32) (KSqrt(Dx0 * Dx0 + Dy0 * Dy0) + KSqrt(Dx1 * Dx1 + Dy1 * Dy1) + KSqrt(Dx2 * Dx2 + Dy2 * Dy2));
    FLOAT32 Shortlen = (FLOAT32) KSqrt(Dx * Dx + Dy * Dy);
    FLOAT32 FlatnessSquared = Longlen * Longlen - Shortlen * Shortlen;

    if (N > 16)
        return;

    if (FlatnessSquared > ObjspaceFlatnessSquared)
    {
        FLOAT32 X01 = (X0 + X1) / 2;
        FLOAT32 Y01 = (Y0 + Y1) / 2;
        FLOAT32 X12 = (X1 + X2) / 2;
        FLOAT32 Y12 = (Y1 + Y2) / 2;
        FLOAT32 X23 = (X2 + X3) / 2;
        FLOAT32 Y23 = (Y2 + Y3) / 2;
        FLOAT32 Xa = (X01 + X12) / 2;
        FLOAT32 Ya = (Y01 + Y12) / 2;
        FLOAT32 Xb = (X12 + X23) / 2;
        FLOAT32 Yb = (Y12 + Y23) / 2;
        FLOAT32 Mx = (Xa + Xb) / 2;
        FLOAT32 My = (Ya + Yb) / 2;

        TesselateCubic(Points, NumPoints, X0, Y0, X01, Y01, Xa, Ya, Mx, My, ObjspaceFlatnessSquared, N + 1);
        TesselateCubic(Points, NumPoints, Mx, My, Xb, Yb, X23, Y23, X3, Y3, ObjspaceFlatnessSquared, N + 1);
    }
    else
    {
        AddPoint(Points, *NumPoints, X3, Y3);
        *NumPoints = *NumPoints + 1;
    }
}
STATIC POINT *FlattenCurves(VERTEX *Vertices, INT32 NumVerts, FLOAT32 ObjspaceFlatness, INT32 **ContourLengths, INT32 *NumContours, VOID *Userdata)
{
    POINT  *Points = 0;
    INT32   NumPoints = 0;
    FLOAT32 ObjspaceFlatnessSquared = ObjspaceFlatness * ObjspaceFlatness;
    INT32   I, N = 0, Start = 0, Pass;

    for (I = 0; I < NumVerts; ++I)
        if (Vertices[I].Type == LINEOS_VMOVE)
            ++N;
    *NumContours = N;

    if (N == 0)
        return 0;
    *ContourLengths = (INT32 *) KTTFAlloc(sizeof(**ContourLengths) * N, Userdata);

    if (*ContourLengths == 0)
    {
        *NumContours = 0;

        return 0;
    }

    for (Pass = 0; Pass < 2; ++Pass)
    {
        FLOAT32 X = 0, Y = 0;

        if (Pass == 1)
        {
            Points = (POINT *) KTTFAlloc(NumPoints * sizeof(Points[0]), Userdata);

            if (Points == NULL)
                goto error;
        }

        NumPoints = 0;
        N = -1;

        for (I = 0; I < NumVerts; ++I)
        {
            switch (Vertices[I].Type)
            {
                case LINEOS_VMOVE:
                    if (N >= 0)
                        (*ContourLengths)[N] = NumPoints - Start;
                    ++N;
                    Start = NumPoints;
                    X = Vertices[I].X, Y = Vertices[I].Y;
                    AddPoint(Points, NumPoints++, X, Y);
                    break;

                case LINEOS_VLINE:
                    X = Vertices[I].X, Y = Vertices[I].Y;
                    AddPoint(Points, NumPoints++, X, Y);
                    break;

                case LINEOS_VCURVE:
                    TesselateCurve(Points, &NumPoints, X, Y, Vertices[I].Cx, Vertices[I].Cy, Vertices[I].X, Vertices[I].Y, ObjspaceFlatnessSquared, 0);
                    X = Vertices[I].X, Y = Vertices[I].Y;
                    break;

                case LINEOS_VCUBIC:
                    TesselateCubic(Points, &NumPoints, X, Y, Vertices[I].Cx, Vertices[I].Cy, Vertices[I].Cx1, Vertices[I].Cy1, Vertices[I].X, Vertices[I].Y, ObjspaceFlatnessSquared, 0);
                    X = Vertices[I].X, Y = Vertices[I].Y;
                    break;
            }
        }

        (*ContourLengths)[N] = NumPoints - Start;
    }

    return Points;
error:
    KTTFFree(Points, Userdata);
    KTTFFree(*ContourLengths, Userdata);
    *ContourLengths = 0;
    *NumContours = 0;

    return NULL;
}
EXTERN VOID Rasterize(BITMAP *Result, FLOAT32 FlatnessInPixels, VERTEX *Vertices, INT32 NumVerts, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 XOff, INT32 YOff, INT32 Invert, VOID *Userdata)
{
    FLOAT32 Scale = ScaleX > ScaleY ? ScaleY : ScaleX;
    INT32   WindingCount = 0;
    INT32  *WindingLengths = NULL;
    POINT  *Windings = FlattenCurves(Vertices, NumVerts, FlatnessInPixels / Scale, &WindingLengths, &WindingCount, Userdata);

    if (Windings)
    {
        RasterizeInternal(Result, Windings, WindingLengths, WindingCount, ScaleX, ScaleY, ShiftX, ShiftY, XOff, YOff, Invert, Userdata);
        KTTFFree(WindingLengths, Userdata);
        KTTFFree(Windings, Userdata);
    }
}
EXTERN VOID FreeBitmap(UINT8 *Bitmap, VOID *Userdata)
{
    KTTFFree(Bitmap, Userdata);
}
EXTERN UINT8 *GetGlyphBitmapSubpixel(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Glyph, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff)
{
    INT32   Ix0, Iy0, Ix1, Iy1;
    BITMAP  Gbm;
    VERTEX *Vertices;
    INT32   NumVerts = GetGlyphShape(Info, Glyph, &Vertices);

    if (ScaleX == 0)
        ScaleX = ScaleY;

    if (ScaleY == 0)
    {
        if (ScaleX == 0)
        {
            KTTFFree(Vertices, Info->UserData);

            return NULL;
        }

        ScaleY = ScaleX;
    }

    GetGlyphBitmapBoxSubpixel(Info, Glyph, ScaleX, ScaleY, ShiftX, ShiftY, &Ix0, &Iy0, &Ix1, &Iy1);
    Gbm.W = (Ix1 - Ix0);
    Gbm.H = (Iy1 - Iy0);
    Gbm.Pixels = NULL;

    if (Width)
        *Width = Gbm.W;

    if (Height)
        *Height = Gbm.H;

    if (Xoff)
        *Xoff = Ix0;

    if (Yoff)
        *Yoff = Iy0;

    if (Gbm.W && Gbm.H)
    {
        Gbm.Pixels = (UINT8 *) KTTFAlloc(Gbm.W * Gbm.H, Info->UserData);

        if (Gbm.Pixels)
        {
            Gbm.Stride = Gbm.W;
            Rasterize(&Gbm, 0.35f, Vertices, NumVerts, ScaleX, ScaleY, ShiftX, ShiftY, Ix0, Iy0, 1, Info->UserData);
        }
    }

    KTTFFree(Vertices, Info->UserData);

    return Gbm.Pixels;
}
EXTERN UINT8 *GetGlyphBitmap(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Glyph, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff)
{
    return GetGlyphBitmapSubpixel(Info, ScaleX, ScaleY, 0.0f, 0.0f, Glyph, Width, Height, Xoff, Yoff);
}
EXTERN VOID MakeGlyphBitmapSubpixel(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Glyph)
{
    INT32   Ix0, Iy0;
    VERTEX *Vertices;
    INT32   NumVerts = GetGlyphShape(Info, Glyph, &Vertices);
    BITMAP  Gbm;

    GetGlyphBitmapBoxSubpixel(Info, Glyph, ScaleX, ScaleY, ShiftX, ShiftY, &Ix0, &Iy0, 0, 0);
    Gbm.Pixels = Output;
    Gbm.W = OutW;
    Gbm.H = OutH;
    Gbm.Stride = OutStride;

    if (Gbm.W && Gbm.H)
        Rasterize(&Gbm, 0.35f, Vertices, NumVerts, ScaleX, ScaleY, ShiftX, ShiftY, Ix0, Iy0, 1, Info->UserData);
    KTTFFree(Vertices, Info->UserData);
}
EXTERN VOID MakeGlyphBitmap(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Glyph)
{
    MakeGlyphBitmapSubpixel(Info, Output, OutW, OutH, OutStride, ScaleX, ScaleY, 0.0f, 0.0f, Glyph);
}
EXTERN UINT8 *GetCodepointBitmapSubpixel(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Codepoint, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff)
{
    return GetGlyphBitmapSubpixel(Info, ScaleX, ScaleY, ShiftX, ShiftY, FindGlyphIndex(Info, Codepoint), Width, Height, Xoff, Yoff);
}
EXTERN VOID MakeCodepointBitmapSubpixelPrefilter(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OversampleX, INT32 OversampleY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 Codepoint)
{
    MakeGlyphBitmapSubpixelPrefilter(Info, Output, OutW, OutH, OutStride, ScaleX, ScaleY, ShiftX, ShiftY, OversampleX, OversampleY, SubX, SubY, FindGlyphIndex(Info, Codepoint));
}
EXTERN VOID MakeCodepointBitmapSubpixel(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Codepoint)
{
    MakeGlyphBitmapSubpixel(Info, Output, OutW, OutH, OutStride, ScaleX, ScaleY, ShiftX, ShiftY, FindGlyphIndex(Info, Codepoint));
}
EXTERN UINT8 *GetCodepointBitmap(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Codepoint, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff)
{
    return GetCodepointBitmapSubpixel(Info, ScaleX, ScaleY, 0.0f, 0.0f, Codepoint, Width, Height, Xoff, Yoff);
}
EXTERN VOID MakeCodepointBitmap(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Codepoint)
{
    MakeCodepointBitmapSubpixel(Info, Output, OutW, OutH, OutStride, ScaleX, ScaleY, 0.0f, 0.0f, Codepoint);
}
STATIC INT32 BakeFontBitmapInternal(UINT8 *Data, INT32 Offset, FLOAT32 PixelHeight, UINT8 *Pixels, INT32 Pw, INT32 Ph, INT32 FirstChar, INT32 NumChars, BAKED_CHAR *Chardata)
{
    FLOAT32   Scale;
    INT32     X, Y, BottomY, I;
    FONT_INFO F;

    F.UserData = NULL;

    if (!InitFont(&F, Data, Offset))
        return -1;
    KMemSet(Pixels, 0, Pw * Ph);
    X = Y = 1;
    BottomY = 1;
    Scale = ScaleForPixelHeight(&F, PixelHeight);

    for (I = 0; I < NumChars; ++I)
    {
        INT32 Advance, Lsb, X0, Y0, X1, Y1, Gw, Gh;
        INT32 G = FindGlyphIndex(&F, FirstChar + I);

        GetGlyphHMetrics(&F, G, &Advance, &Lsb);
        GetGlyphBitmapBox(&F, G, Scale, Scale, &X0, &Y0, &X1, &Y1);
        Gw = X1 - X0;
        Gh = Y1 - Y0;

        if (X + Gw + 1 >= Pw)
            Y = BottomY, X = 1;

        if (Y + Gh + 1 >= Ph)
            return -I;
        KAssert(X + Gw < Pw);
        KAssert(Y + Gh < Ph);
        MakeGlyphBitmap(&F, Pixels + X + Y * Pw, Gw, Gh, Pw, Scale, Scale, G);
        Chardata[I].X0 = (INT16) X;
        Chardata[I].Y0 = (INT16) Y;
        Chardata[I].X1 = (INT16) (X + Gw);
        Chardata[I].Y1 = (INT16) (Y + Gh);
        Chardata[I].Xadvance = Scale * Advance;
        Chardata[I].Xoff = (FLOAT32) X0;
        Chardata[I].Yoff = (FLOAT32) Y0;
        X = X + Gw + 1;

        if (Y + Gh + 1 > BottomY)
            BottomY = Y + Gh + 1;
    }

    return BottomY;
}
EXTERN VOID GetBakedQuad(CONST BAKED_CHAR *Chardata, INT32 Pw, INT32 Ph, INT32 CharIndex, FLOAT32 *Xpos, FLOAT32 *Ypos, ALIGNED_QUAD *Q, INT32 OpenglFillrule)
{
    FLOAT32           D3DBias = OpenglFillrule ? 0 : -0.5f;
    FLOAT32           Ipw = 1.0f / Pw, Iph = 1.0f / Ph;
    CONST BAKED_CHAR *B = Chardata + CharIndex;
    INT32             RoundX = KFloor((*Xpos + B->Xoff) + 0.5f);
    INT32             RoundY = KFloor((*Ypos + B->Yoff) + 0.5f);

    Q->X0 = RoundX + D3DBias;
    Q->Y0 = RoundY + D3DBias;
    Q->X1 = RoundX + B->X1 - B->X0 + D3DBias;
    Q->Y1 = RoundY + B->Y1 - B->Y0 + D3DBias;
    Q->S0 = B->X0 * Ipw;
    Q->T0 = B->Y0 * Iph;
    Q->S1 = B->X1 * Ipw;
    Q->T1 = B->Y1 * Iph;
    *Xpos += B->Xadvance;
}
typedef INT32 STBRP_COORD;
typedef struct
{
    INT32 width, height;
    INT32 x, y, BottomY;
} STBRP_CONTEXT;
typedef struct
{
    UINT8 x;
} STBRP_NODE;
struct STBRP_RECT
{
    STBRP_COORD x, y;
    INT32       id, w, h, WasPacked;
};
STATIC VOID STBRPInitTarget(STBRP_CONTEXT *Con, INT32 Pw, INT32 Ph, STBRP_NODE *Nodes, INT32 NumNodes)
{
    Con->width = Pw;
    Con->height = Ph;
    Con->x = 0;
    Con->y = 0;
    Con->BottomY = 0;
    LINEOS_NOTUSED(Nodes);
    LINEOS_NOTUSED(NumNodes);
}
STATIC VOID STBRPPackRects(STBRP_CONTEXT *Con, STBRP_RECT *Rects, INT32 NumRects)
{
    INT32 I;

    for (I = 0; I < NumRects; ++I)
    {
        if (Con->x + Rects[I].w > Con->width)
        {
            Con->x = 0;
            Con->y = Con->BottomY;
        }

        if (Con->y + Rects[I].h > Con->height)
            break;
        Rects[I].x = Con->x;
        Rects[I].y = Con->y;
        Rects[I].WasPacked = 1;
        Con->x += Rects[I].w;

        if (Con->y + Rects[I].h > Con->BottomY)
            Con->BottomY = Con->y + Rects[I].h;
    }

    for (; I < NumRects; ++I)
        Rects[I].WasPacked = 0;
}
EXTERN INT32 PackBegin(PACK_CONTEXT *Spc, UINT8 *Pixels, INT32 Pw, INT32 Ph, INT32 StrideInBytes, INT32 Padding, VOID *AllocContext)
{
    STBRP_CONTEXT *Context = (STBRP_CONTEXT *) KTTFAlloc(sizeof(*Context), AllocContext);
    INT32          NumNodes = Pw - Padding;
    STBRP_NODE    *Nodes = (STBRP_NODE *) KTTFAlloc(sizeof(*Nodes) * NumNodes, AllocContext);

    if (Context == NULL || Nodes == NULL)
    {
        if (Context != NULL)
            KTTFFree(Context, AllocContext);

        if (Nodes != NULL)
            KTTFFree(Nodes, AllocContext);

        return 0;
    }

    Spc->UserAllocatorContext = AllocContext;
    Spc->Width = Pw;
    Spc->Height = Ph;
    Spc->Pixels = Pixels;
    Spc->PackInfo = Context;
    Spc->Nodes = Nodes;
    Spc->Padding = Padding;
    Spc->StrideInBytes = StrideInBytes != 0 ? StrideInBytes : Pw;
    Spc->HOversample = 1;
    Spc->VOversample = 1;
    Spc->SkipMissing = 0;
    STBRPInitTarget(Context, Pw - Padding, Ph - Padding, Nodes, NumNodes);

    if (Pixels)
        KMemSet(Pixels, 0, Pw * Ph);

    return 1;
}
EXTERN VOID PackEnd(PACK_CONTEXT *Spc)
{
    KTTFFree(Spc->Nodes, Spc->UserAllocatorContext);
    KTTFFree(Spc->PackInfo, Spc->UserAllocatorContext);
}
EXTERN VOID PackSetOversampling(PACK_CONTEXT *Spc, UINT32 HOversample, UINT32 VOversample)
{
    KAssert(HOversample <= LINEOS_MAX_OVERSAMPLE);
    KAssert(VOversample <= LINEOS_MAX_OVERSAMPLE);

    if (HOversample <= LINEOS_MAX_OVERSAMPLE)
        Spc->HOversample = HOversample;

    if (VOversample <= LINEOS_MAX_OVERSAMPLE)
        Spc->VOversample = VOversample;
}
EXTERN VOID PackSetSkipMissingCodepoints(PACK_CONTEXT *Spc, INT32 Skip)
{
    Spc->SkipMissing = Skip;
}
#define LINEOS_OVER_MASK (LINEOS_MAX_OVERSAMPLE - 1)
STATIC VOID HPrefilter(UINT8 *Pixels, INT32 W, INT32 H, INT32 StrideInBytes, UINT32 KernelWidth)
{
    UINT8 Buffer[LINEOS_MAX_OVERSAMPLE];
    INT32 SafeW = W - KernelWidth;
    INT32 J;

    KMemSet(Buffer, 0, LINEOS_MAX_OVERSAMPLE);

    for (J = 0; J < H; ++J)
    {
        INT32  I;
        UINT32 Total;

        KMemSet(Buffer, 0, KernelWidth);
        Total = 0;

        switch (KernelWidth)
        {
            case 2:
                for (I = 0; I <= SafeW; ++I)
                {
                    Total += Pixels[I] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I];
                    Pixels[I] = (UINT8) (Total / 2);
                }

                break;

            case 3:
                for (I = 0; I <= SafeW; ++I)
                {
                    Total += Pixels[I] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I];
                    Pixels[I] = (UINT8) (Total / 3);
                }

                break;

            case 4:
                for (I = 0; I <= SafeW; ++I)
                {
                    Total += Pixels[I] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I];
                    Pixels[I] = (UINT8) (Total / 4);
                }

                break;

            case 5:
                for (I = 0; I <= SafeW; ++I)
                {
                    Total += Pixels[I] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I];
                    Pixels[I] = (UINT8) (Total / 5);
                }

                break;

            default:
                for (I = 0; I <= SafeW; ++I)
                {
                    Total += Pixels[I] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I];
                    Pixels[I] = (UINT8) (Total / KernelWidth);
                }

                break;
        }

        for (; I < W; ++I)
        {
            KAssert(Pixels[I] == 0);
            Total -= Buffer[I & LINEOS_OVER_MASK];
            Pixels[I] = (UINT8) (Total / KernelWidth);
        }

        Pixels += StrideInBytes;
    }
}
STATIC VOID VPrefilter(UINT8 *Pixels, INT32 W, INT32 H, INT32 StrideInBytes, UINT32 KernelWidth)
{
    UINT8 Buffer[LINEOS_MAX_OVERSAMPLE];
    INT32 SafeH = H - KernelWidth;
    INT32 J;

    KMemSet(Buffer, 0, LINEOS_MAX_OVERSAMPLE);

    for (J = 0; J < W; ++J)
    {
        INT32  I;
        UINT32 Total;

        KMemSet(Buffer, 0, KernelWidth);
        Total = 0;

        switch (KernelWidth)
        {
            case 2:
                for (I = 0; I <= SafeH; ++I)
                {
                    Total += Pixels[I * StrideInBytes] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I * StrideInBytes];
                    Pixels[I * StrideInBytes] = (UINT8) (Total / 2);
                }

                break;

            case 3:
                for (I = 0; I <= SafeH; ++I)
                {
                    Total += Pixels[I * StrideInBytes] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I * StrideInBytes];
                    Pixels[I * StrideInBytes] = (UINT8) (Total / 3);
                }

                break;

            case 4:
                for (I = 0; I <= SafeH; ++I)
                {
                    Total += Pixels[I * StrideInBytes] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I * StrideInBytes];
                    Pixels[I * StrideInBytes] = (UINT8) (Total / 4);
                }

                break;

            case 5:
                for (I = 0; I <= SafeH; ++I)
                {
                    Total += Pixels[I * StrideInBytes] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I * StrideInBytes];
                    Pixels[I * StrideInBytes] = (UINT8) (Total / 5);
                }

                break;

            default:
                for (I = 0; I <= SafeH; ++I)
                {
                    Total += Pixels[I * StrideInBytes] - Buffer[I & LINEOS_OVER_MASK];
                    Buffer[(I + KernelWidth) & LINEOS_OVER_MASK] = Pixels[I * StrideInBytes];
                    Pixels[I * StrideInBytes] = (UINT8) (Total / KernelWidth);
                }

                break;
        }

        for (; I < H; ++I)
        {
            KAssert(Pixels[I * StrideInBytes] == 0);
            Total -= Buffer[I & LINEOS_OVER_MASK];
            Pixels[I * StrideInBytes] = (UINT8) (Total / KernelWidth);
        }

        Pixels += 1;
    }
}
STATIC FLOAT32 OversampleShift(INT32 Oversample)
{
    if (!Oversample)
        return 0.0f;

    return (FLOAT32) - (Oversample - 1) / (2.0f * (FLOAT32) Oversample);
}
EXTERN INT32 PackFontRangesGatherRects(PACK_CONTEXT *Spc, CONST FONT_INFO *Info, PACK_RANGE *Ranges, INT32 NumRanges, STBRP_RECT *Rects)
{
    INT32 I, J, K;
    INT32 MissingGlyphAdded = 0;

    K = 0;

    for (I = 0; I < NumRanges; ++I)
    {
        FLOAT32 Fh = Ranges[I].FontSize;
        FLOAT32 Scale = Fh > 0 ? ScaleForPixelHeight(Info, Fh) : ScaleForMappingEmToPixels(Info, -Fh);

        Ranges[I].HOversample = (UINT8) Spc->HOversample;
        Ranges[I].VOversample = (UINT8) Spc->VOversample;

        for (J = 0; J < Ranges[I].NumChars; ++J)
        {
            INT32 X0, Y0, X1, Y1;
            INT32 Codepoint = Ranges[I].ArrayOfUnicodeCodepoints == NULL ? Ranges[I].FirstUnicodeCodepointInRange + J : Ranges[I].ArrayOfUnicodeCodepoints[J];
            INT32 Glyph = FindGlyphIndex(Info, Codepoint);

            if (Glyph == 0 && (Spc->SkipMissing || MissingGlyphAdded))
            {
                Rects[K].w = Rects[K].h = 0;
            }
            else
            {
                GetGlyphBitmapBoxSubpixel(Info, Glyph, Scale * Spc->HOversample, Scale * Spc->VOversample, 0, 0, &X0, &Y0, &X1, &Y1);
                Rects[K].w = (STBRP_COORD) (X1 - X0 + Spc->Padding + Spc->HOversample - 1);
                Rects[K].h = (STBRP_COORD) (Y1 - Y0 + Spc->Padding + Spc->VOversample - 1);

                if (Glyph == 0)
                    MissingGlyphAdded = 1;
            }

            ++K;
        }
    }

    return K;
}
EXTERN VOID MakeGlyphBitmapSubpixelPrefilter(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 PrefilterX, INT32 PrefilterY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 Glyph)
{
    MakeGlyphBitmapSubpixel(Info, Output, OutW - (PrefilterX - 1), OutH - (PrefilterY - 1), OutStride, ScaleX, ScaleY, ShiftX, ShiftY, Glyph);

    if (PrefilterX > 1)
        HPrefilter(Output, OutW, OutH, OutStride, PrefilterX);

    if (PrefilterY > 1)
        VPrefilter(Output, OutW, OutH, OutStride, PrefilterY);
    *SubX = OversampleShift(PrefilterX);
    *SubY = OversampleShift(PrefilterY);
}
EXTERN INT32 PackFontRangesRenderIntoRects(PACK_CONTEXT *Spc, CONST FONT_INFO *Info, PACK_RANGE *Ranges, INT32 NumRanges, STBRP_RECT *Rects)
{
    INT32 I, J, K, MissingGlyph = -1, ReturnValue = 1;
    INT32 OldHOver = Spc->HOversample;
    INT32 OldVOver = Spc->VOversample;

    K = 0;

    for (I = 0; I < NumRanges; ++I)
    {
        FLOAT32 Fh = Ranges[I].FontSize;
        FLOAT32 Scale = Fh > 0 ? ScaleForPixelHeight(Info, Fh) : ScaleForMappingEmToPixels(Info, -Fh);
        FLOAT32 RecipH, RecipV, SubX, SubY;

        Spc->HOversample = Ranges[I].HOversample;
        Spc->VOversample = Ranges[I].VOversample;
        RecipH = 1.0f / Spc->HOversample;
        RecipV = 1.0f / Spc->VOversample;
        SubX = OversampleShift(Spc->HOversample);
        SubY = OversampleShift(Spc->VOversample);

        for (J = 0; J < Ranges[I].NumChars; ++J)
        {
            STBRP_RECT *R = &Rects[K];

            if (R->WasPacked && R->w != 0 && R->h != 0)
            {
                PACKED_CHAR *Bc = &Ranges[I].ChardataForRange[J];
                INT32        Advance, Lsb, X0, Y0, X1, Y1;
                INT32        Codepoint = Ranges[I].ArrayOfUnicodeCodepoints == NULL ? Ranges[I].FirstUnicodeCodepointInRange + J : Ranges[I].ArrayOfUnicodeCodepoints[J];
                INT32        Glyph = FindGlyphIndex(Info, Codepoint);
                STBRP_COORD  Pad = (STBRP_COORD) Spc->Padding;

                R->x += Pad;
                R->y += Pad;
                R->w -= Pad;
                R->h -= Pad;
                GetGlyphHMetrics(Info, Glyph, &Advance, &Lsb);
                GetGlyphBitmapBox(Info, Glyph, Scale * Spc->HOversample, Scale * Spc->VOversample, &X0, &Y0, &X1, &Y1);
                MakeGlyphBitmapSubpixel(Info, Spc->Pixels + R->x + R->y * Spc->StrideInBytes, R->w - Spc->HOversample + 1, R->h - Spc->VOversample + 1, Spc->StrideInBytes, Scale * Spc->HOversample, Scale * Spc->VOversample, 0, 0, Glyph);

                if (Spc->HOversample > 1)
                    HPrefilter(Spc->Pixels + R->x + R->y * Spc->StrideInBytes, R->w, R->h, Spc->StrideInBytes, Spc->HOversample);

                if (Spc->VOversample > 1)
                    VPrefilter(Spc->Pixels + R->x + R->y * Spc->StrideInBytes, R->w, R->h, Spc->StrideInBytes, Spc->VOversample);
                Bc->X0 = (INT16) R->x;
                Bc->Y0 = (INT16) R->y;
                Bc->X1 = (INT16) (R->x + R->w);
                Bc->Y1 = (INT16) (R->y + R->h);
                Bc->Xadvance = Scale * Advance;
                Bc->Xoff = (FLOAT32) X0 * RecipH + SubX;
                Bc->Yoff = (FLOAT32) Y0 * RecipV + SubY;
                Bc->Xoff2 = (X0 + R->w) * RecipH + SubX;
                Bc->Yoff2 = (Y0 + R->h) * RecipV + SubY;

                if (Glyph == 0)
                    MissingGlyph = J;
            }
            else if (Spc->SkipMissing)
            {
                ReturnValue = 0;
            }
            else if (R->WasPacked && R->w == 0 && R->h == 0 && MissingGlyph >= 0)
            {
                Ranges[I].ChardataForRange[J] = Ranges[I].ChardataForRange[MissingGlyph];
            }
            else
            {
                ReturnValue = 0;
            }

            ++K;
        }
    }

    Spc->HOversample = OldHOver;
    Spc->VOversample = OldVOver;

    return ReturnValue;
}
EXTERN VOID PackFontRangesPackRects(PACK_CONTEXT *Spc, STBRP_RECT *Rects, INT32 NumRects)
{
    STBRPPackRects((STBRP_CONTEXT *) Spc->PackInfo, Rects, NumRects);
}
EXTERN INT32 PackFontRanges(PACK_CONTEXT *Spc, CONST UINT8 *Fontdata, INT32 FontIndex, PACK_RANGE *Ranges, INT32 NumRanges)
{
    FONT_INFO   Info;
    INT32       I, J, N, ReturnValue = 1;
    STBRP_RECT *Rects;

    for (I = 0; I < NumRanges; ++I)
        for (J = 0; J < Ranges[I].NumChars; ++J)
            Ranges[I].ChardataForRange[J].X0 = Ranges[I].ChardataForRange[J].Y0 = Ranges[I].ChardataForRange[J].X1 = Ranges[I].ChardataForRange[J].Y1 = 0;
    N = 0;

    for (I = 0; I < NumRanges; ++I)
        N += Ranges[I].NumChars;
    Rects = (STBRP_RECT *) KTTFAlloc(sizeof(*Rects) * N, Spc->UserAllocatorContext);

    if (Rects == NULL)
        return 0;
    Info.UserData = Spc->UserAllocatorContext;
    InitFont(&Info, Fontdata, GetFontOffsetForIndex(Fontdata, FontIndex));
    N = PackFontRangesGatherRects(Spc, &Info, Ranges, NumRanges, Rects);
    PackFontRangesPackRects(Spc, Rects, N);
    ReturnValue = PackFontRangesRenderIntoRects(Spc, &Info, Ranges, NumRanges, Rects);
    KTTFFree(Rects, Spc->UserAllocatorContext);

    return ReturnValue;
}
EXTERN INT32 PackFontRange(PACK_CONTEXT *Spc, CONST UINT8 *Fontdata, INT32 FontIndex, FLOAT32 FontSize, INT32 FirstUnicodeCodepointInRange, INT32 NumCharsInRange, PACKED_CHAR *ChardataForRange)
{
    PACK_RANGE Range;

    Range.FirstUnicodeCodepointInRange = FirstUnicodeCodepointInRange;
    Range.ArrayOfUnicodeCodepoints = NULL;
    Range.NumChars = NumCharsInRange;
    Range.ChardataForRange = ChardataForRange;
    Range.FontSize = FontSize;

    return PackFontRanges(Spc, Fontdata, FontIndex, &Range, 1);
}
EXTERN VOID GetScaledFontVMetrics(CONST UINT8 *Fontdata, INT32 Index, FLOAT32 Size, FLOAT32 *Ascent, FLOAT32 *Descent, FLOAT32 *LineGap)
{
    INT32     IAscent, IDescent, ILineGap;
    FLOAT32   Scale;
    FONT_INFO Info;

    InitFont(&Info, Fontdata, GetFontOffsetForIndex(Fontdata, Index));
    Scale = Size > 0 ? ScaleForPixelHeight(&Info, Size) : ScaleForMappingEmToPixels(&Info, -Size);
    GetFontVMetrics(&Info, &IAscent, &IDescent, &ILineGap);
    *Ascent = (FLOAT32) IAscent * Scale;
    *Descent = (FLOAT32) IDescent * Scale;
    *LineGap = (FLOAT32) ILineGap * Scale;
}
EXTERN VOID GetPackedQuad(CONST PACKED_CHAR *Chardata, INT32 Pw, INT32 Ph, INT32 CharIndex, FLOAT32 *Xpos, FLOAT32 *Ypos, ALIGNED_QUAD *Q, INT32 AlignToInteger)
{
    FLOAT32            Ipw = 1.0f / Pw, Iph = 1.0f / Ph;
    CONST PACKED_CHAR *B = Chardata + CharIndex;

    if (AlignToInteger)
    {
        FLOAT32 X = (FLOAT32) KFloor((*Xpos + B->Xoff) + 0.5f);
        FLOAT32 Y = (FLOAT32) KFloor((*Ypos + B->Yoff) + 0.5f);

        Q->X0 = X;
        Q->Y0 = Y;
        Q->X1 = X + B->Xoff2 - B->Xoff;
        Q->Y1 = Y + B->Yoff2 - B->Yoff;
    }
    else
    {
        Q->X0 = *Xpos + B->Xoff;
        Q->Y0 = *Ypos + B->Yoff;
        Q->X1 = *Xpos + B->Xoff2;
        Q->Y1 = *Ypos + B->Yoff2;
    }

    Q->S0 = B->X0 * Ipw;
    Q->T0 = B->Y0 * Iph;
    Q->S1 = B->X1 * Ipw;
    Q->T1 = B->Y1 * Iph;
    *Xpos += B->Xadvance;
}
#define LINEOS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LINEOS_MAX(a, b) ((a) < (b) ? (b) : (a))
STATIC INT32 RayIntersectBezier(FLOAT32 Orig[2], FLOAT32 Ray[2], FLOAT32 Q0[2], FLOAT32 Q1[2], FLOAT32 Q2[2], FLOAT32 Hits[2][2])
{
    FLOAT32 Q0perp = Q0[1] * Ray[0] - Q0[0] * Ray[1];
    FLOAT32 Q1perp = Q1[1] * Ray[0] - Q1[0] * Ray[1];
    FLOAT32 Q2perp = Q2[1] * Ray[0] - Q2[0] * Ray[1];
    FLOAT32 Roperp = Orig[1] * Ray[0] - Orig[0] * Ray[1];
    FLOAT32 A = Q0perp - 2 * Q1perp + Q2perp;
    FLOAT32 B = Q1perp - Q0perp;
    FLOAT32 C = Q0perp - Roperp;
    FLOAT32 S0 = 0., S1 = 0.;
    INT32   NumS = 0;

    if (A != 0.0)
    {
        FLOAT32 Discr = B * B - A * C;

        if (Discr > 0.0)
        {
            FLOAT32 Rcpna = -1 / A;
            FLOAT32 D = (FLOAT32) KSqrt(Discr);

            S0 = (B + D) * Rcpna;
            S1 = (B - D) * Rcpna;

            if (S0 >= 0.0 && S0 <= 1.0)
                NumS = 1;

            if (D > 0.0 && S1 >= 0.0 && S1 <= 1.0)
            {
                if (NumS == 0)
                    S0 = S1;
                ++NumS;
            }
        }
    }
    else
    {
        S0 = C / (-2 * B);

        if (S0 >= 0.0 && S0 <= 1.0)
            NumS = 1;
    }

    if (NumS == 0)
        return 0;
    else
    {
        FLOAT32 RcpLen2 = 1 / (Ray[0] * Ray[0] + Ray[1] * Ray[1]);
        FLOAT32 RaynX = Ray[0] * RcpLen2, RaynY = Ray[1] * RcpLen2;
        FLOAT32 Q0d = Q0[0] * RaynX + Q0[1] * RaynY;
        FLOAT32 Q1d = Q1[0] * RaynX + Q1[1] * RaynY;
        FLOAT32 Q2d = Q2[0] * RaynX + Q2[1] * RaynY;
        FLOAT32 Rod = Orig[0] * RaynX + Orig[1] * RaynY;
        FLOAT32 Q10d = Q1d - Q0d;
        FLOAT32 Q20d = Q2d - Q0d;
        FLOAT32 Q0rd = Q0d - Rod;

        Hits[0][0] = Q0rd + S0 * (2.0f - 2.0f * S0) * Q10d + S0 * S0 * Q20d;
        Hits[0][1] = A * S0 + B;

        if (NumS > 1)
        {
            Hits[1][0] = Q0rd + S1 * (2.0f - 2.0f * S1) * Q10d + S1 * S1 * Q20d;
            Hits[1][1] = A * S1 + B;

            return 2;
        }
        else
        {
            return 1;
        }
    }
}
STATIC INT32 Equal(FLOAT32 *A, FLOAT32 *B)
{
    return (A[0] == B[0] && A[1] == B[1]);
}
STATIC INT32 ComputeCrossingsX(FLOAT32 X, FLOAT32 Y, INT32 Nverts, VERTEX *Verts)
{
    INT32   I;
    FLOAT32 Orig[2], Ray[2] = {1, 0};
    FLOAT32 YFrac;
    INT32   Winding = 0;

    YFrac = (FLOAT32) KFMod(Y, 1.0f);

    if (YFrac < 0.01f)
        Y += 0.01f;
    else if (YFrac > 0.99f)
        Y -= 0.01f;
    Orig[0] = X;
    Orig[1] = Y;

    for (I = 0; I < Nverts; ++I)
    {
        if (Verts[I].Type == LINEOS_VLINE)
        {
            INT32 X0 = (INT32) Verts[I - 1].X, Y0 = (INT32) Verts[I - 1].Y;
            INT32 X1 = (INT32) Verts[I].X, Y1 = (INT32) Verts[I].Y;

            if (Y > LINEOS_MIN(Y0, Y1) && Y < LINEOS_MAX(Y0, Y1) && X > LINEOS_MIN(X0, X1))
            {
                FLOAT32 XInter = (Y - Y0) / (Y1 - Y0) * (X1 - X0) + X0;

                if (XInter < X)
                    Winding += (Y0 < Y1) ? 1 : -1;
            }
        }

        if (Verts[I].Type == LINEOS_VCURVE)
        {
            INT32 X0 = (INT32) Verts[I - 1].X, Y0 = (INT32) Verts[I - 1].Y;
            INT32 X1 = (INT32) Verts[I].Cx, Y1 = (INT32) Verts[I].Cy;
            INT32 X2 = (INT32) Verts[I].X, Y2 = (INT32) Verts[I].Y;
            INT32 Ax = LINEOS_MIN(X0, LINEOS_MIN(X1, X2)), Ay = LINEOS_MIN(Y0, LINEOS_MIN(Y1, Y2));
            INT32 By = LINEOS_MAX(Y0, LINEOS_MAX(Y1, Y2));

            if (Y > Ay && Y < By && X > Ax)
            {
                FLOAT32 Q0[2], Q1[2], Q2[2];
                FLOAT32 Hits[2][2];

                Q0[0] = (FLOAT32) X0;
                Q0[1] = (FLOAT32) Y0;
                Q1[0] = (FLOAT32) X1;
                Q1[1] = (FLOAT32) Y1;
                Q2[0] = (FLOAT32) X2;
                Q2[1] = (FLOAT32) Y2;

                if (Equal(Q0, Q1) || Equal(Q1, Q2))
                {
                    X0 = (INT32) Verts[I - 1].X;
                    Y0 = (INT32) Verts[I - 1].Y;
                    X1 = (INT32) Verts[I].X;
                    Y1 = (INT32) Verts[I].Y;

                    if (Y > LINEOS_MIN(Y0, Y1) && Y < LINEOS_MAX(Y0, Y1) && X > LINEOS_MIN(X0, X1))
                    {
                        FLOAT32 XInter = (Y - Y0) / (Y1 - Y0) * (X1 - X0) + X0;

                        if (XInter < X)
                            Winding += (Y0 < Y1) ? 1 : -1;
                    }
                }
                else
                {
                    INT32 NumHits = RayIntersectBezier(Orig, Ray, Q0, Q1, Q2, Hits);

                    if (NumHits >= 1)
                        if (Hits[0][0] < 0)
                            Winding += (Hits[0][1] < 0 ? -1 : 1);

                    if (NumHits >= 2)
                        if (Hits[1][0] < 0)
                            Winding += (Hits[1][1] < 0 ? -1 : 1);
                }
            }
        }
    }

    return Winding;
}
STATIC FLOAT32 Cuberoot(FLOAT32 X)
{
    if (X < 0)
        return -(FLOAT32) KPow(-X, 1.0f / 3.0f);
    else

        return (FLOAT32) KPow(X, 1.0f / 3.0f);
}
STATIC INT32 SolveCubic(FLOAT32 A, FLOAT32 B, FLOAT32 C, FLOAT32 *R)
{
    FLOAT32 S = -A / 3;
    FLOAT32 P = B - A * A / 3;
    FLOAT32 Q = A * (2 * A * A - 9 * B) / 27 + C;
    FLOAT32 P3 = P * P * P;
    FLOAT32 D = Q * Q + 4 * P3 / 27;

    if (D >= 0)
    {
        FLOAT32 Z = (FLOAT32) KSqrt(D);
        FLOAT32 U = (-Q + Z) / 2;
        FLOAT32 V = (-Q - Z) / 2;

        U = Cuberoot(U);
        V = Cuberoot(V);
        R[0] = S + U + V;

        return 1;
    }
    else
    {
        FLOAT32 U = (FLOAT32) KSqrt(-P / 3);
        FLOAT32 V = (FLOAT32) KACos(-KSqrt(-27 / P3) * Q / 2) / 3;
        FLOAT32 M = (FLOAT32) KCos(V);
        FLOAT32 N = (FLOAT32) KCos(V - 3.141592 / 2) * 1.732050808f;

        R[0] = S + U * 2 * M;
        R[1] = S - U * (M + N);
        R[2] = S - U * (M - N);

        return 3;
    }
}
EXTERN UINT8 *GetGlyphSDF(CONST FONT_INFO *Info, FLOAT32 Scale, INT32 Glyph, INT32 Padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff)
{
    FLOAT32 ScaleX = Scale, ScaleY = Scale;
    INT32   Ix0, Iy0, Ix1, Iy1;
    INT32   W, H;
    UINT8  *Data;

    if (Scale == 0)
        return NULL;
    GetGlyphBitmapBoxSubpixel(Info, Glyph, Scale, Scale, 0.0f, 0.0f, &Ix0, &Iy0, &Ix1, &Iy1);

    if (Ix0 == Ix1 || Iy0 == Iy1)
        return NULL;
    Ix0 -= Padding;
    Iy0 -= Padding;
    Ix1 += Padding;
    Iy1 += Padding;
    W = (Ix1 - Ix0);
    H = (Iy1 - Iy0);

    if (Width)
        *Width = W;

    if (Height)
        *Height = H;

    if (Xoff)
        *Xoff = Ix0;

    if (Yoff)
        *Yoff = Iy0;
    ScaleY = -ScaleY;
    {
        CONST FLOAT32 Eps = 1. / 1024, Eps2 = Eps * Eps;
        INT32         X, Y, I, J;
        FLOAT32      *Precompute;
        VERTEX       *Verts;
        INT32         NumVerts = GetGlyphShape(Info, Glyph, &Verts);

        Data = (UINT8 *) KTTFAlloc(W * H, Info->UserData);
        Precompute = (FLOAT32 *) KTTFAlloc(NumVerts * sizeof(FLOAT32), Info->UserData);

        for (I = 0, J = NumVerts - 1; I < NumVerts; J = I++)
        {
            if (Verts[I].Type == LINEOS_VLINE)
            {
                FLOAT32 X0 = Verts[I].X * ScaleX, Y0 = Verts[I].Y * ScaleY;
                FLOAT32 X1 = Verts[J].X * ScaleX, Y1 = Verts[J].Y * ScaleY;
                FLOAT32 Dist = (FLOAT32) KSqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));

                Precompute[I] = (Dist < Eps) ? 0.0f : 1.0f / Dist;
            }
            else if (Verts[I].Type == LINEOS_VCURVE)
            {
                FLOAT32 X2 = Verts[J].X * ScaleX, Y2 = Verts[J].Y * ScaleY;
                FLOAT32 X1 = Verts[I].Cx * ScaleX, Y1 = Verts[I].Cy * ScaleY;
                FLOAT32 X0 = Verts[I].X * ScaleX, Y0 = Verts[I].Y * ScaleY;
                FLOAT32 Bx = X0 - 2 * X1 + X2, By = Y0 - 2 * Y1 + Y2;
                FLOAT32 Len2 = Bx * Bx + By * By;

                if (Len2 >= Eps2)
                    Precompute[I] = 1.0f / Len2;
                else
                    Precompute[I] = 0.0f;
            }
            else
                Precompute[I] = 0.0f;
        }

        for (Y = Iy0; Y < Iy1; ++Y)
        {
            for (X = Ix0; X < Ix1; ++X)
            {
                FLOAT32 Val;
                FLOAT32 MinDist = 999999.0f;
                FLOAT32 Sx = (FLOAT32) X + 0.5f;
                FLOAT32 Sy = (FLOAT32) Y + 0.5f;
                FLOAT32 XGspace = (Sx / ScaleX);
                FLOAT32 YGspace = (Sy / ScaleY);
                INT32   Winding = ComputeCrossingsX(XGspace, YGspace, NumVerts, Verts);

                for (I = 0; I < NumVerts; ++I)
                {
                    FLOAT32 X0 = Verts[I].X * ScaleX, Y0 = Verts[I].Y * ScaleY;

                    if (Verts[I].Type == LINEOS_VLINE && Precompute[I] != 0.0f)
                    {
                        FLOAT32 X1 = Verts[I - 1].X * ScaleX, Y1 = Verts[I - 1].Y * ScaleY;
                        FLOAT32 Dist, Dist2 = (X0 - Sx) * (X0 - Sx) + (Y0 - Sy) * (Y0 - Sy);

                        if (Dist2 < MinDist * MinDist)
                            MinDist = (FLOAT32) KSqrt(Dist2);
                        Dist = (FLOAT32) KFAbs((X1 - X0) * (Y0 - Sy) - (Y1 - Y0) * (X0 - Sx)) * Precompute[I];
                        KAssert(I != 0);

                        if (Dist < MinDist)
                        {
                            FLOAT32 Dx = X1 - X0, Dy = Y1 - Y0;
                            FLOAT32 Px = X0 - Sx, Py = Y0 - Sy;
                            FLOAT32 T = -(Px * Dx + Py * Dy) / (Dx * Dx + Dy * Dy);

                            if (T >= 0.0f && T <= 1.0f)
                                MinDist = Dist;
                        }
                    }
                    else if (Verts[I].Type == LINEOS_VCURVE)
                    {
                        FLOAT32 X2 = Verts[I - 1].X * ScaleX, Y2 = Verts[I - 1].Y * ScaleY;
                        FLOAT32 X1 = Verts[I].Cx * ScaleX, Y1 = Verts[I].Cy * ScaleY;
                        FLOAT32 BoxX0 = LINEOS_MIN(LINEOS_MIN(X0, X1), X2);
                        FLOAT32 BoxY0 = LINEOS_MIN(LINEOS_MIN(Y0, Y1), Y2);
                        FLOAT32 BoxX1 = LINEOS_MAX(LINEOS_MAX(X0, X1), X2);
                        FLOAT32 BoxY1 = LINEOS_MAX(LINEOS_MAX(Y0, Y1), Y2);

                        if (Sx > BoxX0 - MinDist && Sx < BoxX1 + MinDist && Sy > BoxY0 - MinDist && Sy < BoxY1 + MinDist)
                        {
                            INT32   Num = 0;
                            FLOAT32 Ax = X1 - X0, Ay = Y1 - Y0;
                            FLOAT32 Bx = X0 - 2 * X1 + X2, By = Y0 - 2 * Y1 + Y2;
                            FLOAT32 Mx = X0 - Sx, My = Y0 - Sy;
                            FLOAT32 Res[3] = {0.f, 0.f, 0.f};
                            FLOAT32 Px, Py, T, It, Dist2;
                            FLOAT32 AInv = Precompute[I];

                            if (AInv == 0.0)
                            {
                                FLOAT32 A = 3 * (Ax * Bx + Ay * By);
                                FLOAT32 B = 2 * (Ax * Ax + Ay * Ay) + (Mx * Bx + My * By);
                                FLOAT32 C = Mx * Ax + My * Ay;

                                if (KFAbs(A) < Eps2)
                                {
                                    if (KFAbs(B) >= Eps2)
                                    {
                                        Res[Num++] = -C / B;
                                    }
                                }
                                else
                                {
                                    FLOAT32 Discriminant = B * B - 4 * A * C;

                                    if (Discriminant < 0)
                                        Num = 0;
                                    else
                                    {
                                        FLOAT32 Root = (FLOAT32) KSqrt(Discriminant);

                                        Res[0] = (-B - Root) / (2 * A);
                                        Res[1] = (-B + Root) / (2 * A);
                                        Num = 2;
                                    }
                                }
                            }
                            else
                            {
                                FLOAT32 B = 3 * (Ax * Bx + Ay * By) * AInv;
                                FLOAT32 C = (2 * (Ax * Ax + Ay * Ay) + (Mx * Bx + My * By)) * AInv;
                                FLOAT32 D = (Mx * Ax + My * Ay) * AInv;

                                Num = SolveCubic(B, C, D, Res);
                            }

                            Dist2 = (X0 - Sx) * (X0 - Sx) + (Y0 - Sy) * (Y0 - Sy);

                            if (Dist2 < MinDist * MinDist)
                                MinDist = (FLOAT32) KSqrt(Dist2);

                            if (Num >= 1 && Res[0] >= 0.0f && Res[0] <= 1.0f)
                            {
                                T = Res[0], It = 1.0f - T;
                                Px = It * It * X0 + 2 * T * It * X1 + T * T * X2;
                                Py = It * It * Y0 + 2 * T * It * Y1 + T * T * Y2;
                                Dist2 = (Px - Sx) * (Px - Sx) + (Py - Sy) * (Py - Sy);

                                if (Dist2 < MinDist * MinDist)
                                    MinDist = (FLOAT32) KSqrt(Dist2);
                            }

                            if (Num >= 2 && Res[1] >= 0.0f && Res[1] <= 1.0f)
                            {
                                T = Res[1], It = 1.0f - T;
                                Px = It * It * X0 + 2 * T * It * X1 + T * T * X2;
                                Py = It * It * Y0 + 2 * T * It * Y1 + T * T * Y2;
                                Dist2 = (Px - Sx) * (Px - Sx) + (Py - Sy) * (Py - Sy);

                                if (Dist2 < MinDist * MinDist)
                                    MinDist = (FLOAT32) KSqrt(Dist2);
                            }

                            if (Num >= 3 && Res[2] >= 0.0f && Res[2] <= 1.0f)
                            {
                                T = Res[2], It = 1.0f - T;
                                Px = It * It * X0 + 2 * T * It * X1 + T * T * X2;
                                Py = It * It * Y0 + 2 * T * It * Y1 + T * T * Y2;
                                Dist2 = (Px - Sx) * (Px - Sx) + (Py - Sy) * (Py - Sy);

                                if (Dist2 < MinDist * MinDist)
                                    MinDist = (FLOAT32) KSqrt(Dist2);
                            }
                        }
                    }
                }

                if (Winding == 0)
                    MinDist = -MinDist;
                Val = OnedgeValue + PixelDistScale * MinDist;

                if (Val < 0)
                    Val = 0;
                else if (Val > 255)
                    Val = 255;
                Data[(Y - Iy0) * W + (X - Ix0)] = (UINT8) Val;
            }
        }

        KTTFFree(Precompute, Info->UserData);
        KTTFFree(Verts, Info->UserData);
    }

    return Data;
}
EXTERN UINT8 *GetCodepointSDF(CONST FONT_INFO *Info, FLOAT32 Scale, INT32 Codepoint, INT32 Padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff)
{
    return GetGlyphSDF(Info, Scale, FindGlyphIndex(Info, Codepoint), Padding, OnedgeValue, PixelDistScale, Width, Height, Xoff, Yoff);
}
EXTERN VOID FreeSDF(UINT8 *Bitmap, VOID *Userdata)
{
    KTTFFree(Bitmap, Userdata);
}
EXTERN INT32 BakeFontBitmap(CONST UINT8 *Data, INT32 Offset, FLOAT32 PixelHeight, UINT8 *Pixels, INT32 Pw, INT32 Ph, INT32 FirstChar, INT32 NumChars, BAKED_CHAR *Chardata)
{
    return BakeFontBitmapInternal((UINT8 *) Data, Offset, PixelHeight, Pixels, Pw, Ph, FirstChar, NumChars, Chardata);
}
EXTERN INT32 GetFontOffsetForIndex(CONST UINT8 *Data, INT32 Index)
{
    return GetFontOffsetForIndexInternal((UINT8 *) Data, Index);
}
EXTERN INT32 GetNumberOfFonts(CONST UINT8 *Data)
{
    return GetNumberOfFontsInternal((UINT8 *) Data);
}
EXTERN INT32 InitFont(FONT_INFO *Info, CONST UINT8 *Data, INT32 Offset)
{
    return InitFontInternal(Info, (UINT8 *) Data, Offset);
}
