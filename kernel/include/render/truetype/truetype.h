// kernel/include/render/truetype/truetype.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04
// Original from stb_truetype.h
#pragma once

#include <lineos/typeinfo.h>

#define LINEOS_POINT_SIZE(x) (-(x))
#define VERTEX_TYPE          short

typedef struct
{
    UINT8 *data;
    INT32  cursor;
    INT32  size;
} BUFFER;

typedef struct
{
    UINT16  x0, y0, x1, y1;
    FLOAT32 xoff, yoff, xadvance;
} BAKED_CHAR;

typedef struct
{
    INT32  w, h, stride;
    UINT8 *pixels;
} BITMAP;

typedef struct
{
    FLOAT32 x0, y0, s0, T0;
    FLOAT32 x1, y1, s1, T1;
} ALIGNED_QUAD;

typedef struct
{
    UINT16  x0, y0, x1, y1;
    FLOAT32 xoff, yoff, xadvance;
    FLOAT32 Xoff2, Yoff2;
} PACKED_CHAR;

typedef struct PACK_CONTEXT PACK_CONTEXT;
typedef struct FONT_INFO    FONT_INFO;
typedef struct STBRP_RECT   STBRP_RECT;

typedef struct
{
    FLOAT32      FontSize;
    INT32        FirstUnicodeCodepointInRange;
    INT32       *ArrayOfUnicodeCodepoints;
    INT32        NumChars;
    PACKED_CHAR *ChardataForRange;
    UINT8        HOversample, VOversample;
} PACK_RANGE;

struct PACK_CONTEXT
{
    VOID  *UserAllocatorContext;
    VOID  *PackInfo;
    INT32  width;
    INT32  height;
    INT32  StrideInBytes;
    INT32  padding;
    INT32  SkipMissing;
    UINT32 HOversample, VOversample;
    UINT8 *pixels;
    VOID  *nodes;
};

enum
{
    LINEOS_VMOVE = 1,
    LINEOS_VLINE,
    LINEOS_VCURVE,
    LINEOS_VCUBIC
};

typedef struct
{
    VERTEX_TYPE x, y, cx, cy, cx1, cy1;
    UINT8       type, padding;
} VERTEX;

struct FONT_INFO
{
    VOID  *userdata;
    UINT8 *data;
    INT32  fontstart;
    INT32  NumGlyphs;
    INT32  loca, head, glyf, hhea, hmtx, kern, gpos, svg;
    INT32  IndexMap;
    INT32  IndexToLocFormat;
    BUFFER cff;
    BUFFER charstrings;
    BUFFER gsubrs;
    BUFFER subrs;
    BUFFER fontdicts;
    BUFFER fdselect;
};

typedef struct KERNING_ENTRY
{
    INT32 glyph1;
    INT32 glyph2;
    INT32 advance;
} KERNING_ENTRY;

