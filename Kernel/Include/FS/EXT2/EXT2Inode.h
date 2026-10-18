// Kernel/Include/FS/EXT2/EXT2Inode.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <FS/EXT2/EXT2Types.h>

BOOLEAN EXT2ReadInode(EXT2_VOLUME *Volume, UINT32 Number, EXT2_INODE *Inode);
BOOLEAN EXT2WriteInode(EXT2_VOLUME *Volume, UINT32 Number, CONST EXT2_INODE *Inode, BOOLEAN New);
UINT64  EXT2InodeSize(CONST EXT2_INODE *Inode);
BOOLEAN EXT2MapBlock(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Logical, BOOLEAN Allocate, UINT32 *Physical);

BOOLEAN EXT2ResizeInode(EXT2_VOLUME *Volume, EXT2_INODE *Inode, UINT64 Size);
BOOLEAN EXT2ValidateOwnership(EXT2_VOLUME *Volume);
UINT32  EXT2InodeUID(CONST EXT2_INODE *Inode);
UINT32  EXT2InodeGID(CONST EXT2_INODE *Inode);
VOID    EXT2InodeOwner(EXT2_INODE *Inode, UINT32 UID, UINT32 GID);
BOOLEAN EXT2Access(EXT2_VOLUME *Volume, CONST EXT2_INODE *Inode, UINT16 Mask);

UINT64 EXT2MaximumFileSize(EXT2_VOLUME *Volume);
