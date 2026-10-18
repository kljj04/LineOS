// Kernel/Include/FS/EXT2/EXT2Bitmap.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/EXT2/EXT2Types.h>

BOOLEAN EXT2AllocateBlock(EXT2_VOLUME *Volume, UINT32 *Block);
BOOLEAN EXT2AllocateInode(EXT2_VOLUME *Volume, BOOLEAN Directory, UINT32 *Number);
BOOLEAN EXT2InodeAllocated(EXT2_VOLUME *Volume, UINT32 Number);

BOOLEAN EXT2FreeBlock(EXT2_VOLUME *Volume, UINT32 Block);
BOOLEAN EXT2FreeInode(EXT2_VOLUME *Volume, UINT32 Number, BOOLEAN Directory);