EXTERN INT32   BakeFontBitmap(CONST UINT8 *data, INT32 offset, FLOAT32 PixelHeight, UINT8 *pixels, INT32 pw, INT32 ph, INT32 FirstChar, INT32 NumChars, BAKED_CHAR *chardata);
EXTERN VOID    GetBakedQuad(CONST BAKED_CHAR *chardata, INT32 pw, INT32 ph, INT32 CharIndex, FLOAT32 *xpos, FLOAT32 *ypos, ALIGNED_QUAD *q, INT32 OpenglFillrule);
EXTERN VOID    GetScaledFontVMetrics(CONST UINT8 *fontdata, INT32 index, FLOAT32 size, FLOAT32 *ascent, FLOAT32 *descent, FLOAT32 *LineGap);
EXTERN INT32   PackBegin(PACK_CONTEXT *spc, UINT8 *pixels, INT32 width, INT32 height, INT32 StrideInBytes, INT32 padding, VOID *AllocContext);
EXTERN VOID    PackEnd(PACK_CONTEXT *spc);
EXTERN INT32   PackFontRange(PACK_CONTEXT *spc, CONST UINT8 *fontdata, INT32 FontIndex, FLOAT32 FontSize, INT32 FirstUnicodeCharInRange, INT32 NumCharsInRange, PACKED_CHAR *ChardataForRange);
EXTERN INT32   PackFontRanges(PACK_CONTEXT *spc, CONST UINT8 *fontdata, INT32 FontIndex, PACK_RANGE *ranges, INT32 NumRanges);
EXTERN VOID    PackSetOversampling(PACK_CONTEXT *spc, UINT32 HOversample, UINT32 VOversample);
EXTERN VOID    PackSetSkipMissingCodepoints(PACK_CONTEXT *spc, INT32 skip);
EXTERN VOID    GetPackedQuad(CONST PACKED_CHAR *chardata, INT32 pw, INT32 ph, INT32 CharIndex, FLOAT32 *xpos, FLOAT32 *ypos, ALIGNED_QUAD *q, INT32 AlignToInteger);
EXTERN INT32   PackFontRangesGatherRects(PACK_CONTEXT *spc, CONST FONT_INFO *info, PACK_RANGE *ranges, INT32 NumRanges, STBRP_RECT *rects);
EXTERN VOID    PackFontRangesPackRects(PACK_CONTEXT *spc, STBRP_RECT *rects, INT32 NumRects);
EXTERN INT32   PackFontRangesRenderIntoRects(PACK_CONTEXT *spc, CONST FONT_INFO *info, PACK_RANGE *ranges, INT32 NumRanges, STBRP_RECT *rects);
EXTERN INT32   GetNumberOfFonts(CONST UINT8 *data);
EXTERN INT32   GetFontOffsetForIndex(CONST UINT8 *data, INT32 index);
EXTERN INT32   InitFont(FONT_INFO *info, CONST UINT8 *data, INT32 offset);
EXTERN INT32   FindGlyphIndex(CONST FONT_INFO *info, INT32 UnicodeCodepoint);
EXTERN FLOAT32 ScaleForPixelHeight(CONST FONT_INFO *info, FLOAT32 pixels);
EXTERN FLOAT32 ScaleForMappingEmToPixels(CONST FONT_INFO *info, FLOAT32 pixels);
EXTERN VOID    GetFontVMetrics(CONST FONT_INFO *info, INT32 *ascent, INT32 *descent, INT32 *LineGap);
EXTERN INT32   GetFontVMetricsOS2(CONST FONT_INFO *info, INT32 *TypoAscent, INT32 *TypoDescent, INT32 *TypoLineGap);
EXTERN VOID    GetFontBoundingBox(CONST FONT_INFO *info, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1);
EXTERN VOID    GetCodepointHMetrics(CONST FONT_INFO *info, INT32 codepoint, INT32 *AdvanceWidth, INT32 *LeftSideBearing);
EXTERN INT32   GetCodepointKernAdvance(CONST FONT_INFO *info, INT32 ch1, INT32 ch2);
EXTERN INT32   GetCodepointBox(CONST FONT_INFO *info, INT32 codepoint, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1);
EXTERN VOID    GetGlyphHMetrics(CONST FONT_INFO *info, INT32 GlyphIndex, INT32 *AdvanceWidth, INT32 *LeftSideBearing);
EXTERN INT32   GetGlyphKernAdvance(CONST FONT_INFO *info, INT32 glyph1, INT32 glyph2);
EXTERN INT32   GetGlyphBox(CONST FONT_INFO *info, INT32 GlyphIndex, INT32 *x0, INT32 *y0, INT32 *x1, INT32 *y1);
EXTERN INT32   GetKerningTableLength(CONST FONT_INFO *info);
EXTERN INT32   GetKerningTable(CONST FONT_INFO *info, KERNING_ENTRY *table, INT32 TableLength);
EXTERN INT32   IsGlyphEmpty(CONST FONT_INFO *info, INT32 GlyphIndex);
EXTERN INT32   GetCodepointShape(CONST FONT_INFO *info, INT32 UnicodeCodepoint, VERTEX **vertices);
EXTERN INT32   GetGlyphShape(CONST FONT_INFO *info, INT32 GlyphIndex, VERTEX **vertices);
EXTERN VOID    FreeShape(CONST FONT_INFO *info, VERTEX *vertices);
EXTERN UINT8  *FindSVGDoc(CONST FONT_INFO *info, INT32 gl);
EXTERN INT32   GetCodepointSVG(CONST FONT_INFO *info, INT32 UnicodeCodepoint, CONST CHAR8 **svg);
EXTERN INT32   GetGlyphSVG(CONST FONT_INFO *info, INT32 gl, CONST CHAR8 **svg);
EXTERN VOID    FreeBitmap(UINT8 *bitmap, VOID *userdata);
EXTERN UINT8  *GetCodepointBitmap(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 codepoint, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff);
EXTERN UINT8  *GetCodepointBitmapSubpixel(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 codepoint, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff);
EXTERN VOID    MakeCodepointBitmap(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 codepoint);
EXTERN VOID    MakeCodepointBitmapSubpixel(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 codepoint);
EXTERN VOID    MakeCodepointBitmapSubpixelPrefilter(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OversampleX, INT32 OversampleY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 codepoint);
EXTERN VOID    GetCodepointBitmapBox(CONST FONT_INFO *font, INT32 codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1);
EXTERN VOID    GetCodepointBitmapBoxSubpixel(CONST FONT_INFO *font, INT32 codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1);
EXTERN UINT8  *GetGlyphBitmap(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 glyph, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff);
EXTERN UINT8  *GetGlyphBitmapSubpixel(CONST FONT_INFO *info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 glyph, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff);
EXTERN VOID    MakeGlyphBitmap(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 glyph);
EXTERN VOID    MakeGlyphBitmapSubpixel(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 glyph);
EXTERN VOID    MakeGlyphBitmapSubpixelPrefilter(CONST FONT_INFO *info, UINT8 *output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OversampleX, INT32 OversampleY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 glyph);
EXTERN VOID    GetGlyphBitmapBox(CONST FONT_INFO *font, INT32 glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1);
EXTERN VOID    GetGlyphBitmapBoxSubpixel(CONST FONT_INFO *font, INT32 glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *ix0, INT32 *iy0, INT32 *ix1, INT32 *iy1);
EXTERN VOID    Rasterize(BITMAP *result, FLOAT32 FlatnessInPixels, VERTEX *vertices, INT32 NumVerts, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 XOff, INT32 YOff, INT32 invert, VOID *userdata);
EXTERN VOID    FreeSDF(UINT8 *bitmap, VOID *userdata);
EXTERN UINT8  *GetGlyphSDF(CONST FONT_INFO *info, FLOAT32 scale, INT32 glyph, INT32 padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff);
EXTERN UINT8  *GetCodepointSDF(CONST FONT_INFO *info, FLOAT32 scale, INT32 codepoint, INT32 padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *width, INT32 *height, INT32 *xoff, INT32 *yoff);
