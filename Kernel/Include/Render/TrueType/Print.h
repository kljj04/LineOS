// Kernel/Include/Render/TrueType/Print.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Render/TrueType/TrueTypeEngine.h>

#define KPRINT_TAB_SIZE         4
#define KPRINT_PIXEL_HEIGHT     24
#define KPRINT_LINE_HEIGHT      32
#define KPRINT_BACKSPACE_WIDTH  12
#define KPRINT_BACKSPACE_HEIGHT 24
#define KPRINT_MAX_WIDTH        256

VOID KPrint(CONST CHAR16 *Msg, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font, ...);
