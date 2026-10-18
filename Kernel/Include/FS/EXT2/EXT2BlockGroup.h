// Kernel/Include/FS/EXT2/EXT2BlockGroup.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/EXT2/EXT2Types.h>

BOOLEAN EXT2ReadBlockGroup(EXT2_VOLUME *Volume, UINT32 Group, EXT2_BLOCK_GROUP *Descriptor);
BOOLEAN EXT2UpdateBlockGroup(EXT2_VOLUME *Volume, UINT32 Group, CONST EXT2_BLOCK_GROUP *Descriptor);
BOOLEAN EXT2GroupHasSuperblock(EXT2_VOLUME *Volume, UINT32 Group);
