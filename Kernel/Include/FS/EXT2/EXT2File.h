// Kernel/Include/FS/EXT2/EXT2File.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/EXT2/EXT2Types.h>

BOOLEAN EXT2ReadData(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Offset, VOID *Buffer, UINT32 Length, UINT32 *ReadLength);
BOOLEAN EXT2WriteData(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Offset, CONST VOID *Buffer, UINT32 Length);
