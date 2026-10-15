// kernel/include/render/TrueType/print.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <render/TrueType/TrueTypeEngine.h>

VOID KPrint(CONST CHAR16 *msg, UINT32 x, UINT32 baseline, UINT32 color, UINT32 PixelHeight, TRUE_TYPE_FONT font, ...);
