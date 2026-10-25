// Libraries/Include/TrueType/TrueType.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04
// Original from stb_truetype.h
#pragma once

#include <LOSLibC/TypeInfo.h>

#define LINEOS_POINT_SIZE(x) (-(x))
#define VERTEX_TYPE          short

typedef struct
{
    UINT8 *Data;
    INT32  Cursor;
    INT32  Size;
} BUFFER;

typedef struct
{
    UINT16  X0, Y0, X1, Y1;
    FLOAT32 Xoff, Yoff, Xadvance;
} BAKED_CHAR;

typedef struct
{
    INT32  W, H, Stride;
    UINT8 *Pixels;
} BITMAP;

typedef struct
{
    FLOAT32 X0, Y0, S0, T0;
    FLOAT32 X1, Y1, S1, T1;
} ALIGNED_QUAD;

typedef struct
{
    UINT16  X0, Y0, X1, Y1;
    FLOAT32 Xoff, Yoff, Xadvance;
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
    INT32  Width;
    INT32  Height;
    INT32  StrideInBytes;
    INT32  Padding;
    INT32  SkipMissing;
    UINT32 HOversample, VOversample;
    UINT8 *Pixels;
    VOID  *Nodes;
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
    VERTEX_TYPE X, Y, Cx, Cy, Cx1, Cy1;
    UINT8       Type, Padding;
} VERTEX;

struct FONT_INFO
{
    VOID  *UserData;
    UINT8 *Data;
    INT32  FontStart;
    INT32  NumGlyphs;
    INT32  Loca, Head, Glyf, Hhea, Hmtx, Kern, GPOS, Svg;
    INT32  IndexMap;
    INT32  IndexToLocFormat;
    BUFFER CFF;
    BUFFER CharStrings;
    BUFFER GSubrs;
    BUFFER Subrs;
    BUFFER FontDicts;
    BUFFER FdSelect;
};

typedef struct KERNING_ENTRY
{
    INT32 Glyph1;
    INT32 Glyph2;
    INT32 Advance;
} KERNING_ENTRY;

