// Kernel/Include/Render/TrueType/Print.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <LineOS/TypeInfo.h>
#include <Render/TrueType/TrueTypeEngine.h>

VOID KPrint(CONST CHAR16 *Msg, UINT32 X, UINT32 Baseline, UINT32 Color, UINT32 PixelHeight, TRUE_TYPE_FONT Font, ...);
