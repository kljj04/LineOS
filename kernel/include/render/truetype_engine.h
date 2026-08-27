// truetype_engine.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

typedef enum
{
    TRUE_TYPE_FONT_PRETENDARD,
    TRUE_TYPE_FONT_JETBRAINS_MONO
} TRUE_TYPE_FONT;

BOOLEAN TrueTypeInit(VOID);
BOOLEAN TrueTypeSelectFont(TRUE_TYPE_FONT Font);
UINT32  DrawTrueTypeCodepoint(UINT32 Codepoint, UINT32 x, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight);
UINT32  DrawTrueTypeText(CONST CHAR16 *Text, UINT32 x, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight);
