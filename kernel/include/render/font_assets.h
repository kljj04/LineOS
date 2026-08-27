// font_assets.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <lineos/typeinfo.h>

extern CONST UINT8 LineOSJetBrainsMonoFontStart[];
extern CONST UINT8 LineOSJetBrainsMonoFontEnd[];
extern CONST UINT8 LineOSPretendardFontStart[];
extern CONST UINT8 LineOSPretendardFontEnd[];

UINTN FontAssetSize(CONST UINT8 *Start, CONST UINT8 *End);
