// kernel/src/render/truetype/truetype.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/truetype/truetype.h>
#include <render/truetype/truetype_runtime.h>
#include <lineos/typeinfo.h>

#define LINEOS_MAX_OVERSAMPLE 8
typedef INT32 TestOversamplePow2[(LINEOS_MAX_OVERSAMPLE & (LINEOS_MAX_OVERSAMPLE - 1)) == 0 ? 1 : -1];
#define LINEOS_NOTUSED(v) (VOID) sizeof(v)
STATIC UINT8 BufGet8(BUFFER *b)
{
    if (b->cursor >= b->size)
        return 0;
    return b->data[b->cursor++];
}
STATIC UINT8 BufPeek8(BUFFER *b)
{
    if (b->cursor >= b->size)
        return 0;
    return b->data[b->cursor];
}
STATIC VOID BufSeek(BUFFER *b, INT32 o)
{
    KAssert(!(o > b->size || o < 0));
    b->cursor = (o > b->size || o < 0) ? b->size : o;
}
STATIC VOID BufSkip(BUFFER *b, INT32 o)
{
    BufSeek(b, b->cursor + o);
}
STATIC UINT32 BufGet(BUFFER *b, INT32 n)
{
    UINT32 v = 0;
    INT32  i;
    KAssert(n >= 1 && n <= 4);
    for (i = 0; i < n; i++)
        v = (v << 8) | BufGet8(b);
    return v;
}
STATIC BUFFER NewBuf(CONST VOID *p, UINTN size)
{
    BUFFER r;
    KAssert(size < 0x40000000);
    r.data = (UINT8 *) p;
    r.size = (INT32) size;
    r.cursor = 0;
    return r;
}
#define BUF_GET16(b) BufGet((b), 2)
#define BUF_GET32(b) BufGet((b), 4)
STATIC BUFFER BufRange(CONST BUFFER *b, INT32 o, INT32 s)
{
    BUFFER r = NewBuf(NULL, 0);
    if (o < 0 || s < 0 || o > b->size || s > b->size - o)
        return r;
    r.data = b->data + o;
    r.size = s;
    return r;
}
STATIC BUFFER CffGetIndex(BUFFER *b)
{
    INT32 count, start, offsize;
    start = b->cursor;
    count = BUF_GET16(b);
    if (count)
    {
        offsize = BufGet8(b);
        KAssert(offsize >= 1 && offsize <= 4);
        BufSkip(b, offsize * count);
        BufSkip(b, BufGet(b, offsize) - 1);
    }
    return BufRange(b, start, b->cursor - start);
}
STATIC UINT32 CffInt(BUFFER *b)
{
    INT32 b0 = BufGet8(b);
    if (b0 >= 32 && b0 <= 246)
        return b0 - 139;
    else if (b0 >= 247 && b0 <= 250)
        return (b0 - 247) * 256 + BufGet8(b) + 108;
    else if (b0 >= 251 && b0 <= 254)
        return -(b0 - 251) * 256 - BufGet8(b) - 108;
    else if (b0 == 28)
        return BUF_GET16(b);
    else if (b0 == 29)
        return BUF_GET32(b);
    KAssert(0);
    return 0;
}
STATIC VOID CffSkipOperand(BUFFER *b)
{
    INT32 v, b0 = BufPeek8(b);
    KAssert(b0 >= 28);
    if (b0 == 30)
    {
        BufSkip(b, 1);
        while (b->cursor < b->size)
        {
            v = BufGet8(b);
            if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
                break;
        }
    }
    else
    {
        CffInt(b);
    }
}
STATIC BUFFER DictGet(BUFFER *b, INT32 key)
{
    BufSeek(b, 0);
    while (b->cursor < b->size)
    {
        INT32 start = b->cursor, end, op;
        while (BufPeek8(b) >= 28)
            CffSkipOperand(b);
        end = b->cursor;
        op = BufGet8(b);
        if (op == 12)
            op = BufGet8(b) | 0x100;
        if (op == key)
            return BufRange(b, start, end - start);
    }
    return BufRange(b, 0, 0);
}
STATIC VOID DictGetInts(BUFFER *b, INT32 key, INT32 outcount, UINT32 *out)
{
    INT32  i;
    BUFFER operands = DictGet(b, key);
    for (i = 0; i < outcount && operands.cursor < operands.size; i++)
        out[i] = CffInt(&operands);
}
STATIC INT32 CffIndexCount(BUFFER *b)
{
    BufSeek(b, 0);
    return BUF_GET16(b);
}
STATIC BUFFER CffIndexGet(BUFFER b, INT32 i)
{
    INT32 count, offsize, start, end;
    BufSeek(&b, 0);
    count = BUF_GET16(&b);
    offsize = BufGet8(&b);
    KAssert(i >= 0 && i < count);
    KAssert(offsize >= 1 && offsize <= 4);
    BufSkip(&b, i * offsize);
    start = BufGet(&b, offsize);
    end = BufGet(&b, offsize);
    return BufRange(&b, 2 + (count + 1) * offsize + start, end - start);
}
#define TT_BYTE(p)  (*(UINT8 *) (p))
#define TT_CHAR(p)  (*(INT8 *) (p))
#define TT_FIXED(p) TTLONG(p)
STATIC UINT16 TTUSHORT(UINT8 *p)
{
    return p[0] * 256 + p[1];
}
STATIC INT16 TTSHORT(UINT8 *p)
{
    return p[0] * 256 + p[1];
}
STATIC UINT32 TTULONG(UINT8 *p)
{
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}
STATIC INT32 TTLONG(UINT8 *p)
{
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}
#define TAG4(p, c0, c1, c2, c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define TAG(p, str)             TAG4(p, str[0], str[1], str[2], str[3])
STATIC INT32 Isfont(UINT8 *font)
{
    if (TAG4(font, '1', 0, 0, 0))
        return 1;
    if (TAG(font, "typ1"))
        return 1;
    if (TAG(font, "OTTO"))
        return 1;
    if (TAG4(font, 0, 1, 0, 0))
        return 1;
    if (TAG(font, "true"))
        return 1;
    return 0;
}
STATIC UINT32 FindTable(UINT8 *data, UINT32 fontstart, CONST VOID *TagName)
{
    CONST UINT8 *tag = (CONST UINT8 *) TagName;
    INT32        NumTables = TTUSHORT(data + fontstart + 4);
    UINT32       tabledir = fontstart + 12;
    INT32        i;
    for (i = 0; i < NumTables; ++i)
    {
        UINT32 loc = tabledir + 16 * i;
        if (TAG(data + loc + 0, tag))
            return TTULONG(data + loc + 8);
    }
    return 0;
}
STATIC INT32 GetFontOffsetForIndexInternal(UINT8 *FontCollection, INT32 index)
{
    if (Isfont(FontCollection))
        return index == 0 ? 0 : -1;
    if (TAG(FontCollection, "ttcf"))
    {
        if (TTULONG(FontCollection + 4) == 0x00010000 || TTULONG(FontCollection + 4) == 0x00020000)
        {
            INT32 n = TTLONG(FontCollection + 8);
            if (index >= n)
                return -1;
            return TTULONG(FontCollection + 12 + index * 4);
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
STATIC BUFFER GetSubrs(BUFFER cff, BUFFER fontdict)
{
    UINT32 subrsoff = 0, PrivateLoc[2] = {0, 0};
    BUFFER pdict;
    DictGetInts(&fontdict, 18, 2, PrivateLoc);
    if (!PrivateLoc[1] || !PrivateLoc[0])
        return NewBuf(NULL, 0);
    pdict = BufRange(&cff, PrivateLoc[1], PrivateLoc[0]);
    DictGetInts(&pdict, 19, 1, &subrsoff);
    if (!subrsoff)
        return NewBuf(NULL, 0);
    BufSeek(&cff, PrivateLoc[1] + subrsoff);
    return CffGetIndex(&cff);
}
STATIC INT32 GetSVG(FONT_INFO *info)
{
    UINT32 t;
    if (info->svg < 0)
    {
        t = FindTable(info->data, info->fontstart, "SVG ");
        if (t)
        {
            UINT32 offset = TTULONG(info->data + t + 2);
            info->svg = t + offset;
        }
        else
        {
            info->svg = 0;
        }
    }
    return info->svg;
}
STATIC INT32 InitFontInternal(FONT_INFO *info, UINT8 *data, INT32 fontstart)
{
    UINT32 cmap, t;
    INT32  i, NumTables;
    info->data = data;
    info->fontstart = fontstart;
    info->cff = NewBuf(NULL, 0);
    cmap = FindTable(data, fontstart, "cmap");
    info->loca = FindTable(data, fontstart, "loca");
    info->head = FindTable(data, fontstart, "head");
    info->glyf = FindTable(data, fontstart, "glyf");
    info->hhea = FindTable(data, fontstart, "hhea");
    info->hmtx = FindTable(data, fontstart, "hmtx");
    info->kern = FindTable(data, fontstart, "kern");
    info->gpos = FindTable(data, fontstart, "GPOS");
    if (!cmap || !info->head || !info->hhea || !info->hmtx)
        return 0;
    if (info->glyf)
    {
        if (!info->loca)
            return 0;
    }
    else
    {
        BUFFER b, topdict, topdictidx;
        UINT32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
        UINT32 cff;
        cff = FindTable(data, fontstart, "CFF ");
        if (!cff)
            return 0;
        info->fontdicts = NewBuf(NULL, 0);
        info->fdselect = NewBuf(NULL, 0);
        info->cff = NewBuf(data + cff, 512 * 1024 * 1024);
        b = info->cff;
        BufSkip(&b, 2);
        BufSeek(&b, BufGet8(&b));
        CffGetIndex(&b);
        topdictidx = CffGetIndex(&b);
        topdict = CffIndexGet(topdictidx, 0);
        CffGetIndex(&b);
        info->gsubrs = CffGetIndex(&b);
        DictGetInts(&topdict, 17, 1, &charstrings);
        DictGetInts(&topdict, 0x100 | 6, 1, &cstype);
        DictGetInts(&topdict, 0x100 | 36, 1, &fdarrayoff);
        DictGetInts(&topdict, 0x100 | 37, 1, &fdselectoff);
        info->subrs = GetSubrs(b, topdict);
        if (cstype != 2)
            return 0;
        if (charstrings == 0)
            return 0;
        if (fdarrayoff)
        {
            if (!fdselectoff)
                return 0;
            BufSeek(&b, fdarrayoff);
            info->fontdicts = CffGetIndex(&b);
            info->fdselect = BufRange(&b, fdselectoff, b.size - fdselectoff);
        }
        BufSeek(&b, charstrings);
        info->charstrings = CffGetIndex(&b);
    }
    t = FindTable(data, fontstart, "maxp");
    if (t)
        info->NumGlyphs = TTUSHORT(data + t + 4);
    else
        info->NumGlyphs = 0xffff;
    info->svg = -1;
    NumTables = TTUSHORT(data + cmap + 2);
    info->IndexMap = 0;
    for (i = 0; i < NumTables; ++i)
    {
        UINT32 EncodingRecord = cmap + 4 + 8 * i;
        switch (TTUSHORT(data + EncodingRecord))
        {
        case 3:
            switch (TTUSHORT(data + EncodingRecord + 2))
            {
            case 1:
            case 10:
                info->IndexMap = cmap + TTULONG(data + EncodingRecord + 4);
                break;
            }
            break;
        case 0:
            info->IndexMap = cmap + TTULONG(data + EncodingRecord + 4);
            break;
        }
    }
    if (info->IndexMap == 0)
        return 0;
    info->IndexToLocFormat = TTUSHORT(data + info->head + 50);
    return 1;
}
EXTERN INT32 FindGlyphIndex(CONST FONT_INFO *info, INT32 UnicodeCodepoint)
{
    UINT8 *data = info->data;
    UINT32 IndexMap = info->IndexMap;
    UINT16 format = TTUSHORT(data + IndexMap + 0);
    if (format == 0)
    {
        INT32 bytes = TTUSHORT(data + IndexMap + 2);
        if (UnicodeCodepoint < bytes - 6)
            return TT_BYTE(data + IndexMap + 6 + UnicodeCodepoint);
        return 0;
    }
    else if (format == 6)
    {
        UINT32 first = TTUSHORT(data + IndexMap + 6);
        UINT32 count = TTUSHORT(data + IndexMap + 8);
        if ((UINT32) UnicodeCodepoint >= first && (UINT32) UnicodeCodepoint < first + count)
            return TTUSHORT(data + IndexMap + 10 + (UnicodeCodepoint - first) * 2);
        return 0;
    }
    else if (format == 2)
    {
        KAssert(0);
        return 0;
    }
    else if (format == 4)
    {
        UINT16 segcount = TTUSHORT(data + IndexMap + 6) >> 1;
        UINT16 SearchRange = TTUSHORT(data + IndexMap + 8) >> 1;
        UINT16 EntrySelector = TTUSHORT(data + IndexMap + 10);
        UINT16 RangeShift = TTUSHORT(data + IndexMap + 12) >> 1;
        UINT32 EndCount = IndexMap + 14;
        UINT32 search = EndCount;
        if (UnicodeCodepoint > 0xffff)
            return 0;
        if (UnicodeCodepoint >= TTUSHORT(data + search + RangeShift * 2))
            search += RangeShift * 2;
        search -= 2;
        while (EntrySelector)
        {
            UINT16 end;
            SearchRange >>= 1;
            end = TTUSHORT(data + search + SearchRange * 2);
            if (UnicodeCodepoint > end)
                search += SearchRange * 2;
            --EntrySelector;
        }
        search += 2;
        {
            UINT16 offset, start, last;
            UINT16 item = (UINT16) ((search - EndCount) >> 1);
            start = TTUSHORT(data + IndexMap + 14 + segcount * 2 + 2 + 2 * item);
            last = TTUSHORT(data + EndCount + 2 * item);
            if (UnicodeCodepoint < start || UnicodeCodepoint > last)
                return 0;
            offset = TTUSHORT(data + IndexMap + 14 + segcount * 6 + 2 + 2 * item);
            if (offset == 0)
                return (UINT16) (UnicodeCodepoint + TTSHORT(data + IndexMap + 14 + segcount * 4 + 2 + 2 * item));
            return TTUSHORT(data + offset + (UnicodeCodepoint - start) * 2 + IndexMap + 14 + segcount * 6 + 2 + 2 * item);
        }
    }
    else if (format == 12 || format == 13)
    {
        UINT32 ngroups = TTULONG(data + IndexMap + 12);
        INT32  low, high;
        low = 0;
        high = (INT32) ngroups;
        while (low < high)
        {
            INT32  mid = low + ((high - low) >> 1);
            UINT32 StartChar = TTULONG(data + IndexMap + 16 + mid * 12);
            UINT32 EndChar = TTULONG(data + IndexMap + 16 + mid * 12 + 4);
            if ((UINT32) UnicodeCodepoint < StartChar)
                high = mid;
            else if ((UINT32) UnicodeCodepoint > EndChar)
                low = mid + 1;
            else
            {
                UINT32 StartGlyph = TTULONG(data + IndexMap + 16 + mid * 12 + 8);
                if (format == 12)
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
EXTERN INT32 GetCodepointShape(CONST FONT_INFO *info, INT32 UnicodeCodepoint, VERTEX **vertices)
{
    return GetGlyphShape(info, FindGlyphIndex(info, UnicodeCodepoint), vertices);
}
STATIC VOID Setvertex(VERTEX *v, UINT8 type, INT32 x, INT32 y, INT32 cx, INT32 cy)
{
    v->type = type;
    v->x = (INT16) x;
    v->y = (INT16) y;
    v->cx = (INT16) cx;
    v->cy = (INT16) cy;
}
STATIC INT32 GetGlyfOffset(CONST FONT_INFO *info, INT32 GlyphIndex)
{
    INT32 g1, g2;
    KAssert(!info->cff.size);
    if (GlyphIndex >= info->NumGlyphs)
        return -1;
    if (info->IndexToLocFormat >= 2)
        return -1;
    if (info->IndexToLocFormat == 0)
    {
        g1 = info->glyf + TTUSHORT(info->data + info->loca + GlyphIndex * 2) * 2;
        g2 = info->glyf + TTUSHORT(info->data + info->loca + GlyphIndex * 2 + 2) * 2;
    }
    else
    {
        g1 = info->glyf + TTULONG(info->data + info->loca + GlyphIndex * 4);
        g2 = info->glyf + TTULONG(info->data + info->loca + GlyphIndex * 4 + 4);
    }
    return g1 == g2 ? -1 : g1;
}
STATIC INT32 GetGlyphInfoT2(CONST FONT_INFO *info, INT32 GlyphIndex, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1);
EXTERN INT32 GetGlyphBox(CONST FONT_INFO *info, INT32 GlyphIndex, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1)
{
    if (info->cff.size)
    {
        GetGlyphInfoT2(info, GlyphIndex, x0, y0, x1, y1);
    }
    else
    {
        INT32 g = GetGlyfOffset(info, GlyphIndex);
        if (g < 0)
            return 0;
        if (x0)
            *x0 = TTSHORT(info->data + g + 2);
        if (y0)
            *y0 = TTSHORT(info->data + g + 4);
        if (x1)
            *x1 = TTSHORT(info->data + g + 6);
        if (y1)
            *y1 = TTSHORT(info->data + g + 8);
    }
    return 1;
}
EXTERN INT32 GetCodepointBox(CONST FONT_INFO *info, INT32 codepoint, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1)
{
    return GetGlyphBox(info, FindGlyphIndex(info, codepoint), x0, y0, x1, y1);
}
EXTERN INT32 IsGlyphEmpty(CONST FONT_INFO *info, INT32 GlyphIndex)
{
    INT16 NumberOfContours;
    INT32 g;
    if (info->cff.size)
        return GetGlyphInfoT2(info, GlyphIndex, NULL, NULL, NULL, NULL) == 0;
    g = GetGlyfOffset(info, GlyphIndex);
    if (g < 0)
        return 1;
    NumberOfContours = TTSHORT(info->data + g);
    return NumberOfContours == 0;
}
STATIC INT32 CloseShape(VERTEX *vertices, INT32 NumVertices, INT32 WasOff, INT32 StartOff, INT32 sx, INT32 sy, INT32 scx, INT32 scy, INT32 cx, INT32 cy)
{
    if (StartOff)
    {
        if (WasOff)
            Setvertex(&vertices[NumVertices++], LINEOS_VCURVE, (cx + scx) >> 1, (cy + scy) >> 1, cx, cy);
        Setvertex(&vertices[NumVertices++], LINEOS_VCURVE, sx, sy, scx, scy);
    }
    else
    {
        if (WasOff)
            Setvertex(&vertices[NumVertices++], LINEOS_VCURVE, sx, sy, cx, cy);
        else
            Setvertex(&vertices[NumVertices++], LINEOS_VLINE, sx, sy, 0, 0);
    }
    return NumVertices;
}
STATIC INT32 GetGlyphShapeTT(CONST FONT_INFO *info, INT32 GlyphIndex, VERTEX **pvertices)
{
    INT16   NumberOfContours;
    UINT8  *EndPtsOfContours;
    UINT8  *data = info->data;
    VERTEX *vertices = 0;
    INT32   NumVertices = 0;
    INT32   g = GetGlyfOffset(info, GlyphIndex);
    *pvertices = NULL;
    if (g < 0)
        return 0;
    NumberOfContours = TTSHORT(data + g);
    if (NumberOfContours > 0)
    {
        UINT8  flags = 0, flagcount;
        INT32  ins, i, j = 0, m, n, NextMove, WasOff = 0, off, StartOff = 0;
        INT32  x, y, cx, cy, sx, sy, scx, scy;
        UINT8 *points;
        EndPtsOfContours = (data + g + 10);
        ins = TTUSHORT(data + g + 10 + NumberOfContours * 2);
        points = data + g + 10 + NumberOfContours * 2 + 2 + ins;
        n = 1 + TTUSHORT(EndPtsOfContours + NumberOfContours * 2 - 2);
        m = n + 2 * NumberOfContours;
        vertices = (VERTEX *) KTTFAlloc(m * sizeof(vertices[0]), info->userdata);
        if (vertices == 0)
            return 0;
        NextMove = 0;
        flagcount = 0;
        off = m - n;
        for (i = 0; i < n; ++i)
        {
            if (flagcount == 0)
            {
                flags = *points++;
                if (flags & 8)
                    flagcount = *points++;
            }
            else
                --flagcount;
            vertices[off + i].type = flags;
        }
        x = 0;
        for (i = 0; i < n; ++i)
        {
            flags = vertices[off + i].type;
            if (flags & 2)
            {
                INT16 dx = *points++;
                x += (flags & 16) ? dx : -dx;
            }
            else
            {
                if (!(flags & 16))
                {
                    x = x + (INT16) (points[0] * 256 + points[1]);
                    points += 2;
                }
            }
            vertices[off + i].x = (INT16) x;
        }
        y = 0;
        for (i = 0; i < n; ++i)
        {
            flags = vertices[off + i].type;
            if (flags & 4)
            {
                INT16 dy = *points++;
                y += (flags & 32) ? dy : -dy;
            }
            else
            {
                if (!(flags & 32))
                {
                    y = y + (INT16) (points[0] * 256 + points[1]);
                    points += 2;
                }
            }
            vertices[off + i].y = (INT16) y;
        }
        NumVertices = 0;
        sx = sy = cx = cy = scx = scy = 0;
        for (i = 0; i < n; ++i)
        {
            flags = vertices[off + i].type;
            x = (INT16) vertices[off + i].x;
            y = (INT16) vertices[off + i].y;
            if (NextMove == i)
            {
                if (i != 0)
                    NumVertices = CloseShape(vertices, NumVertices, WasOff, StartOff, sx, sy, scx, scy, cx, cy);
                StartOff = !(flags & 1);
                if (StartOff)
                {
                    scx = x;
                    scy = y;
                    if (!(vertices[off + i + 1].type & 1))
                    {
                        sx = (x + (INT32) vertices[off + i + 1].x) >> 1;
                        sy = (y + (INT32) vertices[off + i + 1].y) >> 1;
                    }
                    else
                    {
                        sx = (INT32) vertices[off + i + 1].x;
                        sy = (INT32) vertices[off + i + 1].y;
                        ++i;
                    }
                }
                else
                {
                    sx = x;
                    sy = y;
                }
                Setvertex(&vertices[NumVertices++], LINEOS_VMOVE, sx, sy, 0, 0);
                WasOff = 0;
                NextMove = 1 + TTUSHORT(EndPtsOfContours + j * 2);
                ++j;
            }
            else
            {
                if (!(flags & 1))
                {
                    if (WasOff)
                        Setvertex(&vertices[NumVertices++], LINEOS_VCURVE, (cx + x) >> 1, (cy + y) >> 1, cx, cy);
                    cx = x;
                    cy = y;
                    WasOff = 1;
                }
                else
                {
                    if (WasOff)
                        Setvertex(&vertices[NumVertices++], LINEOS_VCURVE, x, y, cx, cy);
                    else
                        Setvertex(&vertices[NumVertices++], LINEOS_VLINE, x, y, 0, 0);
                    WasOff = 0;
                }
            }
        }
        NumVertices = CloseShape(vertices, NumVertices, WasOff, StartOff, sx, sy, scx, scy, cx, cy);
    }
    else if (NumberOfContours < 0)
    {
        INT32  more = 1;
        UINT8 *comp = data + g + 10;
        NumVertices = 0;
        vertices = 0;
        while (more)
        {
            UINT16  flags, gidx;
            INT32   CompNumVerts = 0, i;
            VERTEX *CompVerts = 0, *tmp = 0;
            FLOAT32 mtx[6] = {1, 0, 0, 1, 0, 0}, m, n;
            flags = TTSHORT(comp);
            comp += 2;
            gidx = TTSHORT(comp);
            comp += 2;
            if (flags & 2)
            {
                if (flags & 1)
                {
                    mtx[4] = TTSHORT(comp);
                    comp += 2;
                    mtx[5] = TTSHORT(comp);
                    comp += 2;
                }
                else
                {
                    mtx[4] = TT_CHAR(comp);
                    comp += 1;
                    mtx[5] = TT_CHAR(comp);
                    comp += 1;
                }
            }
            else
            {
                KAssert(0);
            }
            if (flags & (1 << 3))
            {
                mtx[0] = mtx[3] = TTSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[1] = mtx[2] = 0;
            }
            else if (flags & (1 << 6))
            {
                mtx[0] = TTSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[1] = mtx[2] = 0;
                mtx[3] = TTSHORT(comp) / 16384.0f;
                comp += 2;
            }
            else if (flags & (1 << 7))
            {
                mtx[0] = TTSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[1] = TTSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[2] = TTSHORT(comp) / 16384.0f;
                comp += 2;
                mtx[3] = TTSHORT(comp) / 16384.0f;
                comp += 2;
            }
            m = (FLOAT32) KSqrt(mtx[0] * mtx[0] + mtx[1] * mtx[1]);
            n = (FLOAT32) KSqrt(mtx[2] * mtx[2] + mtx[3] * mtx[3]);
            CompNumVerts = GetGlyphShape(info, gidx, &CompVerts);
            if (CompNumVerts > 0)
            {
                for (i = 0; i < CompNumVerts; ++i)
                {
                    VERTEX     *v = &CompVerts[i];
                    VERTEX_TYPE x, y;
                    x = v->x;
                    y = v->y;
                    v->x = (VERTEX_TYPE) (m * (mtx[0] * x + mtx[2] * y + mtx[4]));
                    v->y = (VERTEX_TYPE) (n * (mtx[1] * x + mtx[3] * y + mtx[5]));
                    x = v->cx;
                    y = v->cy;
                    v->cx = (VERTEX_TYPE) (m * (mtx[0] * x + mtx[2] * y + mtx[4]));
                    v->cy = (VERTEX_TYPE) (n * (mtx[1] * x + mtx[3] * y + mtx[5]));
                }
                tmp = (VERTEX *) KTTFAlloc((NumVertices + CompNumVerts) * sizeof(VERTEX), info->userdata);
                if (!tmp)
                {
                    if (vertices)
                        KTTFFree(vertices, info->userdata);
                    if (CompVerts)
                        KTTFFree(CompVerts, info->userdata);
                    return 0;
                }
                if (NumVertices > 0 && vertices)
                    KMemCpy(tmp, vertices, NumVertices * sizeof(VERTEX));
                KMemCpy(tmp + NumVertices, CompVerts, CompNumVerts * sizeof(VERTEX));
                if (vertices)
                    KTTFFree(vertices, info->userdata);
                vertices = tmp;
                KTTFFree(CompVerts, info->userdata);
                NumVertices += CompNumVerts;
            }
            more = flags & (1 << 5);
        }
    }
    else
    {
    }
    *pvertices = vertices;
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
#define LINEOS_CSCTX_INIT(bounds)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
        bounds, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    }
STATIC VOID TrackVertex(CS_CONTEXT *c, INT32 x, INT32 y)
{
    if (x > c->MaxX || !c->started)
        c->MaxX = x;
    if (y > c->MaxY || !c->started)
        c->MaxY = y;
    if (x < c->MinX || !c->started)
        c->MinX = x;
    if (y < c->MinY || !c->started)
        c->MinY = y;
    c->started = 1;
}
STATIC VOID CsctxV(CS_CONTEXT *c, UINT8 type, INT32 x, INT32 y, INT32 cx, INT32 cy, INT32 cx1, INT32 cy1)
{
    if (c->bounds)
    {
        TrackVertex(c, x, y);
        if (type == LINEOS_VCUBIC)
        {
            TrackVertex(c, cx, cy);
            TrackVertex(c, cx1, cy1);
        }
    }
    else
    {
        Setvertex(&c->pvertices[c->NumVertices], type, x, y, cx, cy);
        c->pvertices[c->NumVertices].cx1 = (INT16) cx1;
        c->pvertices[c->NumVertices].cy1 = (INT16) cy1;
    }
    c->NumVertices++;
}
STATIC VOID CsctxCloseShape(CS_CONTEXT *ctx)
{
    if (ctx->FirstX != ctx->x || ctx->FirstY != ctx->y)
        CsctxV(ctx, LINEOS_VLINE, (INT32) ctx->FirstX, (INT32) ctx->FirstY, 0, 0, 0, 0);
}
STATIC VOID CsctxRmoveTo(CS_CONTEXT *ctx, FLOAT32 dx, FLOAT32 dy)
{
    CsctxCloseShape(ctx);
    ctx->FirstX = ctx->x = ctx->x + dx;
    ctx->FirstY = ctx->y = ctx->y + dy;
    CsctxV(ctx, LINEOS_VMOVE, (INT32) ctx->x, (INT32) ctx->y, 0, 0, 0, 0);
}
STATIC VOID CsctxRlineTo(CS_CONTEXT *ctx, FLOAT32 dx, FLOAT32 dy)
{
    ctx->x += dx;
    ctx->y += dy;
    CsctxV(ctx, LINEOS_VLINE, (INT32) ctx->x, (INT32) ctx->y, 0, 0, 0, 0);
}
STATIC VOID CsctxRccurveTo(CS_CONTEXT *ctx, FLOAT32 dx1, FLOAT32 dy1, FLOAT32 dx2, FLOAT32 dy2, FLOAT32 dx3, FLOAT32 dy3)
{
    FLOAT32 cx1 = ctx->x + dx1;
    FLOAT32 cy1 = ctx->y + dy1;
    FLOAT32 cx2 = cx1 + dx2;
    FLOAT32 cy2 = cy1 + dy2;
    ctx->x = cx2 + dx3;
    ctx->y = cy2 + dy3;
    CsctxV(ctx, LINEOS_VCUBIC, (INT32) ctx->x, (INT32) ctx->y, (INT32) cx1, (INT32) cy1, (INT32) cx2, (INT32) cy2);
}
STATIC BUFFER GetSubr(BUFFER idx, INT32 n)
{
    INT32 count = CffIndexCount(&idx);
    INT32 bias = 107;
    if (count >= 33900)
        bias = 32768;
    else if (count >= 1240)
        bias = 1131;
    n += bias;
    if (n < 0 || n >= count)
        return NewBuf(NULL, 0);
    return CffIndexGet(idx, n);
}
STATIC BUFFER CidGetGlyphSubrs(CONST FONT_INFO *info, INT32 GlyphIndex)
{
    BUFFER fdselect = info->fdselect;
    INT32  nranges, start, end, v, fmt, fdselector = -1, i;
    BufSeek(&fdselect, 0);
    fmt = BufGet8(&fdselect);
    if (fmt == 0)
    {
        BufSkip(&fdselect, GlyphIndex);
        fdselector = BufGet8(&fdselect);
    }
    else if (fmt == 3)
    {
        nranges = BUF_GET16(&fdselect);
        start = BUF_GET16(&fdselect);
        for (i = 0; i < nranges; i++)
        {
            v = BufGet8(&fdselect);
            end = BUF_GET16(&fdselect);
            if (GlyphIndex >= start && GlyphIndex < end)
            {
                fdselector = v;
                break;
            }
            start = end;
        }
    }
    if (fdselector == -1)
        NewBuf(NULL, 0);
    return GetSubrs(info->cff, CffIndexGet(info->fontdicts, fdselector));
}
STATIC INT32 RunCharstring(CONST FONT_INFO *info, INT32 GlyphIndex, CS_CONTEXT *c)
{
    INT32   InHeader = 1, maskbits = 0, SubrStackHeight = 0, sp = 0, v, i, b0;
    INT32   HasSubrs = 0, ClearStack;
    FLOAT32 s[48];
    BUFFER  SubrStack[10], subrs = info->subrs, b;
    FLOAT32 f;
#define LINEOS_CSERR(s) (0)
    b = CffIndexGet(info->charstrings, GlyphIndex);
    while (b.cursor < b.size)
    {
        i = 0;
        ClearStack = 1;
        b0 = BufGet8(&b);
        switch (b0)
        {
        case 0x13:
        case 0x14:
            if (InHeader)
                maskbits += (sp / 2);
            InHeader = 0;
            BufSkip(&b, (maskbits + 7) / 8);
            break;
        case 0X01:
        case 0x03:
        case 0X12:
        case 0x17:
            maskbits += (sp / 2);
            break;
        case 0x15:
            InHeader = 0;
            if (sp < 2)
                return LINEOS_CSERR("rmoveto stack");
            CsctxRmoveTo(c, s[sp - 2], s[sp - 1]);
            break;
        case 0x04:
            InHeader = 0;
            if (sp < 1)
                return LINEOS_CSERR("vmoveto stack");
            CsctxRmoveTo(c, 0, s[sp - 1]);
            break;
        case 0x16:
            InHeader = 0;
            if (sp < 1)
                return LINEOS_CSERR("hmoveto stack");
            CsctxRmoveTo(c, s[sp - 1], 0);
            break;
        case 0x05:
            if (sp < 2)
                return LINEOS_CSERR("rlineto stack");
            for (; i + 1 < sp; i += 2)
                CsctxRlineTo(c, s[i], s[i + 1]);
            break;
        case 0x07:
            if (sp < 1)
                return LINEOS_CSERR("vlineto stack");
            goto vlineto;
        case 0x06:
            if (sp < 1)
                return LINEOS_CSERR("hlineto stack");
            for (;;)
            {
                if (i >= sp)
                    break;
                CsctxRlineTo(c, s[i], 0);
                i++;
            vlineto:
                if (i >= sp)
                    break;
                CsctxRlineTo(c, 0, s[i]);
                i++;
            }
            break;
        case 0x1F:
            if (sp < 4)
                return LINEOS_CSERR("hvcurveto stack");
            goto hvcurveto;
        case 0x1E:
            if (sp < 4)
                return LINEOS_CSERR("vhcurveto stack");
            for (;;)
            {
                if (i + 3 >= sp)
                    break;
                CsctxRccurveTo(c, 0, s[i], s[i + 1], s[i + 2], s[i + 3], (sp - i == 5) ? s[i + 4] : 0.0f);
                i += 4;
            hvcurveto:
                if (i + 3 >= sp)
                    break;
                CsctxRccurveTo(c, s[i], 0, s[i + 1], s[i + 2], (sp - i == 5) ? s[i + 4] : 0.0f, s[i + 3]);
                i += 4;
            }
            break;
        case 0x08:
            if (sp < 6)
                return LINEOS_CSERR("rcurveline stack");
            for (; i + 5 < sp; i += 6)
                CsctxRccurveTo(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;
        case 0x18:
            if (sp < 8)
                return LINEOS_CSERR("rcurveline stack");
            for (; i + 5 < sp - 2; i += 6)
                CsctxRccurveTo(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            if (i + 1 >= sp)
                return LINEOS_CSERR("rcurveline stack");
            CsctxRlineTo(c, s[i], s[i + 1]);
            break;
        case 0x19:
            if (sp < 8)
                return LINEOS_CSERR("rlinecurve stack");
            for (; i + 1 < sp - 6; i += 2)
                CsctxRlineTo(c, s[i], s[i + 1]);
            if (i + 5 >= sp)
                return LINEOS_CSERR("rlinecurve stack");
            CsctxRccurveTo(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
            break;
        case 0x1A:
        case 0x1B:
            if (sp < 4)
                return LINEOS_CSERR("(vv|hh)curveto stack");
            f = 0.0;
            if (sp & 1)
            {
                f = s[i];
                i++;
            }
            for (; i + 3 < sp; i += 4)
            {
                if (b0 == 0x1B)
                    CsctxRccurveTo(c, s[i], f, s[i + 1], s[i + 2], s[i + 3], 0.0);
                else
                    CsctxRccurveTo(c, f, s[i], s[i + 1], s[i + 2], 0.0, s[i + 3]);
                f = 0.0;
            }
            break;
        case 0x0A:
            if (!HasSubrs)
            {
                if (info->fdselect.size)
                    subrs = CidGetGlyphSubrs(info, GlyphIndex);
                HasSubrs = 1;
            }
        case 0x1D:
            if (sp < 1)
                return LINEOS_CSERR("call(g|)subr stack");
            v = (INT32) s[--sp];
            if (SubrStackHeight >= 10)
                return LINEOS_CSERR("recursion limit");
            SubrStack[SubrStackHeight++] = b;
            b = GetSubr(b0 == 0x0A ? subrs : info->gsubrs, v);
            if (b.size == 0)
                return LINEOS_CSERR("subr not found");
            b.cursor = 0;
            ClearStack = 0;
            break;
        case 0x0B:
            if (SubrStackHeight <= 0)
                return LINEOS_CSERR("return outside subr");
            b = SubrStack[--SubrStackHeight];
            ClearStack = 0;
            break;
        case 0x0E:
            CsctxCloseShape(c);
            return 1;
        case 0x0C:
        {
            FLOAT32 dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
            FLOAT32 dx, dy;
            INT32   b1 = BufGet8(&b);
            switch (b1)
            {
            case 0x22:
                if (sp < 7)
                    return LINEOS_CSERR("hflex stack");
                dx1 = s[0];
                dx2 = s[1];
                dy2 = s[2];
                dx3 = s[3];
                dx4 = s[4];
                dx5 = s[5];
                dx6 = s[6];
                CsctxRccurveTo(c, dx1, 0, dx2, dy2, dx3, 0);
                CsctxRccurveTo(c, dx4, 0, dx5, -dy2, dx6, 0);
                break;
            case 0X23:
                if (sp < 13)
                    return LINEOS_CSERR("flex stack");
                dx1 = s[0];
                dy1 = s[1];
                dx2 = s[2];
                dy2 = s[3];
                dx3 = s[4];
                dy3 = s[5];
                dx4 = s[6];
                dy4 = s[7];
                dx5 = s[8];
                dy5 = s[9];
                dx6 = s[10];
                dy6 = s[11];
                CsctxRccurveTo(c, dx1, dy1, dx2, dy2, dx3, dy3);
                CsctxRccurveTo(c, dx4, dy4, dx5, dy5, dx6, dy6);
                break;
            case 0x24:
                if (sp < 9)
                    return LINEOS_CSERR("hflex1 stack");
                dx1 = s[0];
                dy1 = s[1];
                dx2 = s[2];
                dy2 = s[3];
                dx3 = s[4];
                dx4 = s[5];
                dx5 = s[6];
                dy5 = s[7];
                dx6 = s[8];
                CsctxRccurveTo(c, dx1, dy1, dx2, dy2, dx3, 0);
                CsctxRccurveTo(c, dx4, 0, dx5, dy5, dx6, -(dy1 + dy2 + dy5));
                break;
            case 0x25:
                if (sp < 11)
                    return LINEOS_CSERR("flex1 stack");
                dx1 = s[0];
                dy1 = s[1];
                dx2 = s[2];
                dy2 = s[3];
                dx3 = s[4];
                dy3 = s[5];
                dx4 = s[6];
                dy4 = s[7];
                dx5 = s[8];
                dy5 = s[9];
                dx6 = dy6 = s[10];
                dx = dx1 + dx2 + dx3 + dx4 + dx5;
                dy = dy1 + dy2 + dy3 + dy4 + dy5;
                if (KFAbs(dx) > KFAbs(dy))
                    dy6 = -dy;
                else
                    dx6 = -dx;
                CsctxRccurveTo(c, dx1, dy1, dx2, dy2, dx3, dy3);
                CsctxRccurveTo(c, dx4, dy4, dx5, dy5, dx6, dy6);
                break;
            default:
                return LINEOS_CSERR("unimplemented");
            }
        }
        break;
        default:
            if (b0 != 255 && b0 != 28 && b0 < 32)
                return LINEOS_CSERR("reserved operator");
            if (b0 == 255)
            {
                f = (FLOAT32) (INT32) BUF_GET32(&b) / 0x10000;
            }
            else
            {
                BufSkip(&b, -1);
                f = (FLOAT32) (INT16) CffInt(&b);
            }
            if (sp >= 48)
                return LINEOS_CSERR("push stack overflow");
            s[sp++] = f;
            ClearStack = 0;
            break;
        }
        if (ClearStack)
            sp = 0;
    }
    return LINEOS_CSERR("no endchar");
#undef LINEOS_CSERR
}
STATIC INT32 GetGlyphShapeT2(CONST FONT_INFO *info, INT32 GlyphIndex, VERTEX **pvertices)
{
    CS_CONTEXT CountCtx = LINEOS_CSCTX_INIT(1);
    CS_CONTEXT OutputCtx = LINEOS_CSCTX_INIT(0);
    if (RunCharstring(info, GlyphIndex, &CountCtx))
    {
        *pvertices = (VERTEX *) KTTFAlloc(CountCtx.NumVertices * sizeof(VERTEX), info->userdata);
        OutputCtx.pvertices = *pvertices;
        if (RunCharstring(info, GlyphIndex, &OutputCtx))
        {
            KAssert(OutputCtx.NumVertices == CountCtx.NumVertices);
            return OutputCtx.NumVertices;
        }
    }
    *pvertices = NULL;
    return 0;
}
STATIC INT32 GetGlyphInfoT2(CONST FONT_INFO *info, INT32 GlyphIndex, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1)
{
    CS_CONTEXT c = LINEOS_CSCTX_INIT(1);
    INT32      r = RunCharstring(info, GlyphIndex, &c);
    if (x0)
        *x0 = r ? c.MinX : 0;
    if (y0)
        *y0 = r ? c.MinY : 0;
    if (x1)
        *x1 = r ? c.MaxX : 0;
    if (y1)
        *y1 = r ? c.MaxY : 0;
    return r ? c.NumVertices : 0;
}
EXTERN INT32 GetGlyphShape(CONST FONT_INFO *info, INT32 GlyphIndex, VERTEX **pvertices)
{
    if (!info->cff.size)
        return GetGlyphShapeTT(info, GlyphIndex, pvertices);
    else
        return GetGlyphShapeT2(info, GlyphIndex, pvertices);
}
EXTERN VOID GetGlyphHMetrics(CONST FONT_INFO *info, INT32 GlyphIndex, INT32 *AdvanceWidth, INT32 *LeftSideBearing)
{
    UINT16 NumOfLongHorMetrics = TTUSHORT(info->data + info->hhea + 34);
    if (GlyphIndex < NumOfLongHorMetrics)
    {
        if (AdvanceWidth)
            *AdvanceWidth = TTSHORT(info->data + info->hmtx + 4 * GlyphIndex);
        if (LeftSideBearing)
            *LeftSideBearing = TTSHORT(info->data + info->hmtx + 4 * GlyphIndex + 2);
    }
    else
    {
        if (AdvanceWidth)
            *AdvanceWidth = TTSHORT(info->data + info->hmtx + 4 * (NumOfLongHorMetrics - 1));
        if (LeftSideBearing)
            *LeftSideBearing = TTSHORT(info->data + info->hmtx + 4 * NumOfLongHorMetrics + 2 * (GlyphIndex - NumOfLongHorMetrics));
    }
}
EXTERN INT32 GetKerningTableLength(CONST FONT_INFO *info)
{
    UINT8 *data = info->data + info->kern;
    if (!info->kern)
        return 0;
    if (TTUSHORT(data + 2) < 1)
        return 0;
    if (TTUSHORT(data + 8) != 1)
        return 0;
    return TTUSHORT(data + 10);
}
EXTERN INT32 GetKerningTable(CONST FONT_INFO *info, KERNING_ENTRY *table, INT32 TableLength)
{
    UINT8 *data = info->data + info->kern;
    INT32  k, length;
    if (!info->kern)
        return 0;
    if (TTUSHORT(data + 2) < 1)
        return 0;
    if (TTUSHORT(data + 8) != 1)
        return 0;
    length = TTUSHORT(data + 10);
    if (TableLength < length)
        length = TableLength;
    for (k = 0; k < length; k++)
    {
        table[k].glyph1 = TTUSHORT(data + 18 + (k * 6));
        table[k].glyph2 = TTUSHORT(data + 20 + (k * 6));
        table[k].advance = TTSHORT(data + 22 + (k * 6));
    }
    return length;
}
STATIC INT32 GetGlyphKernInfoAdvance(CONST FONT_INFO *info, INT32 glyph1, INT32 glyph2)
{
    UINT8 *data = info->data + info->kern;
    UINT32 needle, straw;
    INT32  l, r, m;
    if (!info->kern)
        return 0;
    if (TTUSHORT(data + 2) < 1)
        return 0;
    if (TTUSHORT(data + 8) != 1)
        return 0;
    l = 0;
    r = TTUSHORT(data + 10) - 1;
    needle = glyph1 << 16 | glyph2;
    while (l <= r)
    {
        m = (l + r) >> 1;
        straw = TTULONG(data + 18 + (m * 6));
        if (needle < straw)
            r = m - 1;
        else if (needle > straw)
            l = m + 1;
        else
            return TTSHORT(data + 22 + (m * 6));
    }
    return 0;
}
STATIC INT32 GetCoverageIndex(UINT8 *CoverageTable, INT32 glyph)
{
    UINT16 CoverageFormat = TTUSHORT(CoverageTable);
    switch (CoverageFormat)
    {
    case 1:
    {
        UINT16 GlyphCount = TTUSHORT(CoverageTable + 2);
        INT32  l = 0, r = GlyphCount - 1, m;
        INT32  straw, needle = glyph;
        while (l <= r)
        {
            UINT8 *GlyphArray = CoverageTable + 4;
            UINT16 GlyphID;
            m = (l + r) >> 1;
            GlyphID = TTUSHORT(GlyphArray + 2 * m);
            straw = GlyphID;
            if (needle < straw)
                r = m - 1;
            else if (needle > straw)
                l = m + 1;
            else
            {
                return m;
            }
        }
        break;
    }
    case 2:
    {
        UINT16 RangeCount = TTUSHORT(CoverageTable + 2);
        UINT8 *RangeArray = CoverageTable + 4;
        INT32  l = 0, r = RangeCount - 1, m;
        INT32  StrawStart, StrawEnd, needle = glyph;
        while (l <= r)
        {
            UINT8 *RangeRecord;
            m = (l + r) >> 1;
            RangeRecord = RangeArray + 6 * m;
            StrawStart = TTUSHORT(RangeRecord);
            StrawEnd = TTUSHORT(RangeRecord + 2);
            if (needle < StrawStart)
                r = m - 1;
            else if (needle > StrawEnd)
                l = m + 1;
            else
            {
                UINT16 StartCoverageIndex = TTUSHORT(RangeRecord + 4);
                return StartCoverageIndex + glyph - StrawStart;
            }
        }
        break;
    }
    default:
        return -1;
    }
    return -1;
}
STATIC INT32 GetGlyphClass(UINT8 *ClassDefTable, INT32 glyph)
{
    UINT16 ClassDefFormat = TTUSHORT(ClassDefTable);
    switch (ClassDefFormat)
    {
    case 1:
    {
        UINT16 StartGlyphID = TTUSHORT(ClassDefTable + 2);
        UINT16 GlyphCount = TTUSHORT(ClassDefTable + 4);
        UINT8 *ClassDef1ValueArray = ClassDefTable + 6;
        if (glyph >= StartGlyphID && glyph < StartGlyphID + GlyphCount)
            return (INT32) TTUSHORT(ClassDef1ValueArray + 2 * (glyph - StartGlyphID));
        break;
    }
    case 2:
    {
        UINT16 ClassRangeCount = TTUSHORT(ClassDefTable + 2);
        UINT8 *ClassRangeRecords = ClassDefTable + 4;
        INT32  l = 0, r = ClassRangeCount - 1, m;
        INT32  StrawStart, StrawEnd, needle = glyph;
        while (l <= r)
        {
            UINT8 *ClassRangeRecord;
            m = (l + r) >> 1;
            ClassRangeRecord = ClassRangeRecords + 6 * m;
            StrawStart = TTUSHORT(ClassRangeRecord);
            StrawEnd = TTUSHORT(ClassRangeRecord + 2);
            if (needle < StrawStart)
                r = m - 1;
            else if (needle > StrawEnd)
                l = m + 1;
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
STATIC INT32 GetGlyphGPOSInfoAdvance(CONST FONT_INFO *info, INT32 glyph1, INT32 glyph2)
{
    UINT16 LookupListOffset;
    UINT8 *LookupList;
    UINT16 LookupCount;
    UINT8 *data;
    INT32  i, sti;
    if (!info->gpos)
        return 0;
    data = info->data + info->gpos;
    if (TTUSHORT(data + 0) != 1)
        return 0;
    if (TTUSHORT(data + 2) != 0)
        return 0;
    LookupListOffset = TTUSHORT(data + 8);
    LookupList = data + LookupListOffset;
    LookupCount = TTUSHORT(LookupList);
    for (i = 0; i < LookupCount; ++i)
    {
        UINT16 LookupOffset = TTUSHORT(LookupList + 2 + 2 * i);
        UINT8 *LookupTable = LookupList + LookupOffset;
        UINT16 LookupType = TTUSHORT(LookupTable);
        UINT16 SubTableCount = TTUSHORT(LookupTable + 4);
        UINT8 *SubTableOffsets = LookupTable + 6;
        if (LookupType != 2)
            continue;
        for (sti = 0; sti < SubTableCount; sti++)
        {
            UINT16 SubtableOffset = TTUSHORT(SubTableOffsets + 2 * sti);
            UINT8 *table = LookupTable + SubtableOffset;
            UINT16 PosFormat = TTUSHORT(table);
            UINT16 CoverageOffset = TTUSHORT(table + 2);
            INT32  CoverageIndex = GetCoverageIndex(table + CoverageOffset, glyph1);
            if (CoverageIndex == -1)
                continue;
            switch (PosFormat)
            {
            case 1:
            {
                INT32  l, r, m;
                INT32  straw, needle;
                UINT16 ValueFormat1 = TTUSHORT(table + 4);
                UINT16 ValueFormat2 = TTUSHORT(table + 6);
                if (ValueFormat1 == 4 && ValueFormat2 == 0)
                {
                    INT32  ValueRecordPairSizeInBytes = 2;
                    UINT16 PairSetCount = TTUSHORT(table + 8);
                    UINT16 PairPosOffset = TTUSHORT(table + 10 + 2 * CoverageIndex);
                    UINT8 *PairValueTable = table + PairPosOffset;
                    UINT16 PairValueCount = TTUSHORT(PairValueTable);
                    UINT8 *PairValueArray = PairValueTable + 2;
                    if (CoverageIndex >= PairSetCount)
                        return 0;
                    needle = glyph2;
                    r = PairValueCount - 1;
                    l = 0;
                    while (l <= r)
                    {
                        UINT16 SecondGlyph;
                        UINT8 *PairValue;
                        m = (l + r) >> 1;
                        PairValue = PairValueArray + (2 + ValueRecordPairSizeInBytes) * m;
                        SecondGlyph = TTUSHORT(PairValue);
                        straw = SecondGlyph;
                        if (needle < straw)
                            r = m - 1;
                        else if (needle > straw)
                            l = m + 1;
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
                UINT16 ValueFormat1 = TTUSHORT(table + 4);
                UINT16 ValueFormat2 = TTUSHORT(table + 6);
                if (ValueFormat1 == 4 && ValueFormat2 == 0)
                {
                    UINT16 ClassDef1Offset = TTUSHORT(table + 8);
                    UINT16 ClassDef2Offset = TTUSHORT(table + 10);
                    INT32  Glyph1Class = GetGlyphClass(table + ClassDef1Offset, glyph1);
                    INT32  Glyph2Class = GetGlyphClass(table + ClassDef2Offset, glyph2);
                    UINT16 Class1Count = TTUSHORT(table + 12);
                    UINT16 Class2Count = TTUSHORT(table + 14);
                    UINT8 *Class1Records, *Class2Records;
                    INT16  XAdvance;
                    if (Glyph1Class < 0 || Glyph1Class >= Class1Count)
                        return 0;
                    if (Glyph2Class < 0 || Glyph2Class >= Class2Count)
                        return 0;
                    Class1Records = table + 16;
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
EXTERN INT32 GetGlyphKernAdvance(CONST FONT_INFO *info, INT32 g1, INT32 g2)
{
    INT32 XAdvance = 0;
    if (info->gpos)
        XAdvance += GetGlyphGPOSInfoAdvance(info, g1, g2);
    else if (info->kern)
        XAdvance += GetGlyphKernInfoAdvance(info, g1, g2);
    return XAdvance;
}
EXTERN INT32 GetCodepointKernAdvance(CONST FONT_INFO *info, INT32 ch1, INT32 ch2)
{
    if (!info->kern && !info->gpos)
        return 0;
    return GetGlyphKernAdvance(info, FindGlyphIndex(info, ch1), FindGlyphIndex(info, ch2));
}
EXTERN VOID GetCodepointHMetrics(CONST FONT_INFO *info, INT32 codepoint, INT32 *AdvanceWidth, INT32 *LeftSideBearing)
{
    GetGlyphHMetrics(info, FindGlyphIndex(info, codepoint), AdvanceWidth, LeftSideBearing);
}
EXTERN VOID GetFontVMetrics(CONST FONT_INFO *info, INT32 *ascent, INT32 *descent, INT32 *LineGap)
{
    if (ascent)
        *ascent = TTSHORT(info->data + info->hhea + 4);
    if (descent)
        *descent = TTSHORT(info->data + info->hhea + 6);
    if (LineGap)
        *LineGap = TTSHORT(info->data + info->hhea + 8);
}
EXTERN INT32 GetFontVMetricsOS2(CONST FONT_INFO *info, INT32 *TypoAscent, INT32 *TypoDescent, INT32 *TypoLineGap)
{
    INT32 tab = FindTable(info->data, info->fontstart, "OS/2");
    if (!tab)
        return 0;
    if (TypoAscent)
        *TypoAscent = TTSHORT(info->data + tab + 68);
    if (TypoDescent)
        *TypoDescent = TTSHORT(info->data + tab + 70);
    if (TypoLineGap)
        *TypoLineGap = TTSHORT(info->data + tab + 72);
    return 1;
}
EXTERN VOID GetFontBoundingBox(CONST FONT_INFO *info, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1)
{
    *x0 = TTSHORT(info->data + info->head + 36);
    *y0 = TTSHORT(info->data + info->head + 38);
    *x1 = TTSHORT(info->data + info->head + 40);
    *y1 = TTSHORT(info->data + info->head + 42);
}
EXTERN FLOAT32 ScaleForPixelHeight(CONST FONT_INFO *info, FLOAT32 height)
{
    INT32 fheight = TTSHORT(info->data + info->hhea + 4) - TTSHORT(info->data + info->hhea + 6);
    return (FLOAT32) height / fheight;
}
EXTERN FLOAT32 ScaleForMappingEmToPixels(CONST FONT_INFO *info, FLOAT32 pixels)
{
    INT32 UnitsPerEm = TTUSHORT(info->data + info->head + 18);
    return pixels / UnitsPerEm;
}
EXTERN VOID FreeShape(CONST FONT_INFO *info, VERTEX *v)
{
    KTTFFree(v, info->userdata);
}
EXTERN UINT8 *FindSVGDoc(CONST FONT_INFO *info, INT32 gl)
{
    INT32  i;
    UINT8 *data = info->data;
    UINT8 *SVGDocList = data + GetSVG((FONT_INFO *) info);
    INT32  NumEntries = TTUSHORT(SVGDocList);
    UINT8 *SVGDocs = SVGDocList + 2;
    for (i = 0; i < NumEntries; i++)
    {
        UINT8 *SVGDoc = SVGDocs + (12 * i);
        if ((gl >= TTUSHORT(SVGDoc)) && (gl <= TTUSHORT(SVGDoc + 2)))
            return SVGDoc;
    }
    return 0;
}
EXTERN INT32 GetGlyphSVG(CONST FONT_INFO *info, INT32 gl, CONST CHAR8 **svg)
{
    UINT8 *data = info->data;
    UINT8 *SVGDoc;
    if (info->svg == 0)
        return 0;
    SVGDoc = FindSVGDoc(info, gl);
    if (SVGDoc != NULL)
    {
        *svg = (CHAR8 *) data + info->svg + TTULONG(SVGDoc + 4);
        return TTULONG(SVGDoc + 8);
    }
    else
    {
        return 0;
    }
}
EXTERN INT32 GetCodepointSVG(CONST FONT_INFO *info, INT32 UnicodeCodepoint, CONST CHAR8 **svg)
{
    return GetGlyphSVG(info, FindGlyphIndex(info, UnicodeCodepoint), svg);
}
EXTERN VOID GetGlyphBitmapBoxSubpixel(CONST FONT_INFO *font, INT32 glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1)
{
    INT32 x0 = 0, y0 = 0, x1, y1;
    if (!GetGlyphBox(font, glyph, &x0, &y0, &x1, &y1))
    {
        if (ix0)
            *ix0 = 0;
        if (iy0)
            *iy0 = 0;
        if (ix1)
            *ix1 = 0;
        if (iy1)
            *iy1 = 0;
    }
    else
    {
        if (ix0)
            *ix0 = KFloor(x0 * ScaleX + ShiftX);
        if (iy0)
            *iy0 = KFloor(-y1 * ScaleY + ShiftY);
        if (ix1)
            *ix1 = KCeil(x1 * ScaleX + ShiftX);
        if (iy1)
            *iy1 = KCeil(-y0 * ScaleY + ShiftY);
    }
}
EXTERN VOID GetGlyphBitmapBox(CONST FONT_INFO *font, INT32 glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1)
{
    GetGlyphBitmapBoxSubpixel(font, glyph, ScaleX, ScaleY, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
}
EXTERN VOID GetCodepointBitmapBoxSubpixel(CONST FONT_INFO *font, INT32 codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1)
{
    GetGlyphBitmapBoxSubpixel(font, FindGlyphIndex(font, codepoint), ScaleX, ScaleY, ShiftX, ShiftY, ix0, iy0, ix1, iy1);
}
EXTERN VOID GetCodepointBitmapBox(CONST FONT_INFO *font, INT32 codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1)
{
    GetCodepointBitmapBoxSubpixel(font, codepoint, ScaleX, ScaleY, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
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
STATIC VOID *HheapAlloc(HHEAP *hh, UINTN size, VOID *userdata)
{
    if (hh->FirstFree)
    {
        VOID *p = hh->FirstFree;
        hh->FirstFree = *(VOID **) p;
        return p;
    }
    else
    {
        if (hh->NumRemainingInHeadChunk == 0)
        {
            INT32        count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
            HHEAP_CHUNK *c = (HHEAP_CHUNK *) KTTFAlloc(sizeof(HHEAP_CHUNK) + size * count, userdata);
            if (c == NULL)
                return NULL;
            c->next = hh->head;
            hh->head = c;
            hh->NumRemainingInHeadChunk = count;
        }
        --hh->NumRemainingInHeadChunk;
        return (CHAR8 *) (hh->head) + sizeof(HHEAP_CHUNK) + size * hh->NumRemainingInHeadChunk;
    }
}
STATIC VOID HheapFree(HHEAP *hh, VOID *p)
{
    *(VOID **) p = hh->FirstFree;
    hh->FirstFree = p;
}
STATIC VOID HheapCleanup(HHEAP *hh, VOID *userdata)
{
    HHEAP_CHUNK *c = hh->head;
    while (c)
    {
        HHEAP_CHUNK *n = c->next;
        KTTFFree(c, userdata);
        c = n;
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
STATIC ACTIVE_EDGE *NewActive(HHEAP *hh, EDGE *e, INT32 OffX, FLOAT32 StartPoint, VOID *userdata)
{
    ACTIVE_EDGE *z = (ACTIVE_EDGE *) HheapAlloc(hh, sizeof(*z), userdata);
    FLOAT32      dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
    KAssert(z != NULL);
    if (!z)
        return z;
    z->fdx = dxdy;
    z->fdy = dxdy != 0.0f ? (1.0f / dxdy) : 0.0f;
    z->fx = e->x0 + dxdy * (StartPoint - e->y0);
    z->fx -= OffX;
    z->direction = e->invert ? 1.0f : -1.0f;
    z->sy = e->y0;
    z->ey = e->y1;
    z->next = 0;
    return z;
}
STATIC VOID HandleClippedEdge(FLOAT32 *scanline, INT32 x, ACTIVE_EDGE *e, FLOAT32 x0, FLOAT32 y0, FLOAT32 x1, FLOAT32 y1)
{
    if (y0 == y1)
        return;
    KAssert(y0 < y1);
    KAssert(e->sy <= e->ey);
    if (y0 > e->ey)
        return;
    if (y1 < e->sy)
        return;
    if (y0 < e->sy)
    {
        x0 += (x1 - x0) * (e->sy - y0) / (y1 - y0);
        y0 = e->sy;
    }
    if (y1 > e->ey)
    {
        x1 += (x1 - x0) * (e->ey - y1) / (y1 - y0);
        y1 = e->ey;
    }
    if (x0 == x)
        KAssert(x1 <= x + 1);
    else if (x0 == x + 1)
        KAssert(x1 >= x);
    else if (x0 <= x)
        KAssert(x1 <= x);
    else if (x0 >= x + 1)
        KAssert(x1 >= x + 1);
    else
        KAssert(x1 >= x && x1 <= x + 1);
    if (x0 <= x && x1 <= x)
        scanline[x] += e->direction * (y1 - y0);
    else if (x0 >= x + 1 && x1 >= x + 1)
        ;
    else
    {
        KAssert(x0 >= x && x0 <= x + 1 && x1 >= x && x1 <= x + 1);
        scanline[x] += e->direction * (y1 - y0) * (1 - ((x0 - x) + (x1 - x)) / 2);
    }
}
STATIC FLOAT32 SizedTrapezoidArea(FLOAT32 height, FLOAT32 TopWidth, FLOAT32 BottomWidth)
{
    KAssert(TopWidth >= 0);
    KAssert(BottomWidth >= 0);
    return (TopWidth + BottomWidth) / 2.0f * height;
}
STATIC FLOAT32 PositionTrapezoidArea(FLOAT32 height, FLOAT32 Tx0, FLOAT32 Tx1, FLOAT32 bx0, FLOAT32 bx1)
{
    return SizedTrapezoidArea(height, Tx1 - Tx0, bx1 - bx0);
}
STATIC FLOAT32 SizedTriangleArea(FLOAT32 height, FLOAT32 width)
{
    return height * width / 2;
}
STATIC VOID FillActiveEdgesNew(FLOAT32 *scanline, FLOAT32 *ScanlineFill, INT32 len, ACTIVE_EDGE *e, FLOAT32 YTop)
{
    FLOAT32 YBottom = YTop + 1;
    while (e)
    {
        KAssert(e->ey >= YTop);
        if (e->fdx == 0)
        {
            FLOAT32 x0 = e->fx;
            if (x0 < len)
            {
                if (x0 >= 0)
                {
                    HandleClippedEdge(scanline, (INT32) x0, e, x0, YTop, x0, YBottom);
                    HandleClippedEdge(ScanlineFill - 1, (INT32) x0 + 1, e, x0, YTop, x0, YBottom);
                }
                else
                {
                    HandleClippedEdge(ScanlineFill - 1, 0, e, x0, YTop, x0, YBottom);
                }
            }
        }
        else
        {
            FLOAT32 x0 = e->fx;
            FLOAT32 dx = e->fdx;
            FLOAT32 xb = x0 + dx;
            FLOAT32 XTop, XBottom;
            FLOAT32 Sy0, Sy1;
            FLOAT32 dy = e->fdy;
            KAssert(e->sy <= YBottom && e->ey >= YTop);
            if (e->sy > YTop)
            {
                XTop = x0 + dx * (e->sy - YTop);
                Sy0 = e->sy;
            }
            else
            {
                XTop = x0;
                Sy0 = YTop;
            }
            if (e->ey < YBottom)
            {
                XBottom = x0 + dx * (e->ey - YTop);
                Sy1 = e->ey;
            }
            else
            {
                XBottom = xb;
                Sy1 = YBottom;
            }
            if (XTop >= 0 && XBottom >= 0 && XTop < len && XBottom < len)
            {
                if ((INT32) XTop == (INT32) XBottom)
                {
                    FLOAT32 height;
                    INT32   x = (INT32) XTop;
                    height = (Sy1 - Sy0) * e->direction;
                    KAssert(x >= 0 && x < len);
                    scanline[x] += PositionTrapezoidArea(height, XTop, x + 1.0f, XBottom, x + 1.0f);
                    ScanlineFill[x] += height;
                }
                else
                {
                    INT32   x, x1, x2;
                    FLOAT32 YCrossing, YFinal, step, sign, area;
                    if (XTop > XBottom)
                    {
                        FLOAT32 t;
                        Sy0 = YBottom - (Sy0 - YTop);
                        Sy1 = YBottom - (Sy1 - YTop);
                        t = Sy0, Sy0 = Sy1, Sy1 = t;
                        t = XBottom, XBottom = XTop, XTop = t;
                        dx = -dx;
                        dy = -dy;
                        t = x0, x0 = xb, xb = t;
                    }
                    KAssert(dy >= 0);
                    KAssert(dx >= 0);
                    x1 = (INT32) XTop;
                    x2 = (INT32) XBottom;
                    YCrossing = YTop + dy * (x1 + 1 - x0);
                    YFinal = YTop + dy * (x2 - x0);
                    if (YCrossing > YBottom)
                        YCrossing = YBottom;
                    sign = e->direction;
                    area = sign * (YCrossing - Sy0);
                    scanline[x1] += SizedTriangleArea(area, x1 + 1 - XTop);
                    if (YFinal > YBottom)
                    {
                        YFinal = YBottom;
                        dy = (YFinal - YCrossing) / (x2 - (x1 + 1));
                    }
                    step = sign * dy * 1;
                    for (x = x1 + 1; x < x2; ++x)
                    {
                        scanline[x] += area + step / 2;
                        area += step;
                    }
                    KAssert(KFAbs(area) <= 1.01f);
                    KAssert(Sy1 > YFinal - 0.01f);
                    scanline[x2] += area + sign * PositionTrapezoidArea(Sy1 - YFinal, (FLOAT32) x2, x2 + 1.0f, XBottom, x2 + 1.0f);
                    ScanlineFill[x2] += sign * (Sy1 - Sy0);
                }
            }
            else
            {
                INT32 x;
                for (x = 0; x < len; ++x)
                {
                    FLOAT32 y0 = YTop;
                    FLOAT32 x1 = (FLOAT32) (x);
                    FLOAT32 x2 = (FLOAT32) (x + 1);
                    FLOAT32 x3 = xb;
                    FLOAT32 y3 = YBottom;
                    FLOAT32 y1 = (x - x0) / dx + YTop;
                    FLOAT32 y2 = (x + 1 - x0) / dx + YTop;
                    if (x0 < x1 && x3 > x2)
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x1, y1);
                        HandleClippedEdge(scanline, x, e, x1, y1, x2, y2);
                        HandleClippedEdge(scanline, x, e, x2, y2, x3, y3);
                    }
                    else if (x3 < x1 && x0 > x2)
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x2, y2);
                        HandleClippedEdge(scanline, x, e, x2, y2, x1, y1);
                        HandleClippedEdge(scanline, x, e, x1, y1, x3, y3);
                    }
                    else if (x0 < x1 && x3 > x1)
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x1, y1);
                        HandleClippedEdge(scanline, x, e, x1, y1, x3, y3);
                    }
                    else if (x3 < x1 && x0 > x1)
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x1, y1);
                        HandleClippedEdge(scanline, x, e, x1, y1, x3, y3);
                    }
                    else if (x0 < x2 && x3 > x2)
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x2, y2);
                        HandleClippedEdge(scanline, x, e, x2, y2, x3, y3);
                    }
                    else if (x3 < x2 && x0 > x2)
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x2, y2);
                        HandleClippedEdge(scanline, x, e, x2, y2, x3, y3);
                    }
                    else
                    {
                        HandleClippedEdge(scanline, x, e, x0, y0, x3, y3);
                    }
                }
            }
        }
        e = e->next;
    }
}
STATIC VOID RasterizeSortedEdges(BITMAP *result, EDGE *e, INT32 n, INT32 vsubsample, INT32 OffX, INT32 OffY, VOID *userdata)
{
    HHEAP        hh = {0, 0, 0};
    ACTIVE_EDGE *active = NULL;
    INT32        y, j = 0, i;
    FLOAT32      ScanlineData[129], *scanline, *Scanline2;
    LINEOS_NOTUSED(vsubsample);
    if (result->w > 64)
        scanline = (FLOAT32 *) KTTFAlloc((result->w * 2 + 1) * sizeof(FLOAT32), userdata);
    else
        scanline = ScanlineData;
    Scanline2 = scanline + result->w;
    y = OffY;
    e[n].y0 = (FLOAT32) (OffY + result->h) + 1;
    while (j < result->h)
    {
        FLOAT32       ScanYTop = y + 0.0f;
        FLOAT32       ScanYBottom = y + 1.0f;
        ACTIVE_EDGE **step = &active;
        KMemSet(scanline, 0, result->w * sizeof(scanline[0]));
        KMemSet(Scanline2, 0, (result->w + 1) * sizeof(scanline[0]));
        while (*step)
        {
            ACTIVE_EDGE *z = *step;
            if (z->ey <= ScanYTop)
            {
                *step = z->next;
                KAssert(z->direction);
                z->direction = 0;
                HheapFree(&hh, z);
            }
            else
            {
                step = &((*step)->next);
            }
        }
        while (e->y0 <= ScanYBottom)
        {
            if (e->y0 != e->y1)
            {
                ACTIVE_EDGE *z = NewActive(&hh, e, OffX, ScanYTop, userdata);
                if (z != NULL)
                {
                    if (j == 0 && OffY != 0)
                    {
                        if (z->ey < ScanYTop)
                        {
                            z->ey = ScanYTop;
                        }
                    }
                    KAssert(z->ey >= ScanYTop);
                    z->next = active;
                    active = z;
                }
            }
            ++e;
        }
        if (active)
            FillActiveEdgesNew(scanline, Scanline2 + 1, result->w, active, ScanYTop);
        {
            FLOAT32 sum = 0;
            for (i = 0; i < result->w; ++i)
            {
                FLOAT32 k;
                INT32   m;
                sum += Scanline2[i];
                k = scanline[i] + sum;
                k = (FLOAT32) KFAbs(k) * 255 + 0.5f;
                m = (INT32) k;
                if (m > 255)
                    m = 255;
                result->pixels[j * result->stride + i] = (UINT8) m;
            }
        }
        step = &active;
        while (*step)
        {
            ACTIVE_EDGE *z = *step;
            z->fx += z->fdx;
            step = &((*step)->next);
        }
        ++y;
        ++j;
    }
    HheapCleanup(&hh, userdata);
    if (scanline != ScanlineData)
        KTTFFree(scanline, userdata);
}
#define LINEOS_COMPARE(a, b) ((a)->y0 < (b)->y0)
STATIC VOID SortEdgesInsSort(EDGE *p, INT32 n)
{
    INT32 i, j;
    for (i = 1; i < n; ++i)
    {
        EDGE t = p[i], *a = &t;
        j = i;
        while (j > 0)
        {
            EDGE *b = &p[j - 1];
            INT32 c = LINEOS_COMPARE(a, b);
            if (!c)
                break;
            p[j] = p[j - 1];
            --j;
        }
        if (i != j)
            p[j] = t;
    }
}
STATIC VOID SortEdgesQuicksort(EDGE *p, INT32 n)
{
    while (n > 12)
    {
        EDGE  t;
        INT32 c01, c12, c, m, i, j;
        m = n >> 1;
        c01 = LINEOS_COMPARE(&p[0], &p[m]);
        c12 = LINEOS_COMPARE(&p[m], &p[n - 1]);
        if (c01 != c12)
        {
            INT32 z;
            c = LINEOS_COMPARE(&p[0], &p[n - 1]);
            z = (c == c12) ? 0 : n - 1;
            t = p[z];
            p[z] = p[m];
            p[m] = t;
        }
        t = p[0];
        p[0] = p[m];
        p[m] = t;
        i = 1;
        j = n - 1;
        for (;;)
        {
            for (;; ++i)
            {
                if (!LINEOS_COMPARE(&p[i], &p[0]))
                    break;
            }
            for (;; --j)
            {
                if (!LINEOS_COMPARE(&p[0], &p[j]))
                    break;
            }
            if (i >= j)
                break;
            t = p[i];
            p[i] = p[j];
            p[j] = t;
            ++i;
            --j;
        }
        if (j < (n - i))
        {
            SortEdgesQuicksort(p, j);
            p = p + i;
            n = n - i;
        }
        else
        {
            SortEdgesQuicksort(p + i, n - i);
            n = j;
        }
    }
}
STATIC VOID SortEdges(EDGE *p, INT32 n)
{
    SortEdgesQuicksort(p, n);
    SortEdgesInsSort(p, n);
}
typedef struct
{
    FLOAT32 x, y;
} POINT;
STATIC VOID RasterizeInternal(BITMAP *result, POINT *pts, INT32 *wcount, INT32 windings, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OffX, INT32 OffY, INT32 invert, VOID *userdata)
{
    FLOAT32 YScaleInv = invert ? -ScaleY : ScaleY;
    EDGE   *e;
    INT32   n, i, j, k, m;
    INT32   vsubsample = 1;
    n = 0;
    for (i = 0; i < windings; ++i)
        n += wcount[i];
    e = (EDGE *) KTTFAlloc(sizeof(*e) * (n + 1), userdata);
    if (e == 0)
        return;
    n = 0;
    m = 0;
    for (i = 0; i < windings; ++i)
    {
        POINT *p = pts + m;
        m += wcount[i];
        j = wcount[i] - 1;
        for (k = 0; k < wcount[i]; j = k++)
        {
            INT32 a = k, b = j;
            if (p[j].y == p[k].y)
                continue;
            e[n].invert = 0;
            if (invert ? p[j].y > p[k].y : p[j].y < p[k].y)
            {
                e[n].invert = 1;
                a = j, b = k;
            }
            e[n].x0 = p[a].x * ScaleX + ShiftX;
            e[n].y0 = (p[a].y * YScaleInv + ShiftY) * vsubsample;
            e[n].x1 = p[b].x * ScaleX + ShiftX;
            e[n].y1 = (p[b].y * YScaleInv + ShiftY) * vsubsample;
            ++n;
        }
    }
    SortEdges(e, n);
    RasterizeSortedEdges(result, e, n, vsubsample, OffX, OffY, userdata);
    KTTFFree(e, userdata);
}
STATIC VOID AddPoint(POINT *points, INT32 n, FLOAT32 x, FLOAT32 y)
{
    if (!points)
        return;
    points[n].x = x;
    points[n].y = y;
}
STATIC INT32 TesselateCurve(POINT *points, INT32 *NumPoints, FLOAT32 x0, FLOAT32 y0, FLOAT32 x1, FLOAT32 y1, FLOAT32 x2, FLOAT32 y2, FLOAT32 ObjspaceFlatnessSquared, INT32 n)
{
    FLOAT32 mx = (x0 + 2 * x1 + x2) / 4;
    FLOAT32 my = (y0 + 2 * y1 + y2) / 4;
    FLOAT32 dx = (x0 + x2) / 2 - mx;
    FLOAT32 dy = (y0 + y2) / 2 - my;
    if (n > 16)
        return 1;
    if (dx * dx + dy * dy > ObjspaceFlatnessSquared)
    {
        TesselateCurve(points, NumPoints, x0, y0, (x0 + x1) / 2.0f, (y0 + y1) / 2.0f, mx, my, ObjspaceFlatnessSquared, n + 1);
        TesselateCurve(points, NumPoints, mx, my, (x1 + x2) / 2.0f, (y1 + y2) / 2.0f, x2, y2, ObjspaceFlatnessSquared, n + 1);
    }
    else
    {
        AddPoint(points, *NumPoints, x2, y2);
        *NumPoints = *NumPoints + 1;
    }
    return 1;
}
STATIC VOID TesselateCubic(POINT *points, INT32 *NumPoints, FLOAT32 x0, FLOAT32 y0, FLOAT32 x1, FLOAT32 y1, FLOAT32 x2, FLOAT32 y2, FLOAT32 x3, FLOAT32 y3, FLOAT32 ObjspaceFlatnessSquared, INT32 n)
{
    FLOAT32 dx0 = x1 - x0;
    FLOAT32 dy0 = y1 - y0;
    FLOAT32 dx1 = x2 - x1;
    FLOAT32 dy1 = y2 - y1;
    FLOAT32 dx2 = x3 - x2;
    FLOAT32 dy2 = y3 - y2;
    FLOAT32 dx = x3 - x0;
    FLOAT32 dy = y3 - y0;
    FLOAT32 longlen = (FLOAT32) (KSqrt(dx0 * dx0 + dy0 * dy0) + KSqrt(dx1 * dx1 + dy1 * dy1) + KSqrt(dx2 * dx2 + dy2 * dy2));
    FLOAT32 shortlen = (FLOAT32) KSqrt(dx * dx + dy * dy);
    FLOAT32 FlatnessSquared = longlen * longlen - shortlen * shortlen;
    if (n > 16)
        return;
    if (FlatnessSquared > ObjspaceFlatnessSquared)
    {
        FLOAT32 X01 = (x0 + x1) / 2;
        FLOAT32 Y01 = (y0 + y1) / 2;
        FLOAT32 X12 = (x1 + x2) / 2;
        FLOAT32 Y12 = (y1 + y2) / 2;
        FLOAT32 X23 = (x2 + x3) / 2;
        FLOAT32 Y23 = (y2 + y3) / 2;
        FLOAT32 xa = (X01 + X12) / 2;
        FLOAT32 ya = (Y01 + Y12) / 2;
        FLOAT32 xb = (X12 + X23) / 2;
        FLOAT32 yb = (Y12 + Y23) / 2;
        FLOAT32 mx = (xa + xb) / 2;
        FLOAT32 my = (ya + yb) / 2;
        TesselateCubic(points, NumPoints, x0, y0, X01, Y01, xa, ya, mx, my, ObjspaceFlatnessSquared, n + 1);
        TesselateCubic(points, NumPoints, mx, my, xb, yb, X23, Y23, x3, y3, ObjspaceFlatnessSquared, n + 1);
    }
    else
    {
        AddPoint(points, *NumPoints, x3, y3);
        *NumPoints = *NumPoints + 1;
    }
}
STATIC POINT *FlattenCurves(VERTEX *vertices, INT32 NumVerts, FLOAT32 ObjspaceFlatness, INT32 **ContourLengths, INT32 *NumContours, VOID *userdata)
{
    POINT  *points = 0;
    INT32   NumPoints = 0;
    FLOAT32 ObjspaceFlatnessSquared = ObjspaceFlatness * ObjspaceFlatness;
    INT32   i, n = 0, start = 0, pass;
    for (i = 0; i < NumVerts; ++i)
        if (vertices[i].type == LINEOS_VMOVE)
            ++n;
    *NumContours = n;
    if (n == 0)
        return 0;
    *ContourLengths = (INT32 *) KTTFAlloc(sizeof(**ContourLengths) * n, userdata);
    if (*ContourLengths == 0)
    {
        *NumContours = 0;
        return 0;
    }
    for (pass = 0; pass < 2; ++pass)
    {
        FLOAT32 x = 0, y = 0;
        if (pass == 1)
        {
            points = (POINT *) KTTFAlloc(NumPoints * sizeof(points[0]), userdata);
            if (points == NULL)
                goto error;
        }
        NumPoints = 0;
        n = -1;
        for (i = 0; i < NumVerts; ++i)
        {
            switch (vertices[i].type)
            {
            case LINEOS_VMOVE:
                if (n >= 0)
                    (*ContourLengths)[n] = NumPoints - start;
                ++n;
                start = NumPoints;
                x = vertices[i].x, y = vertices[i].y;
                AddPoint(points, NumPoints++, x, y);
                break;
            case LINEOS_VLINE:
                x = vertices[i].x, y = vertices[i].y;
                AddPoint(points, NumPoints++, x, y);
                break;
            case LINEOS_VCURVE:
                TesselateCurve(points, &NumPoints, x, y, vertices[i].cx, vertices[i].cy, vertices[i].x, vertices[i].y, ObjspaceFlatnessSquared, 0);
                x = vertices[i].x, y = vertices[i].y;
                break;
            case LINEOS_VCUBIC:
                TesselateCubic(points, &NumPoints, x, y, vertices[i].cx, vertices[i].cy, vertices[i].cx1, vertices[i].cy1, vertices[i].x, vertices[i].y, ObjspaceFlatnessSquared, 0);
                x = vertices[i].x, y = vertices[i].y;
                break;
            }
        }
        (*ContourLengths)[n] = NumPoints - start;
    }
    return points;
error:
    KTTFFree(points, userdata);
    KTTFFree(*ContourLengths, userdata);
    *ContourLengths = 0;
    *NumContours = 0;
    return NULL;
}
EXTERN VOID Rasterize(BITMAP *result, FLOAT32 FlatnessInPixels, VERTEX *vertices, INT32 NumVerts, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 XOff, INT32 YOff, INT32 invert, VOID *userdata)
{
    FLOAT32 scale = ScaleX > ScaleY ? ScaleY : ScaleX;
    INT32   WindingCount = 0;
    INT32  *WindingLengths = NULL;
    POINT  *windings = FlattenCurves(vertices, NumVerts, FlatnessInPixels / scale, &WindingLengths, &WindingCount, userdata);
    if (windings)
    {
        RasterizeInternal(result, windings, WindingLengths, WindingCount, ScaleX, ScaleY, ShiftX, ShiftY, XOff, YOff, invert, userdata);
        KTTFFree(WindingLengths, userdata);
        KTTFFree(windings, userdata);
    }
}
EXTERN VOID FreeBitmap(UINT8 *bitmap, VOID *userdata)
{
    KTTFFree(bitmap, userdata);
}
EXTERN UINT8 *GetGlyphBitmapSubpixel(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 glyph, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff)
{
    INT32   ix0, iy0, ix1, iy1;
    BITMAP  gbm;
    VERTEX *vertices;
    INT32   NumVerts = GetGlyphShape(info, glyph, &vertices);
    if (ScaleX == 0)
        ScaleX = ScaleY;
    if (ScaleY == 0)
    {
        if (ScaleX == 0)
        {
            KTTFFree(vertices, info->userdata);
            return NULL;
        }
        ScaleY = ScaleX;
    }
    GetGlyphBitmapBoxSubpixel(info, glyph, ScaleX, ScaleY, ShiftX, ShiftY, &ix0, &iy0, &ix1, &iy1);
    gbm.w = (ix1 - ix0);
    gbm.h = (iy1 - iy0);
    gbm.pixels = NULL;
    if (width)
        *width = gbm.w;
    if (height)
        *height = gbm.h;
    if (xoff)
        *xoff = ix0;
    if (yoff)
        *yoff = iy0;
    if (gbm.w && gbm.h)
    {
        gbm.pixels = (UINT8 *) KTTFAlloc(gbm.w * gbm.h, info->userdata);
        if (gbm.pixels)
        {
            gbm.stride = gbm.w;
            Rasterize(&gbm, 0.35f, vertices, NumVerts, ScaleX, ScaleY, ShiftX, ShiftY, ix0, iy0, 1, info->userdata);
        }
    }
    KTTFFree(vertices, info->userdata);
    return gbm.pixels;
}
EXTERN UINT8 *GetGlyphBitmap(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 glyph, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff)
{
    return GetGlyphBitmapSubpixel(info, ScaleX, ScaleY, 0.0f, 0.0f, glyph, width, height, xoff, yoff);
}
EXTERN VOID MakeGlyphBitmapSubpixel(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 glyph)
{
    INT32   ix0, iy0;
    VERTEX *vertices;
    INT32   NumVerts = GetGlyphShape(info, glyph, &vertices);
    BITMAP  gbm;
    GetGlyphBitmapBoxSubpixel(info, glyph, ScaleX, ScaleY, ShiftX, ShiftY, &ix0, &iy0, 0, 0);
    gbm.pixels = output;
    gbm.w = OutW;
    gbm.h = OutH;
    gbm.stride = OutStride;
    if (gbm.w && gbm.h)
        Rasterize(&gbm, 0.35f, vertices, NumVerts, ScaleX, ScaleY, ShiftX, ShiftY, ix0, iy0, 1, info->userdata);
    KTTFFree(vertices, info->userdata);
}
EXTERN VOID MakeGlyphBitmap(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 glyph)
{
    MakeGlyphBitmapSubpixel(info, output, OutW, OutH, OutStride, ScaleX, ScaleY, 0.0f, 0.0f, glyph);
}
EXTERN UINT8 *GetCodepointBitmapSubpixel(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 codepoint, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff)
{
    return GetGlyphBitmapSubpixel(info, ScaleX, ScaleY, ShiftX, ShiftY, FindGlyphIndex(info, codepoint), width, height, xoff, yoff);
}
EXTERN VOID MakeCodepointBitmapSubpixelPrefilter(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OversampleX, INT32 OversampleY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 codepoint)
{
    MakeGlyphBitmapSubpixelPrefilter(info, output, OutW, OutH, OutStride, ScaleX, ScaleY, ShiftX, ShiftY, OversampleX, OversampleY, SubX, SubY, FindGlyphIndex(info, codepoint));
}
EXTERN VOID MakeCodepointBitmapSubpixel(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 codepoint)
{
    MakeGlyphBitmapSubpixel(info, output, OutW, OutH, OutStride, ScaleX, ScaleY, ShiftX, ShiftY, FindGlyphIndex(info, codepoint));
}
EXTERN UINT8 *GetCodepointBitmap(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 codepoint, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff)
{
    return GetCodepointBitmapSubpixel(info, ScaleX, ScaleY, 0.0f, 0.0f, codepoint, width, height, xoff, yoff);
}
EXTERN VOID MakeCodepointBitmap(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 codepoint)
{
    MakeCodepointBitmapSubpixel(info, output, OutW, OutH, OutStride, ScaleX, ScaleY, 0.0f, 0.0f, codepoint);
}
STATIC INT32 BakeFontBitmapInternal(UINT8 *data, INT32 offset, FLOAT32 PixelHeight, UINT8 *pixels, INT32 pw, INT32 ph, INT32 FirstChar, INT32 NumChars, BAKED_CHAR *chardata)
{
    FLOAT32   scale;
    INT32     x, y, BottomY, i;
    FONT_INFO f;
    f.userdata = NULL;
    if (!InitFont(&f, data, offset))
        return -1;
    KMemSet(pixels, 0, pw * ph);
    x = y = 1;
    BottomY = 1;
    scale = ScaleForPixelHeight(&f, PixelHeight);
    for (i = 0; i < NumChars; ++i)
    {
        INT32 advance, lsb, x0, y0, x1, y1, gw, gh;
        INT32 g = FindGlyphIndex(&f, FirstChar + i);
        GetGlyphHMetrics(&f, g, &advance, &lsb);
        GetGlyphBitmapBox(&f, g, scale, scale, &x0, &y0, &x1, &y1);
        gw = x1 - x0;
        gh = y1 - y0;
        if (x + gw + 1 >= pw)
            y = BottomY, x = 1;
        if (y + gh + 1 >= ph)
            return -i;
        KAssert(x + gw < pw);
        KAssert(y + gh < ph);
        MakeGlyphBitmap(&f, pixels + x + y * pw, gw, gh, pw, scale, scale, g);
        chardata[i].x0 = (INT16) x;
        chardata[i].y0 = (INT16) y;
        chardata[i].x1 = (INT16) (x + gw);
        chardata[i].y1 = (INT16) (y + gh);
        chardata[i].xadvance = scale * advance;
        chardata[i].xoff = (FLOAT32) x0;
        chardata[i].yoff = (FLOAT32) y0;
        x = x + gw + 1;
        if (y + gh + 1 > BottomY)
            BottomY = y + gh + 1;
    }
    return BottomY;
}
EXTERN VOID GetBakedQuad(CONST BAKED_CHAR *chardata, INT32 pw, INT32 ph, INT32 CharIndex, FLOAT32 *xpos, FLOAT32 *ypos, ALIGNED_QUAD *q, INT32 OpenglFillrule)
{
    FLOAT32           D3DBias = OpenglFillrule ? 0 : -0.5f;
    FLOAT32           ipw = 1.0f / pw, iph = 1.0f / ph;
    CONST BAKED_CHAR *b = chardata + CharIndex;
    INT32             RoundX = KFloor((*xpos + b->xoff) + 0.5f);
    INT32             RoundY = KFloor((*ypos + b->yoff) + 0.5f);
    q->x0 = RoundX + D3DBias;
    q->y0 = RoundY + D3DBias;
    q->x1 = RoundX + b->x1 - b->x0 + D3DBias;
    q->y1 = RoundY + b->y1 - b->y0 + D3DBias;
    q->s0 = b->x0 * ipw;
    q->T0 = b->y0 * iph;
    q->s1 = b->x1 * ipw;
    q->T1 = b->y1 * iph;
    *xpos += b->xadvance;
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
STATIC VOID STBRPInitTarget(STBRP_CONTEXT *con, INT32 pw, INT32 ph, STBRP_NODE *nodes, INT32 NumNodes)
{
    con->width = pw;
    con->height = ph;
    con->x = 0;
    con->y = 0;
    con->BottomY = 0;
    LINEOS_NOTUSED(nodes);
    LINEOS_NOTUSED(NumNodes);
}
STATIC VOID STBRPPackRects(STBRP_CONTEXT *con, STBRP_RECT *rects, INT32 NumRects)
{
    INT32 i;
    for (i = 0; i < NumRects; ++i)
    {
        if (con->x + rects[i].w > con->width)
        {
            con->x = 0;
            con->y = con->BottomY;
        }
        if (con->y + rects[i].h > con->height)
            break;
        rects[i].x = con->x;
        rects[i].y = con->y;
        rects[i].WasPacked = 1;
        con->x += rects[i].w;
        if (con->y + rects[i].h > con->BottomY)
            con->BottomY = con->y + rects[i].h;
    }
    for (; i < NumRects; ++i)
        rects[i].WasPacked = 0;
}
EXTERN INT32 PackBegin(PACK_CONTEXT *spc, UINT8 *pixels, INT32 pw, INT32 ph, INT32 StrideInBytes, INT32 padding, VOID *AllocContext)
{
    STBRP_CONTEXT *context = (STBRP_CONTEXT *) KTTFAlloc(sizeof(*context), AllocContext);
    INT32          NumNodes = pw - padding;
    STBRP_NODE    *nodes = (STBRP_NODE *) KTTFAlloc(sizeof(*nodes) * NumNodes, AllocContext);
    if (context == NULL || nodes == NULL)
    {
        if (context != NULL)
            KTTFFree(context, AllocContext);
        if (nodes != NULL)
            KTTFFree(nodes, AllocContext);
        return 0;
    }
    spc->UserAllocatorContext = AllocContext;
    spc->width = pw;
    spc->height = ph;
    spc->pixels = pixels;
    spc->PackInfo = context;
    spc->nodes = nodes;
    spc->padding = padding;
    spc->StrideInBytes = StrideInBytes != 0 ? StrideInBytes : pw;
    spc->HOversample = 1;
    spc->VOversample = 1;
    spc->SkipMissing = 0;
    STBRPInitTarget(context, pw - padding, ph - padding, nodes, NumNodes);
    if (pixels)
        KMemSet(pixels, 0, pw * ph);
    return 1;
}
EXTERN VOID PackEnd(PACK_CONTEXT *spc)
{
    KTTFFree(spc->nodes, spc->UserAllocatorContext);
    KTTFFree(spc->PackInfo, spc->UserAllocatorContext);
}
EXTERN VOID PackSetOversampling(PACK_CONTEXT *spc, UINT32 HOversample, UINT32 VOversample)
{
    KAssert(HOversample <= LINEOS_MAX_OVERSAMPLE);
    KAssert(VOversample <= LINEOS_MAX_OVERSAMPLE);
    if (HOversample <= LINEOS_MAX_OVERSAMPLE)
        spc->HOversample = HOversample;
    if (VOversample <= LINEOS_MAX_OVERSAMPLE)
        spc->VOversample = VOversample;
}
EXTERN VOID PackSetSkipMissingCodepoints(PACK_CONTEXT *spc, INT32 skip)
{
    spc->SkipMissing = skip;
}
#define LINEOS_OVER_MASK (LINEOS_MAX_OVERSAMPLE - 1)
STATIC VOID HPrefilter(UINT8 *pixels, INT32 w, INT32 h, INT32 StrideInBytes, UINT32 KernelWidth)
{
    UINT8 buffer[LINEOS_MAX_OVERSAMPLE];
    INT32 SafeW = w - KernelWidth;
    INT32 j;
    KMemSet(buffer, 0, LINEOS_MAX_OVERSAMPLE);
    for (j = 0; j < h; ++j)
    {
        INT32  i;
        UINT32 total;
        KMemSet(buffer, 0, KernelWidth);
        total = 0;
        switch (KernelWidth)
        {
        case 2:
            for (i = 0; i <= SafeW; ++i)
            {
                total += pixels[i] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i];
                pixels[i] = (UINT8) (total / 2);
            }
            break;
        case 3:
            for (i = 0; i <= SafeW; ++i)
            {
                total += pixels[i] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i];
                pixels[i] = (UINT8) (total / 3);
            }
            break;
        case 4:
            for (i = 0; i <= SafeW; ++i)
            {
                total += pixels[i] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i];
                pixels[i] = (UINT8) (total / 4);
            }
            break;
        case 5:
            for (i = 0; i <= SafeW; ++i)
            {
                total += pixels[i] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i];
                pixels[i] = (UINT8) (total / 5);
            }
            break;
        default:
            for (i = 0; i <= SafeW; ++i)
            {
                total += pixels[i] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i];
                pixels[i] = (UINT8) (total / KernelWidth);
            }
            break;
        }
        for (; i < w; ++i)
        {
            KAssert(pixels[i] == 0);
            total -= buffer[i & LINEOS_OVER_MASK];
            pixels[i] = (UINT8) (total / KernelWidth);
        }
        pixels += StrideInBytes;
    }
}
STATIC VOID VPrefilter(UINT8 *pixels, INT32 w, INT32 h, INT32 StrideInBytes, UINT32 KernelWidth)
{
    UINT8 buffer[LINEOS_MAX_OVERSAMPLE];
    INT32 SafeH = h - KernelWidth;
    INT32 j;
    KMemSet(buffer, 0, LINEOS_MAX_OVERSAMPLE);
    for (j = 0; j < w; ++j)
    {
        INT32  i;
        UINT32 total;
        KMemSet(buffer, 0, KernelWidth);
        total = 0;
        switch (KernelWidth)
        {
        case 2:
            for (i = 0; i <= SafeH; ++i)
            {
                total += pixels[i * StrideInBytes] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i * StrideInBytes];
                pixels[i * StrideInBytes] = (UINT8) (total / 2);
            }
            break;
        case 3:
            for (i = 0; i <= SafeH; ++i)
            {
                total += pixels[i * StrideInBytes] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i * StrideInBytes];
                pixels[i * StrideInBytes] = (UINT8) (total / 3);
            }
            break;
        case 4:
            for (i = 0; i <= SafeH; ++i)
            {
                total += pixels[i * StrideInBytes] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i * StrideInBytes];
                pixels[i * StrideInBytes] = (UINT8) (total / 4);
            }
            break;
        case 5:
            for (i = 0; i <= SafeH; ++i)
            {
                total += pixels[i * StrideInBytes] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i * StrideInBytes];
                pixels[i * StrideInBytes] = (UINT8) (total / 5);
            }
            break;
        default:
            for (i = 0; i <= SafeH; ++i)
            {
                total += pixels[i * StrideInBytes] - buffer[i & LINEOS_OVER_MASK];
                buffer[(i + KernelWidth) & LINEOS_OVER_MASK] = pixels[i * StrideInBytes];
                pixels[i * StrideInBytes] = (UINT8) (total / KernelWidth);
            }
            break;
        }
        for (; i < h; ++i)
        {
            KAssert(pixels[i * StrideInBytes] == 0);
            total -= buffer[i & LINEOS_OVER_MASK];
            pixels[i * StrideInBytes] = (UINT8) (total / KernelWidth);
        }
        pixels += 1;
    }
}
STATIC FLOAT32 OversampleShift(INT32 oversample)
{
    if (!oversample)
        return 0.0f;
    return (FLOAT32) - (oversample - 1) / (2.0f * (FLOAT32) oversample);
}
EXTERN INT32 PackFontRangesGatherRects(PACK_CONTEXT *spc, CONST FONT_INFO *info, PACK_RANGE *ranges, INT32 NumRanges, STBRP_RECT *rects)
{
    INT32 i, j, k;
    INT32 MissingGlyphAdded = 0;
    k = 0;
    for (i = 0; i < NumRanges; ++i)
    {
        FLOAT32 fh = ranges[i].FontSize;
        FLOAT32 scale = fh > 0 ? ScaleForPixelHeight(info, fh) : ScaleForMappingEmToPixels(info, -fh);
        ranges[i].HOversample = (UINT8) spc->HOversample;
        ranges[i].VOversample = (UINT8) spc->VOversample;
        for (j = 0; j < ranges[i].NumChars; ++j)
        {
            INT32 x0, y0, x1, y1;
            INT32 codepoint = ranges[i].ArrayOfUnicodeCodepoints == NULL ? ranges[i].FirstUnicodeCodepointInRange + j : ranges[i].ArrayOfUnicodeCodepoints[j];
            INT32 glyph = FindGlyphIndex(info, codepoint);
            if (glyph == 0 && (spc->SkipMissing || MissingGlyphAdded))
            {
                rects[k].w = rects[k].h = 0;
            }
            else
            {
                GetGlyphBitmapBoxSubpixel(info, glyph, scale * spc->HOversample, scale * spc->VOversample, 0, 0, &x0, &y0, &x1, &y1);
                rects[k].w = (STBRP_COORD) (x1 - x0 + spc->padding + spc->HOversample - 1);
                rects[k].h = (STBRP_COORD) (y1 - y0 + spc->padding + spc->VOversample - 1);
                if (glyph == 0)
                    MissingGlyphAdded = 1;
            }
            ++k;
        }
    }
    return k;
}
EXTERN VOID MakeGlyphBitmapSubpixelPrefilter(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 PrefilterX, INT32 PrefilterY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 glyph)
{
    MakeGlyphBitmapSubpixel(info, output, OutW - (PrefilterX - 1), OutH - (PrefilterY - 1), OutStride, ScaleX, ScaleY, ShiftX, ShiftY, glyph);
    if (PrefilterX > 1)
        HPrefilter(output, OutW, OutH, OutStride, PrefilterX);
    if (PrefilterY > 1)
        VPrefilter(output, OutW, OutH, OutStride, PrefilterY);
    *SubX = OversampleShift(PrefilterX);
    *SubY = OversampleShift(PrefilterY);
}
EXTERN INT32 PackFontRangesRenderIntoRects(PACK_CONTEXT *spc, CONST FONT_INFO *info, PACK_RANGE *ranges, INT32 NumRanges, STBRP_RECT *rects)
{
    INT32 i, j, k, MissingGlyph = -1, ReturnValue = 1;
    INT32 OldHOver = spc->HOversample;
    INT32 OldVOver = spc->VOversample;
    k = 0;
    for (i = 0; i < NumRanges; ++i)
    {
        FLOAT32 fh = ranges[i].FontSize;
        FLOAT32 scale = fh > 0 ? ScaleForPixelHeight(info, fh) : ScaleForMappingEmToPixels(info, -fh);
        FLOAT32 RecipH, RecipV, SubX, SubY;
        spc->HOversample = ranges[i].HOversample;
        spc->VOversample = ranges[i].VOversample;
        RecipH = 1.0f / spc->HOversample;
        RecipV = 1.0f / spc->VOversample;
        SubX = OversampleShift(spc->HOversample);
        SubY = OversampleShift(spc->VOversample);
        for (j = 0; j < ranges[i].NumChars; ++j)
        {
            STBRP_RECT *r = &rects[k];
            if (r->WasPacked && r->w != 0 && r->h != 0)
            {
                PACKED_CHAR *bc = &ranges[i].ChardataForRange[j];
                INT32        advance, lsb, x0, y0, x1, y1;
                INT32        codepoint = ranges[i].ArrayOfUnicodeCodepoints == NULL ? ranges[i].FirstUnicodeCodepointInRange + j : ranges[i].ArrayOfUnicodeCodepoints[j];
                INT32        glyph = FindGlyphIndex(info, codepoint);
                STBRP_COORD  pad = (STBRP_COORD) spc->padding;
                r->x += pad;
                r->y += pad;
                r->w -= pad;
                r->h -= pad;
                GetGlyphHMetrics(info, glyph, &advance, &lsb);
                GetGlyphBitmapBox(info, glyph, scale * spc->HOversample, scale * spc->VOversample, &x0, &y0, &x1, &y1);
                MakeGlyphBitmapSubpixel(info, spc->pixels + r->x + r->y * spc->StrideInBytes, r->w - spc->HOversample + 1, r->h - spc->VOversample + 1, spc->StrideInBytes, scale * spc->HOversample, scale * spc->VOversample, 0, 0, glyph);
                if (spc->HOversample > 1)
                    HPrefilter(spc->pixels + r->x + r->y * spc->StrideInBytes, r->w, r->h, spc->StrideInBytes, spc->HOversample);
                if (spc->VOversample > 1)
                    VPrefilter(spc->pixels + r->x + r->y * spc->StrideInBytes, r->w, r->h, spc->StrideInBytes, spc->VOversample);
                bc->x0 = (INT16) r->x;
                bc->y0 = (INT16) r->y;
                bc->x1 = (INT16) (r->x + r->w);
                bc->y1 = (INT16) (r->y + r->h);
                bc->xadvance = scale * advance;
                bc->xoff = (FLOAT32) x0 * RecipH + SubX;
                bc->yoff = (FLOAT32) y0 * RecipV + SubY;
                bc->Xoff2 = (x0 + r->w) * RecipH + SubX;
                bc->Yoff2 = (y0 + r->h) * RecipV + SubY;
                if (glyph == 0)
                    MissingGlyph = j;
            }
            else if (spc->SkipMissing)
            {
                ReturnValue = 0;
            }
            else if (r->WasPacked && r->w == 0 && r->h == 0 && MissingGlyph >= 0)
            {
                ranges[i].ChardataForRange[j] = ranges[i].ChardataForRange[MissingGlyph];
            }
            else
            {
                ReturnValue = 0;
            }
            ++k;
        }
    }
    spc->HOversample = OldHOver;
    spc->VOversample = OldVOver;
    return ReturnValue;
}
EXTERN VOID PackFontRangesPackRects(PACK_CONTEXT *spc, STBRP_RECT *rects, INT32 NumRects)
{
    STBRPPackRects((STBRP_CONTEXT *) spc->PackInfo, rects, NumRects);
}
EXTERN INT32 PackFontRanges(PACK_CONTEXT *spc, CONST UINT8 *fontdata, INT32 FontIndex, PACK_RANGE *ranges, INT32 NumRanges)
{
    FONT_INFO   info;
    INT32       i, j, n, ReturnValue = 1;
    STBRP_RECT *rects;
    for (i = 0; i < NumRanges; ++i)
        for (j = 0; j < ranges[i].NumChars; ++j)
            ranges[i].ChardataForRange[j].x0 = ranges[i].ChardataForRange[j].y0 = ranges[i].ChardataForRange[j].x1 = ranges[i].ChardataForRange[j].y1 = 0;
    n = 0;
    for (i = 0; i < NumRanges; ++i)
        n += ranges[i].NumChars;
    rects = (STBRP_RECT *) KTTFAlloc(sizeof(*rects) * n, spc->UserAllocatorContext);
    if (rects == NULL)
        return 0;
    info.userdata = spc->UserAllocatorContext;
    InitFont(&info, fontdata, GetFontOffsetForIndex(fontdata, FontIndex));
    n = PackFontRangesGatherRects(spc, &info, ranges, NumRanges, rects);
    PackFontRangesPackRects(spc, rects, n);
    ReturnValue = PackFontRangesRenderIntoRects(spc, &info, ranges, NumRanges, rects);
    KTTFFree(rects, spc->UserAllocatorContext);
    return ReturnValue;
}
EXTERN INT32 PackFontRange(PACK_CONTEXT *spc, CONST UINT8 *fontdata, INT32 FontIndex, FLOAT32 FontSize, INT32 FirstUnicodeCodepointInRange, INT32 NumCharsInRange, PACKED_CHAR *ChardataForRange)
{
    PACK_RANGE range;
    range.FirstUnicodeCodepointInRange = FirstUnicodeCodepointInRange;
    range.ArrayOfUnicodeCodepoints = NULL;
    range.NumChars = NumCharsInRange;
    range.ChardataForRange = ChardataForRange;
    range.FontSize = FontSize;
    return PackFontRanges(spc, fontdata, FontIndex, &range, 1);
}
EXTERN VOID GetScaledFontVMetrics(CONST UINT8 *fontdata, INT32 index, FLOAT32 size, FLOAT32 *ascent, FLOAT32 *descent, FLOAT32 *LineGap)
{
    INT32     IAscent, IDescent, ILineGap;
    FLOAT32   scale;
    FONT_INFO info;
    InitFont(&info, fontdata, GetFontOffsetForIndex(fontdata, index));
    scale = size > 0 ? ScaleForPixelHeight(&info, size) : ScaleForMappingEmToPixels(&info, -size);
    GetFontVMetrics(&info, &IAscent, &IDescent, &ILineGap);
    *ascent = (FLOAT32) IAscent * scale;
    *descent = (FLOAT32) IDescent * scale;
    *LineGap = (FLOAT32) ILineGap * scale;
}
EXTERN VOID GetPackedQuad(CONST PACKED_CHAR *chardata, INT32 pw, INT32 ph, INT32 CharIndex, FLOAT32 *xpos, FLOAT32 *ypos, ALIGNED_QUAD *q, INT32 AlignToInteger)
{
    FLOAT32            ipw = 1.0f / pw, iph = 1.0f / ph;
    CONST PACKED_CHAR *b = chardata + CharIndex;
    if (AlignToInteger)
    {
        FLOAT32 x = (FLOAT32) KFloor((*xpos + b->xoff) + 0.5f);
        FLOAT32 y = (FLOAT32) KFloor((*ypos + b->yoff) + 0.5f);
        q->x0 = x;
        q->y0 = y;
        q->x1 = x + b->Xoff2 - b->xoff;
        q->y1 = y + b->Yoff2 - b->yoff;
    }
    else
    {
        q->x0 = *xpos + b->xoff;
        q->y0 = *ypos + b->yoff;
        q->x1 = *xpos + b->Xoff2;
        q->y1 = *ypos + b->Yoff2;
    }
    q->s0 = b->x0 * ipw;
    q->T0 = b->y0 * iph;
    q->s1 = b->x1 * ipw;
    q->T1 = b->y1 * iph;
    *xpos += b->xadvance;
}
#define LINEOS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define LINEOS_MAX(a, b) ((a) < (b) ? (b) : (a))
STATIC INT32 RayIntersectBezier(FLOAT32 orig[2], FLOAT32 ray[2], FLOAT32 q0[2], FLOAT32 q1[2], FLOAT32 q2[2], FLOAT32 hits[2][2])
{
    FLOAT32 q0perp = q0[1] * ray[0] - q0[0] * ray[1];
    FLOAT32 q1perp = q1[1] * ray[0] - q1[0] * ray[1];
    FLOAT32 q2perp = q2[1] * ray[0] - q2[0] * ray[1];
    FLOAT32 roperp = orig[1] * ray[0] - orig[0] * ray[1];
    FLOAT32 a = q0perp - 2 * q1perp + q2perp;
    FLOAT32 b = q1perp - q0perp;
    FLOAT32 c = q0perp - roperp;
    FLOAT32 s0 = 0., s1 = 0.;
    INT32   NumS = 0;
    if (a != 0.0)
    {
        FLOAT32 discr = b * b - a * c;
        if (discr > 0.0)
        {
            FLOAT32 rcpna = -1 / a;
            FLOAT32 d = (FLOAT32) KSqrt(discr);
            s0 = (b + d) * rcpna;
            s1 = (b - d) * rcpna;
            if (s0 >= 0.0 && s0 <= 1.0)
                NumS = 1;
            if (d > 0.0 && s1 >= 0.0 && s1 <= 1.0)
            {
                if (NumS == 0)
                    s0 = s1;
                ++NumS;
            }
        }
    }
    else
    {
        s0 = c / (-2 * b);
        if (s0 >= 0.0 && s0 <= 1.0)
            NumS = 1;
    }
    if (NumS == 0)
        return 0;
    else
    {
        FLOAT32 RcpLen2 = 1 / (ray[0] * ray[0] + ray[1] * ray[1]);
        FLOAT32 RaynX = ray[0] * RcpLen2, RaynY = ray[1] * RcpLen2;
        FLOAT32 q0d = q0[0] * RaynX + q0[1] * RaynY;
        FLOAT32 q1d = q1[0] * RaynX + q1[1] * RaynY;
        FLOAT32 q2d = q2[0] * RaynX + q2[1] * RaynY;
        FLOAT32 rod = orig[0] * RaynX + orig[1] * RaynY;
        FLOAT32 q10d = q1d - q0d;
        FLOAT32 q20d = q2d - q0d;
        FLOAT32 q0rd = q0d - rod;
        hits[0][0] = q0rd + s0 * (2.0f - 2.0f * s0) * q10d + s0 * s0 * q20d;
        hits[0][1] = a * s0 + b;
        if (NumS > 1)
        {
            hits[1][0] = q0rd + s1 * (2.0f - 2.0f * s1) * q10d + s1 * s1 * q20d;
            hits[1][1] = a * s1 + b;
            return 2;
        }
        else
        {
            return 1;
        }
    }
}
STATIC INT32 Equal(FLOAT32 *a, FLOAT32 *b)
{
    return (a[0] == b[0] && a[1] == b[1]);
}
STATIC INT32 ComputeCrossingsX(FLOAT32 x, FLOAT32 y, INT32 nverts, VERTEX *verts)
{
    INT32   i;
    FLOAT32 orig[2], ray[2] = {1, 0};
    FLOAT32 YFrac;
    INT32   winding = 0;
    YFrac = (FLOAT32) KFMod(y, 1.0f);
    if (YFrac < 0.01f)
        y += 0.01f;
    else if (YFrac > 0.99f)
        y -= 0.01f;
    orig[0] = x;
    orig[1] = y;
    for (i = 0; i < nverts; ++i)
    {
        if (verts[i].type == LINEOS_VLINE)
        {
            INT32 x0 = (INT32) verts[i - 1].x, y0 = (INT32) verts[i - 1].y;
            INT32 x1 = (INT32) verts[i].x, y1 = (INT32) verts[i].y;
            if (y > LINEOS_MIN(y0, y1) && y < LINEOS_MAX(y0, y1) && x > LINEOS_MIN(x0, x1))
            {
                FLOAT32 XInter = (y - y0) / (y1 - y0) * (x1 - x0) + x0;
                if (XInter < x)
                    winding += (y0 < y1) ? 1 : -1;
            }
        }
        if (verts[i].type == LINEOS_VCURVE)
        {
            INT32 x0 = (INT32) verts[i - 1].x, y0 = (INT32) verts[i - 1].y;
            INT32 x1 = (INT32) verts[i].cx, y1 = (INT32) verts[i].cy;
            INT32 x2 = (INT32) verts[i].x, y2 = (INT32) verts[i].y;
            INT32 ax = LINEOS_MIN(x0, LINEOS_MIN(x1, x2)), ay = LINEOS_MIN(y0, LINEOS_MIN(y1, y2));
            INT32 by = LINEOS_MAX(y0, LINEOS_MAX(y1, y2));
            if (y > ay && y < by && x > ax)
            {
                FLOAT32 q0[2], q1[2], q2[2];
                FLOAT32 hits[2][2];
                q0[0] = (FLOAT32) x0;
                q0[1] = (FLOAT32) y0;
                q1[0] = (FLOAT32) x1;
                q1[1] = (FLOAT32) y1;
                q2[0] = (FLOAT32) x2;
                q2[1] = (FLOAT32) y2;
                if (Equal(q0, q1) || Equal(q1, q2))
                {
                    x0 = (INT32) verts[i - 1].x;
                    y0 = (INT32) verts[i - 1].y;
                    x1 = (INT32) verts[i].x;
                    y1 = (INT32) verts[i].y;
                    if (y > LINEOS_MIN(y0, y1) && y < LINEOS_MAX(y0, y1) && x > LINEOS_MIN(x0, x1))
                    {
                        FLOAT32 XInter = (y - y0) / (y1 - y0) * (x1 - x0) + x0;
                        if (XInter < x)
                            winding += (y0 < y1) ? 1 : -1;
                    }
                }
                else
                {
                    INT32 NumHits = RayIntersectBezier(orig, ray, q0, q1, q2, hits);
                    if (NumHits >= 1)
                        if (hits[0][0] < 0)
                            winding += (hits[0][1] < 0 ? -1 : 1);
                    if (NumHits >= 2)
                        if (hits[1][0] < 0)
                            winding += (hits[1][1] < 0 ? -1 : 1);
                }
            }
        }
    }
    return winding;
}
STATIC FLOAT32 Cuberoot(FLOAT32 x)
{
    if (x < 0)
        return -(FLOAT32) KPow(-x, 1.0f / 3.0f);
    else
        return (FLOAT32) KPow(x, 1.0f / 3.0f);
}
STATIC INT32 SolveCubic(FLOAT32 a, FLOAT32 b, FLOAT32 c, FLOAT32 *r)
{
    FLOAT32 s = -a / 3;
    FLOAT32 p = b - a * a / 3;
    FLOAT32 q = a * (2 * a * a - 9 * b) / 27 + c;
    FLOAT32 p3 = p * p * p;
    FLOAT32 d = q * q + 4 * p3 / 27;
    if (d >= 0)
    {
        FLOAT32 z = (FLOAT32) KSqrt(d);
        FLOAT32 u = (-q + z) / 2;
        FLOAT32 v = (-q - z) / 2;
        u = Cuberoot(u);
        v = Cuberoot(v);
        r[0] = s + u + v;
        return 1;
    }
    else
    {
        FLOAT32 u = (FLOAT32) KSqrt(-p / 3);
        FLOAT32 v = (FLOAT32) KACos(-KSqrt(-27 / p3) * q / 2) / 3;
        FLOAT32 m = (FLOAT32) KCos(v);
        FLOAT32 n = (FLOAT32) KCos(v - 3.141592 / 2) * 1.732050808f;
        r[0] = s + u * 2 * m;
        r[1] = s - u * (m + n);
        r[2] = s - u * (m - n);
        return 3;
    }
}
EXTERN UINT8 *GetGlyphSDF(CONST FONT_INFO *info, FLOAT32 scale, INT32 glyph, INT32 padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff)
{
    FLOAT32 ScaleX = scale, ScaleY = scale;
    INT32   ix0, iy0, ix1, iy1;
    INT32   w, h;
    UINT8  *data;
    if (scale == 0)
        return NULL;
    GetGlyphBitmapBoxSubpixel(info, glyph, scale, scale, 0.0f, 0.0f, &ix0, &iy0, &ix1, &iy1);
    if (ix0 == ix1 || iy0 == iy1)
        return NULL;
    ix0 -= padding;
    iy0 -= padding;
    ix1 += padding;
    iy1 += padding;
    w = (ix1 - ix0);
    h = (iy1 - iy0);
    if (width)
        *width = w;
    if (height)
        *height = h;
    if (xoff)
        *xoff = ix0;
    if (yoff)
        *yoff = iy0;
    ScaleY = -ScaleY;
    {
        CONST FLOAT32 eps = 1. / 1024, Eps2 = eps * eps;
        INT32         x, y, i, j;
        FLOAT32      *precompute;
        VERTEX       *verts;
        INT32         NumVerts = GetGlyphShape(info, glyph, &verts);
        data = (UINT8 *) KTTFAlloc(w * h, info->userdata);
        precompute = (FLOAT32 *) KTTFAlloc(NumVerts * sizeof(FLOAT32), info->userdata);
        for (i = 0, j = NumVerts - 1; i < NumVerts; j = i++)
        {
            if (verts[i].type == LINEOS_VLINE)
            {
                FLOAT32 x0 = verts[i].x * ScaleX, y0 = verts[i].y * ScaleY;
                FLOAT32 x1 = verts[j].x * ScaleX, y1 = verts[j].y * ScaleY;
                FLOAT32 dist = (FLOAT32) KSqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
                precompute[i] = (dist < eps) ? 0.0f : 1.0f / dist;
            }
            else if (verts[i].type == LINEOS_VCURVE)
            {
                FLOAT32 x2 = verts[j].x * ScaleX, y2 = verts[j].y * ScaleY;
                FLOAT32 x1 = verts[i].cx * ScaleX, y1 = verts[i].cy * ScaleY;
                FLOAT32 x0 = verts[i].x * ScaleX, y0 = verts[i].y * ScaleY;
                FLOAT32 bx = x0 - 2 * x1 + x2, by = y0 - 2 * y1 + y2;
                FLOAT32 len2 = bx * bx + by * by;
                if (len2 >= Eps2)
                    precompute[i] = 1.0f / len2;
                else
                    precompute[i] = 0.0f;
            }
            else
                precompute[i] = 0.0f;
        }
        for (y = iy0; y < iy1; ++y)
        {
            for (x = ix0; x < ix1; ++x)
            {
                FLOAT32 val;
                FLOAT32 MinDist = 999999.0f;
                FLOAT32 sx = (FLOAT32) x + 0.5f;
                FLOAT32 sy = (FLOAT32) y + 0.5f;
                FLOAT32 XGspace = (sx / ScaleX);
                FLOAT32 YGspace = (sy / ScaleY);
                INT32   winding = ComputeCrossingsX(XGspace, YGspace, NumVerts, verts);
                for (i = 0; i < NumVerts; ++i)
                {
                    FLOAT32 x0 = verts[i].x * ScaleX, y0 = verts[i].y * ScaleY;
                    if (verts[i].type == LINEOS_VLINE && precompute[i] != 0.0f)
                    {
                        FLOAT32 x1 = verts[i - 1].x * ScaleX, y1 = verts[i - 1].y * ScaleY;
                        FLOAT32 dist, dist2 = (x0 - sx) * (x0 - sx) + (y0 - sy) * (y0 - sy);
                        if (dist2 < MinDist * MinDist)
                            MinDist = (FLOAT32) KSqrt(dist2);
                        dist = (FLOAT32) KFAbs((x1 - x0) * (y0 - sy) - (y1 - y0) * (x0 - sx)) * precompute[i];
                        KAssert(i != 0);
                        if (dist < MinDist)
                        {
                            FLOAT32 dx = x1 - x0, dy = y1 - y0;
                            FLOAT32 px = x0 - sx, py = y0 - sy;
                            FLOAT32 t = -(px * dx + py * dy) / (dx * dx + dy * dy);
                            if (t >= 0.0f && t <= 1.0f)
                                MinDist = dist;
                        }
                    }
                    else if (verts[i].type == LINEOS_VCURVE)
                    {
                        FLOAT32 x2 = verts[i - 1].x * ScaleX, y2 = verts[i - 1].y * ScaleY;
                        FLOAT32 x1 = verts[i].cx * ScaleX, y1 = verts[i].cy * ScaleY;
                        FLOAT32 BoxX0 = LINEOS_MIN(LINEOS_MIN(x0, x1), x2);
                        FLOAT32 BoxY0 = LINEOS_MIN(LINEOS_MIN(y0, y1), y2);
                        FLOAT32 BoxX1 = LINEOS_MAX(LINEOS_MAX(x0, x1), x2);
                        FLOAT32 BoxY1 = LINEOS_MAX(LINEOS_MAX(y0, y1), y2);
                        if (sx > BoxX0 - MinDist && sx < BoxX1 + MinDist && sy > BoxY0 - MinDist && sy < BoxY1 + MinDist)
                        {
                            INT32   num = 0;
                            FLOAT32 ax = x1 - x0, ay = y1 - y0;
                            FLOAT32 bx = x0 - 2 * x1 + x2, by = y0 - 2 * y1 + y2;
                            FLOAT32 mx = x0 - sx, my = y0 - sy;
                            FLOAT32 res[3] = {0.f, 0.f, 0.f};
                            FLOAT32 px, py, t, it, dist2;
                            FLOAT32 AInv = precompute[i];
                            if (AInv == 0.0)
                            {
                                FLOAT32 a = 3 * (ax * bx + ay * by);
                                FLOAT32 b = 2 * (ax * ax + ay * ay) + (mx * bx + my * by);
                                FLOAT32 c = mx * ax + my * ay;
                                if (KFAbs(a) < Eps2)
                                {
                                    if (KFAbs(b) >= Eps2)
                                    {
                                        res[num++] = -c / b;
                                    }
                                }
                                else
                                {
                                    FLOAT32 discriminant = b * b - 4 * a * c;
                                    if (discriminant < 0)
                                        num = 0;
                                    else
                                    {
                                        FLOAT32 root = (FLOAT32) KSqrt(discriminant);
                                        res[0] = (-b - root) / (2 * a);
                                        res[1] = (-b + root) / (2 * a);
                                        num = 2;
                                    }
                                }
                            }
                            else
                            {
                                FLOAT32 b = 3 * (ax * bx + ay * by) * AInv;
                                FLOAT32 c = (2 * (ax * ax + ay * ay) + (mx * bx + my * by)) * AInv;
                                FLOAT32 d = (mx * ax + my * ay) * AInv;
                                num = SolveCubic(b, c, d, res);
                            }
                            dist2 = (x0 - sx) * (x0 - sx) + (y0 - sy) * (y0 - sy);
                            if (dist2 < MinDist * MinDist)
                                MinDist = (FLOAT32) KSqrt(dist2);
                            if (num >= 1 && res[0] >= 0.0f && res[0] <= 1.0f)
                            {
                                t = res[0], it = 1.0f - t;
                                px = it * it * x0 + 2 * t * it * x1 + t * t * x2;
                                py = it * it * y0 + 2 * t * it * y1 + t * t * y2;
                                dist2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
                                if (dist2 < MinDist * MinDist)
                                    MinDist = (FLOAT32) KSqrt(dist2);
                            }
                            if (num >= 2 && res[1] >= 0.0f && res[1] <= 1.0f)
                            {
                                t = res[1], it = 1.0f - t;
                                px = it * it * x0 + 2 * t * it * x1 + t * t * x2;
                                py = it * it * y0 + 2 * t * it * y1 + t * t * y2;
                                dist2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
                                if (dist2 < MinDist * MinDist)
                                    MinDist = (FLOAT32) KSqrt(dist2);
                            }
                            if (num >= 3 && res[2] >= 0.0f && res[2] <= 1.0f)
                            {
                                t = res[2], it = 1.0f - t;
                                px = it * it * x0 + 2 * t * it * x1 + t * t * x2;
                                py = it * it * y0 + 2 * t * it * y1 + t * t * y2;
                                dist2 = (px - sx) * (px - sx) + (py - sy) * (py - sy);
                                if (dist2 < MinDist * MinDist)
                                    MinDist = (FLOAT32) KSqrt(dist2);
                            }
                        }
                    }
                }
                if (winding == 0)
                    MinDist = -MinDist;
                val = OnedgeValue + PixelDistScale * MinDist;
                if (val < 0)
                    val = 0;
                else if (val > 255)
                    val = 255;
                data[(y - iy0) * w + (x - ix0)] = (UINT8) val;
            }
        }
        KTTFFree(precompute, info->userdata);
        KTTFFree(verts, info->userdata);
    }
    return data;
}
EXTERN UINT8 *GetCodepointSDF(CONST FONT_INFO *info, FLOAT32 scale, INT32 codepoint, INT32 padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff)
{
    return GetGlyphSDF(info, scale, FindGlyphIndex(info, codepoint), padding, OnedgeValue, PixelDistScale, width, height, xoff, yoff);
}
EXTERN VOID FreeSDF(UINT8 *bitmap, VOID *userdata)
{
    KTTFFree(bitmap, userdata);
}
EXTERN INT32 BakeFontBitmap(CONST UINT8 *data, INT32 offset, FLOAT32 PixelHeight, UINT8 *pixels, INT32 pw, INT32 ph, INT32 FirstChar, INT32 NumChars, BAKED_CHAR *chardata)
{
    return BakeFontBitmapInternal((UINT8 *) data, offset, PixelHeight, pixels, pw, ph, FirstChar, NumChars, chardata);
}
EXTERN INT32 GetFontOffsetForIndex(CONST UINT8 *data, INT32 index)
{
    return GetFontOffsetForIndexInternal((UINT8 *) data, index);
}
EXTERN INT32 GetNumberOfFonts(CONST UINT8 *data)
{
    return GetNumberOfFontsInternal((UINT8 *) data);
}
EXTERN INT32 InitFont(FONT_INFO *info, CONST UINT8 *data, INT32 offset)
{
    return InitFontInternal(info, (UINT8 *) data, offset);
}
