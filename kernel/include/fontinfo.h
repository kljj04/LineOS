// fontinfo.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/bootinfo.h>

typedef struct
{
    UINT32 bitmap_offset;
    UINT16 width;
    UINT16 height;
    UINT16 advance;
    INT16 offset_x;
    INT16 offset_y;
} LINEOS_GLYPH;

extern const UINT32 glyph_count;
extern const UINT8 glyph_bitmap[];
extern const LINEOS_GLYPH glyph_dsc[];
extern const UINT16 glyph_unicode[];