EXTERN INT32   BakeFontBitmap(CONST UINT8 *Data, INT32 Offset, FLOAT32 PixelHeight, UINT8 *Pixels, INT32 Pw, INT32 Ph, INT32 FirstChar, INT32 NumChars, BAKED_CHAR *Chardata);
EXTERN VOID    GetBakedQuad(CONST BAKED_CHAR *Chardata, INT32 Pw, INT32 Ph, INT32 CharIndex, FLOAT32 *Xpos, FLOAT32 *Ypos, ALIGNED_QUAD *Q, INT32 OpenglFillrule);
EXTERN VOID    GetScaledFontVMetrics(CONST UINT8 *Fontdata, INT32 Index, FLOAT32 Size, FLOAT32 *Ascent, FLOAT32 *Descent, FLOAT32 *LineGap);
EXTERN INT32   PackBegin(PACK_CONTEXT *Spc, UINT8 *Pixels, INT32 Width, INT32 Height, INT32 StrideInBytes, INT32 Padding, VOID *AllocContext);
EXTERN VOID    PackEnd(PACK_CONTEXT *Spc);
EXTERN INT32   PackFontRange(PACK_CONTEXT *Spc, CONST UINT8 *Fontdata, INT32 FontIndex, FLOAT32 FontSize, INT32 FirstUnicodeCharInRange, INT32 NumCharsInRange, PACKED_CHAR *ChardataForRange);
EXTERN INT32   PackFontRanges(PACK_CONTEXT *Spc, CONST UINT8 *Fontdata, INT32 FontIndex, PACK_RANGE *Ranges, INT32 NumRanges);
EXTERN VOID    PackSetOversampling(PACK_CONTEXT *Spc, UINT32 HOversample, UINT32 VOversample);
EXTERN VOID    PackSetSkipMissingCodepoints(PACK_CONTEXT *Spc, INT32 Skip);
EXTERN VOID    GetPackedQuad(CONST PACKED_CHAR *Chardata, INT32 Pw, INT32 Ph, INT32 CharIndex, FLOAT32 *Xpos, FLOAT32 *Ypos, ALIGNED_QUAD *Q, INT32 AlignToInteger);
EXTERN INT32   PackFontRangesGatherRects(PACK_CONTEXT *Spc, CONST FONT_INFO *Info, PACK_RANGE *Ranges, INT32 NumRanges, STBRP_RECT *Rects);
EXTERN VOID    PackFontRangesPackRects(PACK_CONTEXT *Spc, STBRP_RECT *Rects, INT32 NumRects);
EXTERN INT32   PackFontRangesRenderIntoRects(PACK_CONTEXT *Spc, CONST FONT_INFO *Info, PACK_RANGE *Ranges, INT32 NumRanges, STBRP_RECT *Rects);
EXTERN INT32   GetNumberOfFonts(CONST UINT8 *Data);
EXTERN INT32   GetFontOffsetForIndex(CONST UINT8 *Data, INT32 Index);
EXTERN INT32   InitFont(FONT_INFO *Info, CONST UINT8 *Data, INT32 Offset);
EXTERN INT32   FindGlyphIndex(CONST FONT_INFO *Info, INT32 UnicodeCodepoint);
EXTERN FLOAT32 ScaleForPixelHeight(CONST FONT_INFO *Info, FLOAT32 Pixels);
EXTERN FLOAT32 ScaleForMappingEmToPixels(CONST FONT_INFO *Info, FLOAT32 Pixels);
EXTERN VOID    GetFontVMetrics(CONST FONT_INFO *Info, INT32 *Ascent, INT32 *Descent, INT32 *LineGap);
EXTERN INT32   GetFontVMetricsOS2(CONST FONT_INFO *Info, INT32 *TypoAscent, INT32 *TypoDescent, INT32 *TypoLineGap);
EXTERN VOID    GetFontBoundingBox(CONST FONT_INFO *Info, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1);
EXTERN VOID    GetCodepointHMetrics(CONST FONT_INFO *Info, INT32 Codepoint, INT32 *AdvanceWidth, INT32 *LeftSideBearing);
EXTERN INT32   GetCodepointKernAdvance(CONST FONT_INFO *Info, INT32 Ch1, INT32 Ch2);
EXTERN INT32   GetCodepointBox(CONST FONT_INFO *Info, INT32 Codepoint, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1);
EXTERN VOID    GetGlyphHMetrics(CONST FONT_INFO *Info, INT32 GlyphIndex, INT32 *AdvanceWidth, INT32 *LeftSideBearing);
EXTERN INT32   GetGlyphKernAdvance(CONST FONT_INFO *Info, INT32 Glyph1, INT32 Glyph2);
EXTERN INT32   GetGlyphBox(CONST FONT_INFO *Info, INT32 GlyphIndex, INT32 *X0, INT32 *Y0, INT32 *X1, INT32 *Y1);
EXTERN INT32   GetKerningTableLength(CONST FONT_INFO *Info);
EXTERN INT32   GetKerningTable(CONST FONT_INFO *Info, KERNING_ENTRY *Table, INT32 TableLength);
EXTERN INT32   IsGlyphEmpty(CONST FONT_INFO *Info, INT32 GlyphIndex);
EXTERN INT32   GetCodepointShape(CONST FONT_INFO *Info, INT32 UnicodeCodepoint, VERTEX **Vertices);
EXTERN INT32   GetGlyphShape(CONST FONT_INFO *Info, INT32 GlyphIndex, VERTEX **Vertices);
EXTERN VOID    FreeShape(CONST FONT_INFO *Info, VERTEX *Vertices);
EXTERN UINT8  *FindSVGDoc(CONST FONT_INFO *Info, INT32 Gl);
EXTERN INT32   GetCodepointSVG(CONST FONT_INFO *Info, INT32 UnicodeCodepoint, CONST CHAR8 **Svg);
EXTERN INT32   GetGlyphSVG(CONST FONT_INFO *Info, INT32 Gl, CONST CHAR8 **Svg);
EXTERN VOID    FreeBitmap(UINT8 *Bitmap, VOID *Userdata);
EXTERN UINT8  *GetCodepointBitmap(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Codepoint, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff);
EXTERN UINT8  *GetCodepointBitmapSubpixel(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Codepoint, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff);
EXTERN VOID    MakeCodepointBitmap(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Codepoint);
EXTERN VOID    MakeCodepointBitmapSubpixel(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Codepoint);
EXTERN VOID    MakeCodepointBitmapSubpixelPrefilter(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OversampleX, INT32 OversampleY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 Codepoint);
EXTERN VOID    GetCodepointBitmapBox(CONST FONT_INFO *Font, INT32 Codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1);
EXTERN VOID    GetCodepointBitmapBoxSubpixel(CONST FONT_INFO *Font, INT32 Codepoint, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1);
EXTERN UINT8  *GetGlyphBitmap(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Glyph, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff);
EXTERN UINT8  *GetGlyphBitmapSubpixel(CONST FONT_INFO *Info, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Glyph, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff);
EXTERN VOID    MakeGlyphBitmap(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 Glyph);
EXTERN VOID    MakeGlyphBitmapSubpixel(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 Glyph);
EXTERN VOID    MakeGlyphBitmapSubpixelPrefilter(CONST FONT_INFO *Info, UINT8 *Output, INT32 OutW, INT32 OutH, INT32 OutStride, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 OversampleX, INT32 OversampleY, FLOAT32 *SubX, FLOAT32 *SubY, INT32 Glyph);
EXTERN VOID    GetGlyphBitmapBox(CONST FONT_INFO *Font, INT32 Glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1);
EXTERN VOID    GetGlyphBitmapBoxSubpixel(CONST FONT_INFO *Font, INT32 Glyph, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 *Ix0, INT32 *Iy0, INT32 *Ix1, INT32 *Iy1);
EXTERN VOID    Rasterize(BITMAP *Result, FLOAT32 FlatnessInPixels, VERTEX *Vertices, INT32 NumVerts, FLOAT32 ScaleX, FLOAT32 ScaleY, FLOAT32 ShiftX, FLOAT32 ShiftY, INT32 XOff, INT32 YOff, INT32 Invert, VOID *Userdata);
EXTERN VOID    FreeSDF(UINT8 *Bitmap, VOID *Userdata);
EXTERN UINT8  *GetGlyphSDF(CONST FONT_INFO *Info, FLOAT32 Scale, INT32 Glyph, INT32 Padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff);
EXTERN UINT8  *GetCodepointSDF(CONST FONT_INFO *Info, FLOAT32 Scale, INT32 Codepoint, INT32 Padding, UINT8 OnedgeValue, FLOAT32 PixelDistScale, INT32 *Width, INT32 *Height, INT32 *Xoff, INT32 *Yoff);
