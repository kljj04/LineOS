// Libraries/Include/TrueType/TrueTypeEngine.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LOSLibC/TypeInfo.h>
#include <TrueType/TrueTypeRuntime.h>
#include <TrueType/TrueType.h>

typedef struct
{
    FONT_INFO Info;
    BOOLEAN   Ready;
} TRUE_TYPE_FONT;

typedef struct
{
    UINT32 *FrameBuffer;
    UINT32  FrameBufferWidth;
    UINT32  FrameBufferHeight;
} TRUE_TYPE_SURFACE;

BOOLEAN TrueTypeInit(TTF_ALLOCATOR *Allocator, TRUE_TYPE_SURFACE *Surface);
BOOLEAN TrueTypeRegisterFont(TRUE_TYPE_FONT *Font, CONST UINT8 *Data);
UINT32  DrawTrueTypeCodepoint(TRUE_TYPE_FONT *Font, UINT32 Codepoint, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight);
UINT32  DrawTrueTypeText(TRUE_TYPE_FONT *Font, CONST CHAR16 *Text, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight);
BOOLEAN MeasureTrueTypeText(TRUE_TYPE_FONT *Font, CONST CHAR16 *Text, UINT32 PixelHeight, INT32 *Left, INT32 *Top, INT32 *Right, INT32 *Bottom);
