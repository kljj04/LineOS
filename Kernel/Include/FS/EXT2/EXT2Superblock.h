// Kernel/Include/FS/EXT2/EXT2Superblock.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/EXT2/EXT2Types.h>

BOOLEAN EXT2LoadSuperblock(EXT2_VOLUME *Volume);
BOOLEAN EXT2ValidateGroups(EXT2_VOLUME *Volume);
BOOLEAN EXT2WriteSuperblock(EXT2_VOLUME *Volume, CONST UINT8 *Superblock);
