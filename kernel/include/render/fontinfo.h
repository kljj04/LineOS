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

extern const UINT32 GlyphCount;
extern const UINT8 GlyphBitmap[];
extern const LINEOS_GLYPH GlyphDsc[];
extern const UINT16 GlyphUnicode[];