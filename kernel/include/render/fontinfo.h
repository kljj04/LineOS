// fontinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct
{
    UINT32 BitmapOffset;
    UINT16 Width;
    UINT16 Height;
    UINT16 Advance;
    INT16  OffsetX;
    INT16  OffsetY;
} LINEOS_GLYPH;

EXTERN CONST UINT32       GlyphCount;
EXTERN CONST UINT8        GlyphBitmap[];
EXTERN CONST LINEOS_GLYPH GlyphDsc[];
EXTERN CONST UINT16       GlyphUnicode[];
