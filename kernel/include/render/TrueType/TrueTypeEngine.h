// kernel/include/render/TrueType/TrueTypeEngine.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>

typedef enum
{
    PRETENDARD,
    JETBRAINS_MONO
} TRUE_TYPE_FONT;

BOOLEAN TrueTypeInit(VOID);
UINT32  DrawTrueTypeCodepoint(TRUE_TYPE_FONT Font, UINT32 Codepoint, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight);
UINT32  DrawTrueTypeText(TRUE_TYPE_FONT Font, CONST CHAR16 *Text, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight);
BOOLEAN MeasureTrueTypeText(TRUE_TYPE_FONT Font, CONST CHAR16 *Text, UINT32 PixelHeight, INT32 *Left, INT32 *Top, INT32 *Right, INT32 *Bottom);
