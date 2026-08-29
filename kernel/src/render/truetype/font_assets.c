// kernel/src/render/truetype/font_assets.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <render/truetype/font_assets.h>

UINTN FontAssetSize(CONST UINT8 *Start, CONST UINT8 *End)
{
    return (UINTN) (End - Start);
}
