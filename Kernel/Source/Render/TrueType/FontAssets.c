// Kernel/Source/Render/TrueType/FontAssets.c
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#include <Render/TrueType/FontAssets.h>

UINTN FontAssetSize(CONST UINT8 *Start, CONST UINT8 *End)
{
    return (UINTN) (End - Start);
}
